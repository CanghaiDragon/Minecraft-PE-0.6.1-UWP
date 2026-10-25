#include "UwpOriginalClientBridge.h"

#include "../../AppPlatform.h"
#include "../../NinecraftApp.h"
#include "../../client/Minecraft.h"
#include "../../client/player/LocalPlayer.h"
#include "../../client/renderer/Textures.h"
#include "../../client/renderer/LevelRenderer.h"
#include "../../network/RakNetInstance.h"
#include "../../network/ServerSideNetworkHandler.h"
#include "../../client/gui/screens/StartMenuScreen.h"
#include "../../client/gui/screens/ProgressScreen.h"
#include "../../client/gui/screens/OptionsScreen.h"
#include "../../client/gui/Screen.h"
#include "../../client/gui/components/Button.h"
#include "../../client/gui/components/ImageButton.h"
#include "../../client/gui/screens/touch/TouchStartMenuScreen.h"
#include "../../platform/input/Keyboard.h"
#include "../../world/level/LevelSettings.h"

namespace
{
	UwpOriginalMenuAction g_pendingMenuAction = UwpOriginalMenuAction::None;
	std::string g_pendingLevelId;
	std::string g_pendingLevelName;
	std::string g_pendingWorldEditId;
	long g_pendingLevelSeed = -1;
	int g_pendingGameType = GameType::Undefined;
	int g_pendingWorldType = WorldType::Old;

	class UwpStartMenuScreen final : public StartMenuScreen
	{
	public:
		UwpStartMenuScreen()
			: StartMenuScreen(), skinButton(8, "")
		{
			ImageDef def;
			def.name = "gui/gui.png";
			// Keep the icon square and just below the original Quit Game button's
			// 22-pixel GUI height.  The complete 20x20 artwork rectangle includes
			// the black frame on all four sides.
			def.width = 20;
			def.height = 20;
			// The keyboard/mouse skin icons are the two square states on the
			// right edge of gui.png: normal first, hover second.  They are stacked
			// directly with no gap.
			def.setSrc(IntRectangle(236, 66, 20, 20));
			def.setSecondImageVertical(true);
			def.setSecondImageStep(0, 20);
			skinButton.setImageDef(def, true);
			skinButton.scaleWhenPressed = false;
			skinButton.active = true;
		}

		void init() override
		{
			StartMenuScreen::init();
			// The Additional settings toggle controls whether this optional
			// entry is shown.  It no longer controls a first-level Options tab.
			if (minecraft != nullptr && minecraft->options.skinMenu)
			{
				buttons.push_back(&skinButton);
				tabButtons.push_back(&skinButton);
			}
		}

		void setupPositions() override
		{
			StartMenuScreen::setupPositions();
			// Keep the skin entry immediately to the right of the original
			// Quit button, aligned to its vertical center.  Deriving this from
			// the original button preserves the relationship when the viewport
			// is resized instead of pinning the icon to the screen corner.
			Button& quitButton = getQuitButtonForPlatform();
			// Keep the destination square just two GUI pixels smaller than the
			// original button height so it remains slightly inset and centered if
			// the desktop layout is retuned.
			const int skinButtonSize = quitButton.height > 2 ? quitButton.height - 2 : 1;
			skinButton.width = skinButtonSize;
			skinButton.height = skinButtonSize;
			skinButton.x = quitButton.x + quitButton.width + 4;
			skinButton.y = quitButton.y + (quitButton.height - skinButton.height) / 2;
		}

		void buttonClicked(Button* button) override
		{
			if (button != nullptr && button->id == skinButton.id)
			{
				// The skin button is owned by the original menu, but its editor is
				// a native UWP XAML surface.  Leave the original screen in place
				// and let MainPage display the overlay on the next game tick.
				g_pendingMenuAction = UwpOriginalMenuAction::OpenSkin;
				return;
			}
			if (button != nullptr && button->id == 2)
			{
				// The original SelectWorldScreen remains the next screen; the
				// UWP session still owns the asynchronous world load itself.
				g_pendingMenuAction = UwpOriginalMenuAction::Singleplayer;
				return;
			}
			StartMenuScreen::buttonClicked(button);
		}

	private:
		ImageButton skinButton;
	};

