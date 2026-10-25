#include "UwpOriginalClientHost.h"

#include "App.h"
#include "AppPlatform_uwp.h"
#include "UwpAngleContext.h"
#include "UwpOriginalClientBridge.h"
#include "UwpStoragePaths.h"
#include "locale/I18n.h"

UwpOriginalClientHost::UwpOriginalClientHost() = default;
UwpOriginalClientHost::~UwpOriginalClientHost()
{
	clear();
}

bool UwpOriginalClientHost::bind(AppPlatform_uwp* platform, const UwpAngleContext& angle)
{
	clear();
	if (platform == nullptr || !angle.isInitialized())
		return false;

	// Match the original desktop AppContext wiring.  UWP's MainPage remains
	// the sole presentation owner; the controlled NinecraftApp update path
	// therefore skips its desktop swapBuffers call.
	m_context.reset(new AppContext{});
	m_context->display = angle.display();
	m_context->context = angle.context();
	m_context->surface = angle.surface();
	m_context->platform = platform;
	m_context->doRender = false;
	m_client = UwpOriginalClientCreate();
	if (m_client == nullptr || !UwpOriginalClientBindContext(m_client, *m_context))
	{
		clear();
		return false;
	}
	if (!UwpOriginalClientSetStoragePaths(
		m_client,
		UwpStoragePaths::localFolderPathUtf8(),
		UwpStoragePaths::localCacheFolderPathUtf8()))
	{
		clear();
		return false;
	}
	m_clientContextReady = true;
	m_bound = true;
	return true;
}

void UwpOriginalClientHost::clear()
{
	if (m_client != nullptr)
	{
		// The UWP session owns the borrowed Level/LocalPlayer.  Detach the
		// original Minecraft pointers before destroying the client object.
		UwpOriginalClientDetachWorld(m_client);
		UwpOriginalClientDestroy(m_client);
		m_client = nullptr;
	}
	m_context.reset();
	m_bound = false;
	m_clientContextReady = false;
	m_localeInitialized = false;
	m_clientInitialized = false;
	m_originalLoopEnabled = false;
	m_originalWorldTickEnabled = false;
}

bool UwpOriginalClientHost::initializeLocale()
{
	if (!m_bound || m_context == nullptr || m_context->platform == nullptr)
		return false;

	// This is the same original-client call made by NinecraftApp::init().
	// It is safe before Minecraft::init(): it needs only AppPlatform's
	// synchronous packaged asset contract, already verified by Stage 1.
	I18n::loadLanguage(m_context->platform, "en_US");
	m_localeInitialized = I18n::get("menu.singleplayer") == "Singleplayer";
	return m_localeInitialized;
}

bool UwpOriginalClientHost::initializeClient()
{
	if (!m_bound || !m_clientContextReady || m_context == nullptr ||
		m_client == nullptr)
		return false;
	if (m_clientInitialized)
		return true;

	if (!UwpOriginalClientInitialize(m_client, *m_context))
		return false;
	m_clientInitialized = true;
	m_originalLoopEnabled = true;
	return true;
}

void UwpOriginalClientHost::setSize(int width, int height)
{
	if (!m_clientInitialized || m_client == nullptr)
		return;
	UwpOriginalClientSetSize(m_client, width, height);
}

bool UwpOriginalClientHost::showStartMenu()
{
	return m_clientInitialized && m_client != nullptr &&
		UwpOriginalClientShowStartMenu(m_client);
}

bool UwpOriginalClientHost::showSelectWorld()
{
	return m_clientInitialized && m_client != nullptr &&
		UwpOriginalClientShowSelectWorld(m_client);
}

bool UwpOriginalClientHost::showLegacyWorldFormatError()
{
	return m_clientInitialized && m_client != nullptr &&
		UwpOriginalClientShowLegacyWorldFormatError(m_client);
}

bool UwpOriginalClientHost::showProgressScreen()
{
	return m_clientInitialized && m_client != nullptr &&
		UwpOriginalClientShowProgress(m_client);
}

bool UwpOriginalClientHost::ensureStartMenu()
{
	return m_clientInitialized && m_client != nullptr &&
		UwpOriginalClientEnsureStartMenu(m_client);
}

bool UwpOriginalClientHost::wantsToQuit() const
{
	return m_client != nullptr && UwpOriginalClientWantsToQuit(m_client);
}

UwpOriginalMenuAction UwpOriginalClientHost::consumeMenuAction()
{
	return m_client != nullptr
		? UwpOriginalClientConsumeMenuAction(m_client)
		: UwpOriginalMenuAction::None;
}

bool UwpOriginalClientHost::consumeWorldEdit(std::string& worldId)
{
	return m_client != nullptr && UwpOriginalClientConsumeWorldEdit(
		m_client, worldId);
}

bool UwpOriginalClientHost::setSkinArmType(bool slim)
{
	return m_clientInitialized && m_client != nullptr &&
		UwpOriginalClientSetSkinArmType(m_client, slim);
}

bool UwpOriginalClientHost::applyImportedSkin()
{
	return m_clientInitialized && m_client != nullptr &&
		UwpOriginalClientApplyImportedSkin(m_client);
}

bool UwpOriginalClientHost::clearImportedSkin()
{
	return m_clientInitialized && m_client != nullptr &&
		UwpOriginalClientClearImportedSkin(m_client);
}

bool UwpOriginalClientHost::consumeLevelSelection(
	std::string& levelId, std::string& levelName, long& seed,
	int& gameType, int& worldType)
{
	return m_client != nullptr && UwpOriginalClientConsumeLevelSelection(
		m_client, levelId, levelName, seed, gameType, worldType);
}

void UwpOriginalClientHost::setLoopEnabled(bool enabled)
{
	m_originalLoopEnabled = m_clientInitialized && enabled;
}

Minecraft* UwpOriginalClientHost::minecraft() const
{
	return m_client != nullptr ? UwpOriginalClientGetMinecraft(m_client) : nullptr;
}

LevelRenderer* UwpOriginalClientHost::levelRenderer() const
{
	return m_client != nullptr ? UwpOriginalClientGetLevelRenderer(m_client) : nullptr;
}

bool UwpOriginalClientHost::attachWorld(Level* world, LocalPlayer* localPlayer)
{
	if (!m_clientInitialized || m_client == nullptr || world == nullptr ||
		localPlayer == nullptr)
		return false;
	if (m_originalWorldTickEnabled)
		return true;
	return UwpOriginalClientAttachWorld(m_client, world, localPlayer);
}

bool UwpOriginalClientHost::hostMultiplayer(int port)
{
	return m_client != nullptr && UwpOriginalClientHostMultiplayer(m_client, port);
}

void UwpOriginalClientHost::detachWorld()
{
	m_originalWorldTickEnabled = false;
	if (m_client != nullptr)
		UwpOriginalClientDetachWorld(m_client);
}

void UwpOriginalClientHost::setWorldTickEnabled(bool enabled)
{
	m_originalWorldTickEnabled = m_clientInitialized && enabled;
}

void UwpOriginalClientHost::update()
{
	if ((m_originalLoopEnabled || m_originalWorldTickEnabled) && m_client != nullptr)
		UwpOriginalClientUpdate(m_client);
}

void UwpOriginalClientHost::fixedTick()
{
	if (m_originalWorldTickEnabled && m_client != nullptr)
		UwpOriginalClientFixedTick(m_client);
}

void UwpOriginalClientHost::render(float alpha)
{
	if ((m_originalLoopEnabled || m_originalWorldTickEnabled) && m_client != nullptr)
		UwpOriginalClientRender(m_client, alpha);
}
