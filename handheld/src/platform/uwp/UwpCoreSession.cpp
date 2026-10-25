#include "UwpCoreSession.h"

#include "../../SharedConstants.h"
#include "../../server/CreatorLevel.h"
#include "../../world/entity/MobCategory.h"
#include "../../world/item/Item.h"
#include "../../world/level/biome/Biome.h"
#include "../../world/level/material/Material.h"
#include "../../world/level/storage/ExternalFileLevelStorageSource.h"
#include "../../world/level/chunk/ChunkSource.h"
#include "../../world/level/LevelSettings.h"
#include "../../world/level/tile/Tile.h"
#include "../../world/level/tile/entity/TileEntity.h"
#include "../../world/entity/player/Player.h"
#include "../../client/player/LocalPlayer.h"
#include "../../client/Minecraft.h"
#include "../../util/Mth.h"
#include "../../platform/log.h"
#include "UwpStoragePaths.h"

#include <algorithm>
#include <exception>
#include <chrono>
#include <stdexcept>

namespace
{
	// The original storage code uses narrow stdio paths.  UWP's application
	// data root is expected to be ASCII on the supported deployment layout;
	// reject an incompatible root instead of changing bytes in an original
	// save-game path.
	bool narrowPath(const std::wstring& source, std::string& destination)
	{
		destination.clear();
		destination.reserve(source.size());
		for (const wchar_t character : source)
		{
			if (character > 0x7f)
				return false;
			destination.push_back(static_cast<char>(character));
		}
		return true;
	}

}

// UWP owns the simulation through the real MCPE level classes.  CreatorLevel
// is used instead of Minecraft because Minecraft.cpp also owns the legacy
// desktop GUI, WGL renderer and Win32 dialogs.
class UwpMinecraftCore final
{
public:
	UwpMinecraftCore() = default;

	~UwpMinecraftCore()
	{
		// After Level::loadPlayer(..., true), the original Level owns the
		// player through its entities/players collections.
		delete m_level;
		m_level = nullptr;
		m_player = nullptr;
		delete m_levelStorage;
		m_levelStorage = nullptr;
		delete m_storageSource;
		m_storageSource = nullptr;
	}

	bool initialize()
	{
		if (m_storageSource != nullptr)
			return true;

		// ExternalFileLevelStorageSource appends the original
		// /games/com.mojang/minecraftWorlds suffix itself.  Pass the two
		// application-data roots here, rather than the already-expanded world
		// folder, so the UWP session and original SelectWorld use one layout.
		const std::wstring worldsPath = UwpStoragePaths::localFolderPath()->Data();
		const std::wstring temporaryWorldsPath =
			UwpStoragePaths::localCacheFolderPath()->Data();
		if (!narrowPath(worldsPath, m_worldsPath) ||
			!narrowPath(temporaryWorldsPath, m_temporaryWorldsPath))
			return false;

		m_storageSource = new ExternalFileLevelStorageSource(
			m_worldsPath, m_temporaryWorldsPath);
		return m_storageSource != nullptr;
	}

	void bindOriginalClient(Minecraft* client)
	{
		m_originalClient = client;
	}

