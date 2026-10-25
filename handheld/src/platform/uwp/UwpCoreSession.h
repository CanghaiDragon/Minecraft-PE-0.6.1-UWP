#pragma once

#include <cstdint>
#include <memory>
#include <string>

class UwpMinecraftCore;
class Level;
class LocalPlayer;
class Minecraft;

// UWP-side lifetime and simulation seam for the MCPE core.  The session owns
// the native CreatorLevel world and advances it from the UWP frame scheduler;
// rendering remains a separate ANGLE-facing layer until the legacy renderer
// is made GLES2-safe.
class UwpCoreSession final
{
public:
	enum class State
	{
		Idle,
		Loading,
		Running,
		Paused
	};

	UwpCoreSession();
	~UwpCoreSession();
	UwpCoreSession(const UwpCoreSession&) = delete;
	UwpCoreSession& operator=(const UwpCoreSession&) = delete;

	void startSavedWorld(const std::wstring& worldId, const std::wstring& worldName);
	void startNewWorld(const std::wstring& worldId, const std::wstring& worldName,
		long seed, int gameType, int worldType);
	// Flush the active original Level without leaving the world.  Used by the
	// UWP suspension path; stop() remains the owner of final destruction.
	void saveGame();
	void stop();
	void togglePaused();
	// Temporary UWP input seam.  The values are consumed by the existing core
	// tick until the legacy Player input path is made platform-neutral.
	void setMoveInput(float x, float z);
	void setJumpInput(bool pressed);
	void tick(std::uint64_t tickNumber);
	// Bind the already initialized original client so newly created UWP worlds
	// use LocalPlayer and can later be lent to Minecraft::tick().
	bool bindOriginalClient(Minecraft* client);
	void syncOriginalTick(std::uint64_t tickNumber);

	State state() const { return m_state; }
	bool isActive() const { return m_state == State::Running || m_state == State::Paused; }
	const std::wstring& worldName() const { return m_worldName; }
	std::uint64_t tickCount() const { return m_tickCount; }
	float playerX() const { return m_playerX; }
	float playerY() const { return m_playerY; }
	float playerZ() const { return m_playerZ; }
	float dayTime() const { return m_dayTime; }
	// Maximum CPU durations for the active world.  They are diagnostic-only:
	// no scheduling or simulation behavior depends on them.
	double maxLevelTickMilliseconds() const;
	double maxPlayerTickMilliseconds() const;
	bool nativeCoreReady() const { return m_nativeCoreReady; }
	bool nativeWorldGenerated() const { return m_nativeWorldGenerated; }
	// The render bridge only borrows this pointer.  UwpCoreSession remains the
	// owner and invalidates it when stop() is called or a new world is created.
	Level* nativeLevel() const;
	LocalPlayer* nativeLocalPlayer() const;
	const std::wstring& errorText() const { return m_errorText; }
	bool legacyStorageRejected() const { return m_legacyStorageRejected; }

private:
	bool initializeNativeCore();
	void beginWorld(const std::wstring& worldId, const std::wstring& worldName,
		long seed, int gameType, int worldType);

	State m_state = State::Idle;
	std::wstring m_worldName;
	std::uint64_t m_tickCount = 0;
	float m_playerX = 0.0f;
	float m_playerY = 0.0f;
	float m_playerZ = 0.0f;
	float m_moveX = 0.0f;
	float m_moveZ = 0.0f;
	bool m_jumpInput = false;
	float m_dayTime = 0.0f;
	bool m_nativeCoreReady = false;
	bool m_nativeWorldGenerated = false;
	bool m_legacyStorageRejected = false;
	// The original client may have initialized the global registries first.
	// Only tear them down when this session performed the initialization.
	bool m_ownsStaticRegistries = false;
	std::wstring m_errorText;
	std::unique_ptr<UwpMinecraftCore> m_nativeCore;
	Minecraft* m_originalClient = nullptr;
};