	// The UWP bridge adds platform-specific handling to the Java start menu
	// (skin entry and the asynchronous Singleplayer handoff). Keep the same
	// handoff for the PE start menu so switching UI styles does not change
	// world-launch behaviour.
	class UwpTouchStartMenuScreen final : public Touch::StartMenuScreen
	{
	public:
		UwpTouchStartMenuScreen()
			: Touch::StartMenuScreen(), skinButton(8, "")
		{
			ImageDef def;
			def.name = "gui/touchgui.png";
			// touchgui.png stores the PE hanger buttons in the upper-right.
			// Each state is a 32x24 cell at x=223; there are two blank rows
			// between the normal cell (y=1) and the hovered cell (y=27).
			def.width = 32;
			def.height = 24;
			def.setSrc(IntRectangle(223, 1, 32, 24));
			def.setSecondImageVertical(true);
			def.setSecondImageStep(0, 26);
			skinButton.setImageDef(def, true);
			skinButton.scaleWhenPressed = false;
			skinButton.active = true;
		}

		void init() override
		{
			Touch::StartMenuScreen::init();
			if (minecraft != nullptr && minecraft->options.skinMenu)
			{
				buttons.push_back(&skinButton);
				tabButtons.push_back(&skinButton);
			}
		}

		void setupPositions() override
		{
			Touch::StartMenuScreen::setupPositions();
			Button& quitButton = getQuitButtonForPlatform();
			// Place the native PE hanger directly to the left of the top-right X.
			skinButton.x = quitButton.x - skinButton.width;
			skinButton.y = quitButton.y + (quitButton.height - skinButton.height) / 2;
		}

		void buttonClicked(Button* button) override
		{
			if (button != nullptr && button->id == skinButton.id)
			{
				g_pendingMenuAction = UwpOriginalMenuAction::OpenSkin;
				return;
			}
			if (button != nullptr && button->id == 2)
			{
				g_pendingMenuAction = UwpOriginalMenuAction::Singleplayer;
				return;
			}
			Touch::StartMenuScreen::buttonClicked(button);
		}

	private:
		ImageButton skinButton;
	};

	class UwpLegacyWorldFormatScreen final : public Screen
	{
		typedef Screen super;

	public:
		UwpLegacyWorldFormatScreen()
			: backButton(nullptr)
		{
		}

		~UwpLegacyWorldFormatScreen() override
		{
			delete backButton;
		}

		void init() override
		{
			// Match the selected UI presentation rather than the input device.
			// PE UI uses touchgui.png; Java UI uses gui.png.
			if (minecraft->options.javaUi)
				backButton = new Button(1, "Back");
			else
				backButton = new Touch::TButton(1, "Back");

			backButton->width = 128;
			backButton->height = 24;
			buttons.push_back(backButton);
			tabButtons.push_back(backButton);
		}

		void setupPositions() override
		{
			backButton->x = (width - backButton->width) / 2;
			backButton->y = height / 2 + 24;
		}

		void render(int xm, int ym, float a) override
		{
			// This screen is shown before a Level is attached.  Calling the
			// explicit dirt renderer keeps the native MC background even if a
			// previously opened world has left transient renderer state behind.
			renderDirtBackground(0);
			drawCenteredString(font,
				"This world is in an unsupported legacy format.",
				width / 2, height / 2 - 42, 0xffffff);
			drawCenteredString(font,
				"Use Windows Universal v1.0 to open it.",
				width / 2, height / 2 - 26, 0xffffff);
			super::render(xm, ym, a);
		}

		void buttonClicked(Button* button) override
		{
			if (button == backButton)
				minecraft->screenChooser.setScreen(SCREEN_SELECTWORLD);
		}

		bool handleBackEvent(bool isDown) override
		{
			if (!isDown)
				minecraft->screenChooser.setScreen(SCREEN_SELECTWORLD);
			return true;
		}

		bool isInGameScreen() override
		{
			return false;
		}

	protected:
		void keyPressed(int eventKey) override
		{
			if (eventKey == Keyboard::KEY_ESCAPE)
			{
				minecraft->screenChooser.setScreen(SCREEN_SELECTWORLD);
				return;
			}
			super::keyPressed(eventKey);
		}

	private:
		Button* backButton;
	};
}

UwpOriginalClientHandle UwpOriginalClientCreate()
{
	return new NinecraftApp();
}

bool UwpOriginalClientBindContext(UwpOriginalClientHandle handle,
	const AppContext& context)
{
	if (handle == nullptr || context.platform == nullptr)
		return false;

	NinecraftApp* app = static_cast<NinecraftApp*>(handle);
	app->bindContextWithoutInit(context);
	return app->isInited() && app->platform() == context.platform;
}

