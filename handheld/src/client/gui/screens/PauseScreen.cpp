#include "PauseScreen.h"
#include "StartMenuScreen.h"
#include "AchievementsScreen.h"
#include "../components/ImageButton.h"
#include "../../Minecraft.h"
#include "../../../util/Mth.h"
#include "../../../world/level/Level.h"
#include "../../../world/level/storage/LevelStorage.h"
#include "../../../world/level/storage/AchievementIds.h"
#include "../../../network/RakNetInstance.h"
#include "../../../network/ServerSideNetworkHandler.h"
#include "../../../locale/I18n.h"

PauseScreen::PauseScreen(bool wasBackPaused)
:	saveStep(0),
	visibleTime(0),
	bContinue(0),
	bQuit(0),
	bQuitAndSaveLocally(0),
	bServerVisibility(0),
//	bThirdPerson(0),
	wasBackPaused(wasBackPaused),
	bSound(&Options::Option::SOUND, 1, 0),
	bThirdPerson(&Options::Option::THIRD_PERSON),
	bHideGui(&Options::Option::HIDE_GUI),
	bEntityButton(5, ""),
	entityButtonState(false),
	bSpectatorButton(6, ""),
	spectatorButtonState(false),
	bAchievementButton(7, "")
{
	ImageDef def;
	// touchguinew.png keeps the pause icons in three consecutive rows.
	// The first two rows are the sound and camera buttons; the third row is
	// reserved for the F3/entity toggle.
	def.setSrc(IntRectangle(160, 142, 39, 31));
	def.name = "gui/touchgui.png";
	IntRectangle& defSrc = *def.getSrc();

	def.width = defSrc.w * 0.666667f;
	def.height = defSrc.h * 0.666667f;

	bSound.setImageDef(def, true);
	defSrc.y = 173;
	bThirdPerson.setImageDef(def, true);
    bHideGui.setImageDef(def, true);
	// F3 toggle frames are the two wrench buttons in the bottom-right row.
	entityButtonOff.setSrc(IntRectangle(160, 204, 39, 31));
	entityButtonOff.name = "gui/touchgui.png";
	entityButtonOff.width = 39 * 0.666667f;
	entityButtonOff.height = 31 * 0.666667f;
	entityButtonOn.setSrc(IntRectangle(199, 204, 39, 31));
	entityButtonOn.name = "gui/touchgui.png";
	entityButtonOn.width = 39 * 0.666667f;
	entityButtonOn.height = 31 * 0.666667f;
	entityButtonState = false;
	bEntityButton.setImageDef(entityButtonState ? entityButtonOn : entityButtonOff, true);
	// The two eye states are supplied by touchgui2.png.  The left frame is
	// spectator mode off; the right frame is spectator mode on.
	spectatorButtonOff.setSrc(IntRectangle(160, 142, 39, 31));
	spectatorButtonOff.name = "gui/touchgui2.png";
	spectatorButtonOff.width = 39 * 0.666667f;
	spectatorButtonOff.height = 31 * 0.666667f;
	spectatorButtonOn.setSrc(IntRectangle(199, 142, 39, 31));
	spectatorButtonOn.name = "gui/touchgui2.png";
	spectatorButtonOn.width = 39 * 0.666667f;
	spectatorButtonOn.height = 31 * 0.666667f;
	spectatorButtonState = false;
	bSpectatorButton.setImageDef(
		spectatorButtonState ? spectatorButtonOn : spectatorButtonOff, true);
	// Trophy icon directly below the spectator eye in touchgui2.png.
	achievementButtonImage.setSrc(IntRectangle(160, 173, 39, 31));
	achievementButtonImage.name = "gui/touchgui2.png";
	achievementButtonImage.width = 39 * 0.666667f;
	achievementButtonImage.height = 31 * 0.666667f;
	bAchievementButton.setImageDef(achievementButtonImage, true);
	//void setImageDef(ImageDef& imageDef, bool setButtonSize);
}

PauseScreen::~PauseScreen() {
	delete bContinue;
	delete bQuit;
	delete bQuitAndSaveLocally;
	delete bServerVisibility;
//	delete bThirdPerson;
}