	void createWorld(const std::string& levelId, const std::string& name,
		const LevelSettings& settings)
	{
		// The active player belongs to Level after loadPlayer(..., true), so
		// dispose the original owner before replacing the storage instance.
		delete m_level;
		m_level = nullptr;
		m_player = nullptr;
		delete m_levelStorage;
		m_levelStorage = nullptr;
		m_worldGenerated = false;
		m_jumpHeld = false;
		m_jumpRequested = false;
		m_maxLevelTickMilliseconds = 0.0;
		m_maxPlayerTickMilliseconds = 0.0;

		if (m_storageSource == nullptr)
			throw std::runtime_error("UWP world storage path is unavailable");

		// The original storage source owns the id-to-directory selection.  Its
		// selected storage reads level.dat, which CreatorLevel consumes through
		// the normal LevelStorage::prepareLevel() call.
		m_levelStorage = m_storageSource->selectLevel(levelId, false);
		m_level = new CreatorLevel(
			m_levelStorage,
			name,
			settings,
			SharedConstants::GeneratorVersion);
		// Match the original single-player CreatorLevel semantics.  A local
		// world is authoritative, so the Level must remain server-side: the
		// native MobSpawner, item drops/projectiles, block callbacks and entity
		// persistence all use this flag to decide whether their simulation is
		// allowed to run.  The UWP host does not provide a remote client
		// callback, so LocalPlayer does not emit online movement packets here.
		m_level->isClientSide = false;

		// Force the first terrain chunk through the genuine MCPE generator before
		// the UI enters the running state.  The original desktop client then
		// performs its full prepareLevel preload before exposing the world; use
		// that same routine for this externally-owned Level instead of letting the
		// renderer generate the visible ring on demand.
		m_level->getChunk(0, 0);
		m_level->setInitialSpawn();
		if (m_originalClient != nullptr)
			m_originalClient->prepareLevelFor(m_level, "Generating level");
		else if (!m_level->isInfinite())
			// The fallback core has no Minecraft::prepareLevelFor() owner, so
			// preserve the finite-world legacy entity load here as well.
			m_level->loadEntities();
		// Use the original player-load path.  It restores an existing Player NBT
		// tag from level.dat, or creates a spawn-position player for a new world,
		// then registers that player in Level::players for original save logic.
		if (m_originalClient != nullptr)
		{
			// Minecraft::tick() requires the original LocalPlayer input/aiStep
			// chain.  The level still owns this object after loadPlayer(..., true).
			// Construct the player with the level's actual game type so Inventory's
			// original setupDefault() populates the complete creative catalogue.
			// Passing false here leaves a creative world with a survival inventory;
			// Minecraft::setIsCreativeMode() later changes the game mode but does
			// not rebuild the already-created player inventory.
			const bool creativeWorld =
				m_level->getLevelData()->getGameType() == GameType::Creative;
			m_player = new LocalPlayer(
				m_originalClient,
				m_level,
				m_originalClient->user,
				m_level->dimension->id,
				creativeWorld);
		}
		else
		{
			m_player = new Player(m_level,
				m_level->getLevelData()->getGameType() == GameType::Creative);
		}
		m_level->loadPlayer(m_player, true);
		m_worldGenerated = true;
	}

	void setMoveInput(float x, float z)
	{
		m_moveX = x;
		m_moveZ = z;
	}

	void setJumpInput(bool pressed)
	{
		// Convert a held key into a single jump request. Player::aiStep() in
		// this stripped UWP seam does not run LocalPlayer's input path, so the
		// request is applied directly before Entity::move().
		if (pressed && !m_jumpHeld)
			m_jumpRequested = true;
		m_jumpHeld = pressed;
	}

	void step()
	{
		if (m_level == nullptr)
			return;

		// Keep the fallback UWP core consistent with the original-client path:
		// instantiate deferred entity tags in bounded batches.
		if (m_levelStorage != nullptr && m_level->isInfinite())
			m_levelStorage->loadDeferredEntities(m_level, 16);

		const auto levelStart = std::chrono::steady_clock::now();
		m_level->tick();
		const auto levelEnd = std::chrono::steady_clock::now();
		const double levelMilliseconds =
			std::chrono::duration<double, std::milli>(levelEnd - levelStart).count();
		if (levelMilliseconds > m_maxLevelTickMilliseconds)
			m_maxLevelTickMilliseconds = levelMilliseconds;
		if (m_player == nullptr)
			return;

		// Player::tick() keeps the normal entity bookkeeping and player state.
		// The small input adapter below supplies horizontal intent until the
		// legacy LocalPlayer/Minecraft input path is made UWP-safe.
		const auto playerStart = std::chrono::steady_clock::now();
		m_player->tick();
		const auto playerEnd = std::chrono::steady_clock::now();
		const double playerMilliseconds =
			std::chrono::duration<double, std::milli>(playerEnd - playerStart).count();
		if (playerMilliseconds > m_maxPlayerTickMilliseconds)
			m_maxPlayerTickMilliseconds = playerMilliseconds;
		if (m_jumpRequested && m_player->onGround)
			m_player->yd = 0.42f;
		m_jumpRequested = false;
		const float speed = m_player->onGround ? 0.11f : 0.065f;
		const float dx = m_moveX * speed;
		const float dz = m_moveZ * speed;
		m_player->move(dx, m_player->yd, dz);
		m_player->xd = dx;
		m_player->zd = dz;
		if (m_player->onGround)
			m_player->yd = 0.0f;
		else
		{
			m_player->yd -= 0.08f;
			m_player->yd *= 0.98f;
		}
	}