bool UwpOriginalClientSetStoragePaths(UwpOriginalClientHandle handle,
	const std::string& externalStoragePath,
	const std::string& externalCacheStoragePath)
{
	if (handle == nullptr)
		return false;

	NinecraftApp* app = static_cast<NinecraftApp*>(handle);
	app->externalStoragePath = externalStoragePath;
	app->externalCacheStoragePath = externalCacheStoragePath;
	return true;
}

bool UwpOriginalClientInitialize(UwpOriginalClientHandle handle,
	AppContext& context)
{
	if (handle == nullptr || context.platform == nullptr)
		return false;

	// NinecraftApp::init hides App::init.  Calling through the base class is
	// intentional: App::init copies the already-bound context, then dispatches
	// to the real virtual NinecraftApp::init implementation.
	NinecraftApp* app = static_cast<NinecraftApp*>(handle);
	try
	{
		static_cast<App*>(app)->init(context);
		return app->isInited() && app->platform() == context.platform;
	}
	catch (...)
	{
		return false;
	}
}

void UwpOriginalClientUpdate(UwpOriginalClientHandle handle)
{
	if (handle != nullptr)
		static_cast<NinecraftApp*>(handle)->update();
}

void UwpOriginalClientFixedTick(UwpOriginalClientHandle handle)
{
	if (handle != nullptr)
		static_cast<NinecraftApp*>(handle)->updateFixedStep();
}

void UwpOriginalClientRender(UwpOriginalClientHandle handle, float alpha)
{
	if (handle == nullptr)
		return;

	Minecraft* minecraft = UwpOriginalClientGetMinecraft(handle);
	if (minecraft != nullptr)
		minecraft->renderFrame(alpha);
}

void UwpOriginalClientSetSize(UwpOriginalClientHandle handle, int width, int height)
{
	if (handle == nullptr || width <= 0 || height <= 0)
		return;

	Minecraft* minecraft = UwpOriginalClientGetMinecraft(handle);
	if (minecraft != nullptr)
		minecraft->setSize(width, height);
}

bool UwpOriginalClientShowStartMenu(UwpOriginalClientHandle handle)
{
	Minecraft* minecraft = UwpOriginalClientGetMinecraft(handle);
	if (minecraft == nullptr)
		return false;
	// Keep the original menu navigation independent from the selected world
	// input mode.  In particular, do not overwrite Touch here: the setting is
	// intentionally carried into the borrowed world so it can be tested.
	g_pendingMenuAction = UwpOriginalMenuAction::None;
	g_pendingLevelId.clear();
	g_pendingLevelName.clear();
	g_pendingWorldEditId.clear();
	g_pendingLevelSeed = -1;
	g_pendingGameType = GameType::Undefined;
	g_pendingWorldType = WorldType::Old;
	// The UWP world-list bridge may have queued key transitions while the
	// original client loop was paused.  Do not replay those transitions into a
	// newly created menu screen.
	Keyboard::reset();
	if (minecraft->options.javaUi)
		minecraft->setScreen(new UwpStartMenuScreen());
	else
		minecraft->setScreen(new UwpTouchStartMenuScreen());
	return true;
}

bool UwpOriginalClientShowSelectWorld(UwpOriginalClientHandle handle)
{
	Minecraft* minecraft = UwpOriginalClientGetMinecraft(handle);
	if (minecraft == nullptr)
		return false;
	g_pendingMenuAction = UwpOriginalMenuAction::None;
	g_pendingLevelId.clear();
	g_pendingLevelName.clear();
	g_pendingWorldEditId.clear();
		g_pendingLevelSeed = -1;
		g_pendingGameType = GameType::Undefined;
		g_pendingWorldType = WorldType::Old;
	Keyboard::reset();
	minecraft->screenChooser.setScreen(SCREEN_SELECTWORLD);
	return minecraft->screen != nullptr;
}

bool UwpOriginalClientShowLegacyWorldFormatError(UwpOriginalClientHandle handle)
{
	Minecraft* minecraft = UwpOriginalClientGetMinecraft(handle);
	if (minecraft == nullptr)
		return false;

	Keyboard::reset();
	minecraft->setScreen(new UwpLegacyWorldFormatScreen());
	return minecraft->screen != nullptr;
}