void PauseScreen::init() {
	entityButtonState = minecraft->options.renderDebug;
	bEntityButton.setImageDef(entityButtonState ? entityButtonOn : entityButtonOff, false);
	spectatorButtonState = minecraft->isSpectatorMode();
	bSpectatorButton.setImageDef(
		spectatorButtonState ? spectatorButtonOn : spectatorButtonOff, false);
	bool allAchievementsUnlocked = false;
	if (minecraft->options.achievementsEnabled && !minecraft->isCreativeMode() && minecraft->level != NULL) {
		LevelStorage* storage = minecraft->level->getLevelStorage();
		if (storage != NULL) {
			allAchievementsUnlocked = true;
			for (int i = 0; i < AchievementIds::Count; ++i) {
				if (!storage->hasAchievement(i)) {
					allAchievementsUnlocked = false;
					break;
				}
			}
		}
	}
	achievementButtonImage.setSrc(IntRectangle(allAchievementsUnlocked ? 199 : 160, 173, 39, 31));
	bAchievementButton.setImageDef(achievementButtonImage, false);

	// Pause presentation follows Java UI, not the selected input device. PE
	// pause controls must remain available when a keyboard is connected.
	if (!minecraft->options.javaUi) {
		bContinue = new Touch::TButton(1, I18n::get("menu.returnToGame"));
		bQuit = new Touch::TButton(2, I18n::get("menu.returnToMenu"));
		bQuitAndSaveLocally = new Touch::TButton(3, I18n::get("menu.quitAndCopyMap"));
		bServerVisibility = new Touch::TButton(4, "");
//		bThirdPerson = new Touch::TButton(5, "Toggle 3:rd person view");
	} else {
		bContinue = new Button(1, I18n::get("menu.returnToGame"));
		bQuit = new Button(2, I18n::get("menu.returnToMenu"));
		bQuitAndSaveLocally = new Button(3, I18n::get("menu.quitAndCopyMap"));
		bServerVisibility = new Button(4, "");
//		bThirdPerson = new Button(5, "Toggle 3:rd person view");
	}

	buttons.push_back(bContinue);
	buttons.push_back(bQuit);
	bool legacyCompatibleWorld = true;
	if (minecraft->level != NULL && minecraft->level->getLevelData() != NULL) {
		int worldType = minecraft->level->getLevelData()->getWorldType();
		legacyCompatibleWorld = worldType == WorldType::Old;
	}
	bool multiplayerAvailable = minecraft->isExtendedMultiplayer() || legacyCompatibleWorld;
	if (!multiplayerAvailable) {
		// Legacy protocol clients cannot represent these custom world types.
		// Close any existing session when the pause screen is opened and keep
		// the multiplayer controls out of this screen.
		minecraft->cancelLocateMultiplayer();
		if (minecraft->raknetInstance)
			minecraft->raknetInstance->disconnect();
	}
	bAchievementButton.visible = minecraft->options.achievementsEnabled &&
		minecraft->level != NULL && !minecraft->isCreativeMode();
	if (bAchievementButton.visible) {
		buttons.push_back(&bAchievementButton);
	}

	bSound.updateImage(&minecraft->options);
	bThirdPerson.updateImage(&minecraft->options);
	bHideGui.updateImage(&minecraft->options);
	buttons.push_back(&bSound);
	buttons.push_back(&bThirdPerson);
	bEntityButton.visible = minecraft->options.debugScreenEnabled;
	if (bEntityButton.visible)
		buttons.push_back(&bEntityButton);
	bSpectatorButton.visible = minecraft->options.debugScreenEnabled &&
		minecraft->isCreativeMode();
	if (bSpectatorButton.visible)
		buttons.push_back(&bSpectatorButton);
    //buttons.push_back(&bHideGui);

	// If Back wasn't pressed, set up additional items (more than Quit to menu
	// and Back to game) here
    
    #if !defined(APPLE_DEMO_PROMOTION) && !defined(RPI)
	if (multiplayerAvailable && (true || !wasBackPaused)) {
		if (minecraft->raknetInstance) {
			// A local UWP world remains offline until the user explicitly opens
			// it to LAN. Keep the original server button visible in that state,
			// but do not create the network callback during world attachment.
			if (minecraft->raknetInstance->isServer()) {
				if (minecraft->netCallback) {
					updateServerVisibilityText();
					buttons.push_back(bServerVisibility);
				}
			}
			// Visitors can save the received temporary world through the existing
			// copy-map flow; renameLevel moves it from temporary to world storage.
			else if (minecraft->level != NULL && minecraft->level->isClientSide) {
				bQuitAndSaveLocally->msg = I18n::get("menu.quitAndCopyMap");
				buttons.push_back(bQuitAndSaveLocally);
			}
			#if defined(MCPE_UWP)
			// UWP local worlds deliberately stay offline until the player chooses
			// Open to LAN. Do not apply this to client-side visitor levels above.
			else if (minecraft->hasUwpWorld()) {
				bServerVisibility->msg = I18n::get("menu.openToLan");
				buttons.push_back(bServerVisibility);
			}
			#endif
			else {
                #if !defined(DEMO_MODE)
                buttons.push_back(bQuitAndSaveLocally);
				#endif
			}
		}
	}
	// Custom world types cannot use the legacy protocol. Offer a one-click
	// transition here so the player can enable the extended protocol and open
	// the current local world without leaving the pause screen.
	if (!multiplayerAvailable && minecraft->level != NULL &&
		minecraft->raknetInstance != NULL) {
		bServerVisibility->msg = I18n::get("menu.extendedMultiplayer");
		buttons.push_back(bServerVisibility);
	}
    #endif
//	buttons.push_back(bThirdPerson);

	for (unsigned int i = 0; i < buttons.size(); ++i) {
		if (buttons[i] == &bSound) continue;
		if (buttons[i] == &bThirdPerson) continue;
		if (buttons[i] == &bHideGui) continue;
		tabButtons.push_back(buttons[i]);
	}
}