	bool worldGenerated() const
	{
		return m_worldGenerated;
	}

	long worldTime() const
	{
		return m_level != nullptr ? m_level->getTime() : 0;
	}

	Level* level() const
	{
		return m_level;
	}

	Player* player() const
	{
		return m_player;
	}

	LocalPlayer* localPlayer() const
	{
		return dynamic_cast<LocalPlayer*>(m_player);
	}

	double maxLevelTickMilliseconds() const { return m_maxLevelTickMilliseconds; }
	double maxPlayerTickMilliseconds() const { return m_maxPlayerTickMilliseconds; }

	void saveGame()
	{
		if (m_level == nullptr)
			return;

		// Level::saveGame() writes level/player data only.  The original client
		// shutdown path flushes the chunk source separately; do the same before
		// UWP destroys the Level and its storage.  ChunkCache::saveAll() also
		// drains resident chunks that have finished base terrain but are still
		// waiting for postProcess (trees, leaves, snow, ores, and plants).
		ChunkSource* chunkSource = m_level->getChunkSource();
		if (chunkSource != nullptr) {
			LOGI("UWP save begin: flushing chunk population and dirty chunks\n");
			chunkSource->saveAll(true);
		}
		m_level->saveGame();
		LOGI("UWP save end: level/player data written\n");
	}

private:
	std::string m_worldsPath;
	std::string m_temporaryWorldsPath;
	ExternalFileLevelStorageSource* m_storageSource = nullptr;
	LevelStorage* m_levelStorage = nullptr;
	CreatorLevel* m_level = nullptr;
	Player* m_player = nullptr;
	float m_moveX = 0.0f;
	float m_moveZ = 0.0f;
	bool m_jumpHeld = false;
	bool m_jumpRequested = false;
	bool m_worldGenerated = false;
	Minecraft* m_originalClient = nullptr;
	double m_maxLevelTickMilliseconds = 0.0;
	double m_maxPlayerTickMilliseconds = 0.0;
};

UwpCoreSession::UwpCoreSession() = default;

UwpCoreSession::~UwpCoreSession()
{
	// Keep application termination safe even if the page did not reach its
	// normal leave-world path (for example, process teardown after suspension).
	if (m_nativeCore != nullptr || m_state != State::Idle)
		stop();
	if (m_ownsStaticRegistries)
	{
		TileEntity::teardownTileEntities();
		Item::teardownItems();
		Tile::teardownTiles();
		Material::teardownMaterials();
		Biome::teardownBiomes();
		m_ownsStaticRegistries = false;
	}
}

Level* UwpCoreSession::nativeLevel() const
{
	return m_nativeWorldGenerated && m_nativeCore != nullptr
		? m_nativeCore->level() : nullptr;
}

LocalPlayer* UwpCoreSession::nativeLocalPlayer() const
{
	return m_nativeWorldGenerated && m_nativeCore != nullptr
		? m_nativeCore->localPlayer() : nullptr;
}

bool UwpCoreSession::bindOriginalClient(Minecraft* client)
{
	m_originalClient = client;
	if (m_nativeCore != nullptr)
		m_nativeCore->bindOriginalClient(client);
	return client != nullptr;
}