bool UwpOriginalClientShowProgress(UwpOriginalClientHandle handle)
{
	Minecraft* minecraft = UwpOriginalClientGetMinecraft(handle);
	if (minecraft == nullptr)
		return false;

	// ProgressScreen is the original client loading UI. It renders the
	// packaged dirt texture through Minecraft::Textures and is safe before a
	// Level is attached because Minecraft::renderFrame() renders a non-null
	// Screen even when isLevelGenerated() is false.
	minecraft->setScreen(new ProgressScreen());
	return minecraft->screen != nullptr;
}

bool UwpOriginalClientEnsureStartMenu(UwpOriginalClientHandle handle)
{
	Minecraft* minecraft = UwpOriginalClientGetMinecraft(handle);
	if (minecraft == nullptr)
		return false;
	// Escape can clear the screen directly instead of going through
	// ScreenChooser.  While the original menu loop is active, an empty screen
	// would otherwise leave GameRenderer presenting only the clear color.  A
	// fresh original start screen is safe here because no world is attached.
	if (minecraft->screen == nullptr)
		return UwpOriginalClientShowStartMenu(handle);
	if (dynamic_cast<UwpStartMenuScreen*>(minecraft->screen) != nullptr)
		return minecraft->options.javaUi ? true : UwpOriginalClientShowStartMenu(handle);
	if (dynamic_cast<UwpTouchStartMenuScreen*>(minecraft->screen) != nullptr)
		return minecraft->options.javaUi ? UwpOriginalClientShowStartMenu(handle) : true;
	// OptionsScreen and the original back/escape path return through
	// ScreenChooser, which creates a plain StartMenuScreen.  Re-wrap only that
	// screen so the UWP Singleplayer handoff remains available without
	// replacing any other original screen (notably OptionsScreen itself).
	if (dynamic_cast<StartMenuScreen*>(minecraft->screen) != nullptr)
		return UwpOriginalClientShowStartMenu(handle);
	if (dynamic_cast<Touch::StartMenuScreen*>(minecraft->screen) != nullptr)
		return UwpOriginalClientShowStartMenu(handle);
	return false;
}

bool UwpOriginalClientWantsToQuit(UwpOriginalClientHandle handle)
{
	return handle != nullptr &&
		static_cast<NinecraftApp*>(handle)->wantToQuit();
}

UwpOriginalMenuAction UwpOriginalClientConsumeMenuAction(UwpOriginalClientHandle handle)
{
	if (handle == nullptr)
		return UwpOriginalMenuAction::None;
	const UwpOriginalMenuAction action = g_pendingMenuAction;
	// SelectWorld carries a second payload consumed by
	// UwpOriginalClientConsumeLevelSelection(); leave the action latched until
	// that payload is copied out.
	if (action != UwpOriginalMenuAction::SelectWorld &&
		action != UwpOriginalMenuAction::OpenWorldEdit)
		g_pendingMenuAction = UwpOriginalMenuAction::None;
	return action;
}

bool UwpOriginalClientSetSkinArmType(UwpOriginalClientHandle handle, bool slim)
{
	Minecraft* minecraft = UwpOriginalClientGetMinecraft(handle);
	if (minecraft == nullptr)
		return false;

	minecraft->options.slimSkin = slim;
	minecraft->options.save();
	return true;
}

bool UwpOriginalClientApplyImportedSkin(UwpOriginalClientHandle handle)
{
	Minecraft* minecraft = UwpOriginalClientGetMinecraft(handle);
	if (minecraft == nullptr)
		return false;

	if (minecraft->textures != nullptr)
		minecraft->textures->reloadAll();
	if (minecraft->player != nullptr)
		minecraft->player->setTextureName("skins/local.png");
	return true;
}

bool UwpOriginalClientClearImportedSkin(UwpOriginalClientHandle handle)
{
	Minecraft* minecraft = UwpOriginalClientGetMinecraft(handle);
	if (minecraft == nullptr)
		return false;

	if (minecraft->textures != nullptr)
		minecraft->textures->reloadAll();
	if (minecraft->player != nullptr)
		minecraft->player->setTextureName("mob/char.png");
	return true;
}

void UwpOriginalClientRequestLevelSelection(const std::string& levelId,
	const std::string& levelName, const LevelSettings& settings)
{
	g_pendingLevelId = levelId;
	g_pendingLevelName = levelName;
	g_pendingWorldEditId.clear();
	g_pendingLevelSeed = settings.getSeed();
	g_pendingGameType = settings.getGameType();
	g_pendingWorldType = settings.getWorldType();
	g_pendingMenuAction = UwpOriginalMenuAction::SelectWorld;
}