void PauseScreen::setupPositions() {
    saveStep = 0;
	int yBase = 16;

	bContinue->width = bQuit->width = /*bThirdPerson->w =*/ 160;
	bQuitAndSaveLocally->width = bServerVisibility->width = 160;

	bContinue->x = (width - bContinue->width) / 2;
	bContinue->y = yBase + 32 * 1;

	bQuit->x = (width - bQuit->width) / 2;
	bQuit->y = yBase + 32 * 2;

#if APPLE_DEMO_PROMOTION
    bQuit->y += 16;
#endif
    
	bQuitAndSaveLocally->x = bServerVisibility->x = (width - bQuitAndSaveLocally->width) / 2;
	bQuitAndSaveLocally->y = bServerVisibility->y = yBase + 32 * 3;
	bAchievementButton.x = width - bAchievementButton.width - 8;
	bAchievementButton.y = 8;

	bSound.y = bThirdPerson.y = 8;
	bSound.x = 4;
	bThirdPerson.x = bSound.x + 4 + bSound.width;
	bHideGui.x = bThirdPerson.x + 4 + bThirdPerson.width;
	bEntityButton.y = bThirdPerson.y;
	bEntityButton.x = bThirdPerson.x + bThirdPerson.width + 4;
	bSpectatorButton.y = bEntityButton.y;
	bSpectatorButton.x = bEntityButton.x + bEntityButton.width + 4;

	//bThirdPerson->x = (width - bThirdPerson->w) / 2;
	//bThirdPerson->y = yBase + 32 * 4;
}

void PauseScreen::tick() {
	super::tick();
	visibleTime++;
}