void UwpCoreSession::syncOriginalTick(std::uint64_t tickNumber)
{
	if (m_state != State::Running || m_nativeCore == nullptr)
		return;

	m_tickCount = tickNumber;
	m_nativeWorldGenerated = m_nativeCore->worldGenerated();
	m_dayTime = static_cast<float>(m_nativeCore->worldTime() % 24000) / 24000.0f;
	if (m_nativeCore->player() != nullptr)
	{
		m_playerX = m_nativeCore->player()->x;
		m_playerY = m_nativeCore->player()->y;
		m_playerZ = m_nativeCore->player()->z;
	}
}

double UwpCoreSession::maxLevelTickMilliseconds() const
{
	return m_nativeCore != nullptr ? m_nativeCore->maxLevelTickMilliseconds() : 0.0;
}

double UwpCoreSession::maxPlayerTickMilliseconds() const
{
	return m_nativeCore != nullptr ? m_nativeCore->maxPlayerTickMilliseconds() : 0.0;
}

bool UwpCoreSession::initializeNativeCore()
{
	if (m_nativeCoreReady)
		return true;

	try
	{
		Mth::initMth();
		const bool registriesReady = Material::air != nullptr &&
			Tile::tiles[1] != nullptr && Item::items[1] != nullptr &&
			Biome::plains != nullptr;
		if (!registriesReady)
		{
			Material::initMaterials();
			MobCategory::initMobCategories();
			Tile::initTiles();
			Item::initItems();
			Biome::initBiomes();
			TileEntity::initTileEntities();
			m_ownsStaticRegistries = true;
		}
		else
		{
			m_ownsStaticRegistries = false;
		}

		if (m_nativeCore == nullptr)
			m_nativeCore = std::make_unique<UwpMinecraftCore>();
		m_nativeCore->bindOriginalClient(m_originalClient);
		m_nativeCoreReady = m_nativeCore->initialize();
		if (!m_nativeCoreReady)
		{
			m_errorText = L"MCPE native core initialization returned incomplete state";
			m_nativeCore.reset();
		}
		return m_nativeCoreReady;
	}
	catch (const std::exception& error)
	{
		LOGE("UWP native core initialization failed: %s\n", error.what());
		m_errorText = L"MCPE native core initialization threw an exception";
		m_nativeCore.reset();
		m_nativeCoreReady = false;
		return false;
	}
	catch (...)
	{
		m_errorText = L"MCPE native core initialization failed";
		m_nativeCore.reset();
		m_nativeCoreReady = false;
		return false;
	}
}

void UwpCoreSession::beginWorld(const std::wstring& worldId,
	const std::wstring& worldName, long seed, int gameType, int worldType)
{
	m_worldName = worldName.empty() ? L"New World" : worldName;
	m_tickCount = 0;
	m_playerX = 0.0f;
	m_playerY = 0.0f;
	m_playerZ = 0.0f;
	m_moveX = 0.0f;
	m_moveZ = 0.0f;
	m_jumpInput = false;
	m_dayTime = 0.0f;
	m_nativeWorldGenerated = false;
	m_legacyStorageRejected = false;
	m_errorText.clear();

	if (!initializeNativeCore())
	{
		m_state = State::Idle;
		return;
	}

	try
	{
		m_nativeCore->createWorld(
			std::string(worldId.begin(), worldId.end()),
			std::string(m_worldName.begin(), m_worldName.end()),
			LevelSettings(seed, gameType, worldType));
		if (m_nativeCore->player() != nullptr)
		{
			m_playerX = m_nativeCore->player()->x;
			m_playerY = m_nativeCore->player()->y;
			m_playerZ = m_nativeCore->player()->z;
		}
		m_state = State::Running;
		m_nativeWorldGenerated = m_nativeCore->worldGenerated();
	}
	catch (const std::exception& error)
	{
		LOGE("UWP native world creation failed: %s\n", error.what());
		if (std::string(error.what()) == "MCPEUWP_LEGACY_INFINITE_STORAGE")
		{
			m_errorText = L"Legacy Infinite/Sky save detected; convert this world before opening";
			m_legacyStorageRejected = true;
		}
		else
		{
			m_errorText = L"MCPE native world creation failed";
			m_legacyStorageRejected = false;
		}
		m_state = State::Idle;
	}
	catch (...)
	{
		m_errorText = L"MCPE native world creation failed";
		m_legacyStorageRejected = false;
		m_state = State::Idle;
	}
}