void UwpOriginalClientRequestLeaveWorld()
{
	// The payload fields belong only to SelectWorld and must not survive a
	// world-exit transition into the next title screen.
	g_pendingLevelId.clear();
	g_pendingLevelName.clear();
	g_pendingWorldEditId.clear();
	g_pendingLevelSeed = -1;
	g_pendingGameType = GameType::Undefined;
	g_pendingWorldType = WorldType::Old;
	g_pendingMenuAction = UwpOriginalMenuAction::LeaveWorld;
}

void UwpOriginalClientRequestKeyMapping()
{
	g_pendingWorldEditId.clear();
	g_pendingMenuAction = UwpOriginalMenuAction::OpenKeyMapping;
}

void UwpOriginalClientRequestWorldEdit(const std::string& worldId)
{
	if (worldId.empty())
		return;
	g_pendingLevelId.clear();
	g_pendingLevelName.clear();
	g_pendingWorldEditId = worldId;
	g_pendingMenuAction = UwpOriginalMenuAction::OpenWorldEdit;
}

bool UwpOriginalClientConsumeLevelSelection(UwpOriginalClientHandle handle,
	std::string& levelId, std::string& levelName, long& seed,
	int& gameType, int& worldType)
{
	if (handle == nullptr || g_pendingMenuAction != UwpOriginalMenuAction::SelectWorld)
		return false;
	levelId = g_pendingLevelId;
	levelName = g_pendingLevelName;
	seed = g_pendingLevelSeed;
	gameType = g_pendingGameType;
	worldType = g_pendingWorldType;
	g_pendingLevelId.clear();
	g_pendingLevelName.clear();
	g_pendingWorldEditId.clear();
	g_pendingLevelSeed = -1;
	g_pendingGameType = GameType::Undefined;
	g_pendingWorldType = WorldType::Old;
	g_pendingMenuAction = UwpOriginalMenuAction::None;
	return !levelId.empty();
}

bool UwpOriginalClientConsumeWorldEdit(UwpOriginalClientHandle handle,
	std::string& worldId)
{
	if (handle == nullptr || g_pendingMenuAction != UwpOriginalMenuAction::OpenWorldEdit)
		return false;
	worldId = g_pendingWorldEditId;
	g_pendingWorldEditId.clear();
	g_pendingMenuAction = UwpOriginalMenuAction::None;
	return !worldId.empty();
}

Minecraft* UwpOriginalClientGetMinecraft(UwpOriginalClientHandle handle)
{
	return handle != nullptr ? static_cast<Minecraft*>(static_cast<NinecraftApp*>(handle)) : nullptr;
}

LevelRenderer* UwpOriginalClientGetLevelRenderer(UwpOriginalClientHandle handle)
{
	Minecraft* minecraft = UwpOriginalClientGetMinecraft(handle);
	return minecraft != nullptr ? minecraft->levelRenderer : nullptr;
}

bool UwpOriginalClientAttachWorld(UwpOriginalClientHandle handle,
	Level* world, LocalPlayer* localPlayer)
{
	Minecraft* minecraft = UwpOriginalClientGetMinecraft(handle);
	return minecraft != nullptr && minecraft->attachUwpWorld(world, localPlayer);
}

bool UwpOriginalClientHostMultiplayer(UwpOriginalClientHandle handle, int port)
{
	Minecraft* minecraft = UwpOriginalClientGetMinecraft(handle);
	if (minecraft == nullptr || minecraft->level == nullptr ||
		minecraft->raknetInstance == nullptr)
		return false;

	// Reuse the original Minecraft host path.  The UWP world is already
	// generated, so explicitly deliver the same levelGenerated callback that
	// Win32 normally receives at the end of threaded generation.
	minecraft->hostMultiplayer(port);
	if (!minecraft->raknetInstance->isServer() || minecraft->netCallback == nullptr)
		return false;

	minecraft->netCallback->levelGenerated(minecraft->level);
	return true;
}

void UwpOriginalClientDetachWorld(UwpOriginalClientHandle handle)
{
	Minecraft* minecraft = UwpOriginalClientGetMinecraft(handle);
	if (minecraft != nullptr)
		minecraft->detachUwpWorld();
}

void UwpOriginalClientDestroy(UwpOriginalClientHandle handle)
{
	delete static_cast<NinecraftApp*>(handle);
}