void PauseScreen::render(int xm, int ym, float a) {
	renderBackground();

	// Keep the icon synchronized with keyboard/F3 changes made outside this
	// pause screen.
	bool debugEnabled = minecraft->options.renderDebug;
	bEntityButton.visible = minecraft->options.debugScreenEnabled;
	if (entityButtonState != debugEnabled) {
		entityButtonState = debugEnabled;
		bEntityButton.setImageDef(entityButtonState ? entityButtonOn : entityButtonOff, false);
	}
	bool spectatorEnabled = minecraft->isSpectatorMode();
	bSpectatorButton.visible = minecraft->options.debugScreenEnabled &&
		minecraft->isCreativeMode();
	if (spectatorButtonState != spectatorEnabled) {
		spectatorButtonState = spectatorEnabled;
		bSpectatorButton.setImageDef(
			spectatorButtonState ? spectatorButtonOn : spectatorButtonOff, false);
	}

	//bool isSaving = !minecraft->level.pauseSave(saveStep++);
	//if (isSaving || visibleTime < 20) {
	//	float col = ((visibleTime % 10) + a) / 10.0f;
	//	col = Mth::sin(col * Mth::PI * 2) * 0.2f + 0.8f;
	//	int br = (int) (255 * col);

	//	drawString(font, "Saving level..", 8, height - 16, br << 16 | br << 8 | br);
	//}

	drawCenteredString(font, I18n::get("menu.game"), width / 2, 24, 0xffffff);

	super::render(xm, ym, a);
}

void PauseScreen::buttonClicked(Button* button) {
	if (button->id == bEntityButton.id) {
		if (!minecraft->options.debugScreenEnabled)
			return;
		entityButtonState = !entityButtonState;
		minecraft->options.renderDebug = entityButtonState;
		bEntityButton.setImageDef(entityButtonState ? entityButtonOn : entityButtonOff, false);
	}
	if (button->id == bSpectatorButton.id) {
		if (!minecraft->options.debugScreenEnabled || !minecraft->isCreativeMode())
			return;
		minecraft->setSpectatorMode(!minecraft->isSpectatorMode());
		spectatorButtonState = minecraft->isSpectatorMode();
		bSpectatorButton.setImageDef(
			spectatorButtonState ? spectatorButtonOn : spectatorButtonOff, false);
	}
	if (button->id == bContinue->id) {
		minecraft->setScreen(NULL);
		//minecraft->grabMouse();
	}
    if (button->id == bQuit->id) {
		minecraft->leaveGame();
    }
	if (button->id == bQuitAndSaveLocally->id) {
		minecraft->leaveGame(true);
	}
	if (button == &bAchievementButton) {
		if (!minecraft->options.achievementsEnabled || minecraft->level == NULL)
			return;
		if (minecraft->isCreativeMode())
			return;
		minecraft->setScreen(new AchievementsScreen());
	}

	if (button->id == bServerVisibility->id) {
		if (!minecraft->isExtendedMultiplayer() && minecraft->level != NULL &&
			minecraft->level->getLevelData() != NULL &&
			minecraft->level->getLevelData()->getWorldType() != WorldType::Old) {
			minecraft->setExtendedMultiplayer(true);
		}
		if (minecraft->raknetInstance &&
			(!minecraft->raknetInstance->isServer() || minecraft->netCallback == NULL)) {
			// UWP local worlds start offline. Reuse the original host path only
			// after an explicit user action, and bind the already existing Level;
			// no second world or player is created here.
			minecraft->hostMultiplayer();
			if (minecraft->raknetInstance->isServer() &&
				minecraft->netCallback != NULL && minecraft->level != NULL) {
				minecraft->netCallback->levelGenerated(minecraft->level);
			}
		}
		if (minecraft->raknetInstance && minecraft->netCallback &&
			minecraft->raknetInstance->isServer()) {
			ServerSideNetworkHandler* ss = (ServerSideNetworkHandler*) minecraft->netCallback;
			bool allows = !ss->allowsIncomingConnections();
			ss->allowIncomingConnections(allows);

			updateServerVisibilityText();
		}
	}

	if (button->id == OptionButton::ButtonId) {
		((OptionButton*)button)->toggle(&minecraft->options);
	}

	//if (button->id == bThirdPerson->id) {
	//	minecraft->options.thirdPersonView = !minecraft->options.thirdPersonView;
	//}
}

void PauseScreen::updateServerVisibilityText()
{
	if (!minecraft->raknetInstance || !minecraft->raknetInstance->isServer() ||
		!minecraft->netCallback)
		return;

	ServerSideNetworkHandler* ss = (ServerSideNetworkHandler*) minecraft->netCallback;
	bServerVisibility->msg = I18n::get(ss->allowsIncomingConnections()?
		"menu.serverVisible" : "menu.serverInvisible");
}