void UwpCoreSession::startSavedWorld(const std::wstring& worldId, const std::wstring& worldName)
{
	// The original external storage source selects this id; CreatorLevel then
	// reloads the corresponding level.dat through its normal construction path.
	beginWorld(worldId, worldName, -1, GameType::Undefined, WorldType::Old);
}

void UwpCoreSession::startNewWorld(const std::wstring& worldId,
	const std::wstring& worldName, long seed, int gameType, int worldType)
{
	beginWorld(worldId, worldName, seed, gameType, worldType);
}

void UwpCoreSession::saveGame()
{
	if (m_nativeCore != nullptr)
		m_nativeCore->saveGame();
}

void UwpCoreSession::stop()
{
	bool saveFailed = false;
	// Match the original shutdown save path before closing the original level
	// and its external storage.  This is intentionally not a custom serializer.
	if (m_nativeCore != nullptr)
	{
		try
		{
			m_nativeCore->saveGame();
		}
		catch (...)
		{
			saveFailed = true;
		}
		m_nativeCore.reset();
		m_nativeCoreReady = false;
	}
	if (m_ownsStaticRegistries)
	{
		TileEntity::teardownTileEntities();
		Item::teardownItems();
		Tile::teardownTiles();
		Material::teardownMaterials();
		Biome::teardownBiomes();
		m_ownsStaticRegistries = false;
	}

	m_state = State::Idle;
	m_nativeWorldGenerated = false;
	m_worldName.clear();
	m_tickCount = 0;
	m_playerX = 0.0f;
	m_playerY = 0.0f;
	m_playerZ = 0.0f;
	m_moveX = 0.0f;
	m_moveZ = 0.0f;
	m_jumpInput = false;
	m_dayTime = 0.0f;
	m_errorText.clear();
	if (saveFailed)
		m_errorText = L"MCPE native world save failed";
}

void UwpCoreSession::setMoveInput(float x, float z)
{
	// Keep the test seam bounded so a held key cannot move the camera outside
	// the deliberately small first terrain slice.
	m_moveX = x < -1.0f ? -1.0f : (x > 1.0f ? 1.0f : x);
	m_moveZ = z < -1.0f ? -1.0f : (z > 1.0f ? 1.0f : z);
}

void UwpCoreSession::setJumpInput(bool pressed)
{
	m_jumpInput = pressed;
}

void UwpCoreSession::togglePaused()
{
	if (m_state == State::Running)
		m_state = State::Paused;
	else if (m_state == State::Paused)
		m_state = State::Running;
}

void UwpCoreSession::tick(std::uint64_t tickNumber)
{
	if (m_state != State::Running || !m_nativeCore)
		return;
	// Once the original client has borrowed the world, its Minecraft::tick()
	// path is the sole simulation owner.  This method remains the fallback
	// path for worlds created before the client hand-off or when the original
	// client is unavailable.
	if (m_originalClient != nullptr && m_nativeCore->localPlayer() != nullptr)
		return;

	try
	{
		m_nativeCore->setMoveInput(m_moveX, m_moveZ);
		m_nativeCore->setJumpInput(m_jumpInput);
		m_nativeCore->step();
		m_tickCount = tickNumber;
		m_nativeWorldGenerated = m_nativeCore->worldGenerated();
		m_dayTime = static_cast<float>(m_nativeCore->worldTime() % 24000) / 24000.0f;
		if (m_nativeCore->player() != nullptr)
		{
			m_playerX = m_nativeCore->player()->x;
			m_playerY = m_nativeCore->player()->y;
			m_playerZ = m_nativeCore->player()->z;
		}
	}
	catch (const std::exception& error)
	{
		LOGE("UWP native MCPE tick failed: %s\n", error.what());
		m_errorText = L"MCPE native tick failed";
		m_state = State::Paused;
	}
	catch (...)
	{
		m_errorText = L"MCPE native tick failed";
		m_state = State::Paused;
	}
}
