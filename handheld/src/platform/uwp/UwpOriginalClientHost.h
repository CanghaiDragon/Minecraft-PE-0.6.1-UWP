#pragma once

#include "UwpOriginalClientBridge.h"
#include <memory>

class AppPlatform_uwp;
class UwpAngleContext;
class Level;
class LocalPlayer;
class Minecraft;
struct AppContext;

// Owns the original AppContext hand-off and a controlled NinecraftApp
// object.  Initialization is deliberately explicit so MainPage can wait for
// the packaged client textures before entering Minecraft::init.
class UwpOriginalClientHost final
{
public:
	UwpOriginalClientHost();
	~UwpOriginalClientHost();

	bool bind(AppPlatform_uwp* platform, const UwpAngleContext& angle);
	void clear();

	// Executes the original I18n bootstrap against the AppPlatform already
	// attached to AppContext.
	bool initializeLocale();
	bool initializeClient();
	void setSize(int width, int height);
	bool showStartMenu();
	bool showSelectWorld();
	bool showLegacyWorldFormatError();
	bool showProgressScreen();
	bool ensureStartMenu();
	bool wantsToQuit() const;
	UwpOriginalMenuAction consumeMenuAction();
	bool consumeWorldEdit(std::string& worldId);
	bool setSkinArmType(bool slim);
	bool applyImportedSkin();
	bool clearImportedSkin();
	bool consumeLevelSelection(std::string& levelId, std::string& levelName,
		long& seed, int& gameType, int& worldType);
	void setLoopEnabled(bool enabled);
	bool attachWorld(Level* world, LocalPlayer* localPlayer);
	bool hostMultiplayer(int port = 19132);
	void detachWorld();
	void setWorldTickEnabled(bool enabled);
	void update();
	void fixedTick();
	void render(float alpha);

	bool isBound() const { return m_bound; }
	bool hasOriginalClientObject() const { return m_client != nullptr; }
	bool isOriginalClientContextReady() const { return m_clientContextReady; }
	bool isOriginalClientInitialized() const { return m_clientInitialized; }
	bool isOriginalClientLoopEnabled() const { return m_originalLoopEnabled; }
	bool isOriginalWorldTickEnabled() const { return m_originalWorldTickEnabled; }
	Minecraft* minecraft() const;
	LevelRenderer* levelRenderer() const;
	UwpOriginalClientHandle handle() const { return m_client; }
	// Valid only while isBound() is true.  Keeping this as a reference avoids
	// exposing the original AppContext definition to XAML-generated code.
	const AppContext& context() const { return *m_context; }

private:
	std::unique_ptr<AppContext> m_context;
	UwpOriginalClientHandle m_client = nullptr;
	bool m_bound = false;
	bool m_clientContextReady = false;
	bool m_localeInitialized = false;
	bool m_clientInitialized = false;
	bool m_originalLoopEnabled = false;
	bool m_originalWorldTickEnabled = false;
};
