#include "WelcomeScreen.h"

#include "../../Minecraft.h"
#include "../Gui.h"
#include "../../renderer/gles.h"
#include "ScreenChooser.h"
#include "../../../platform/input/Mouse.h"
#include "../../../platform/input/Multitouch.h"
#include "../../../locale/I18n.h"

WelcomeScreen::WelcomeScreen()
:	touchButton(1, I18n::get("welcome.touchControls")),
	keyboardButton(2, I18n::get("welcome.keyboardMouse")),
	defaultProfileButton(3, I18n::get("welcome.desktopDefault")),
	lowPerformanceProfileButton(4, I18n::get("welcome.desktopLow")),
	guiScaleButton(5, I18n::get("options.guiScale")),
	originLanguageButton(6, I18n::get("welcome.language.origin")),
	systemLanguageButton(7, I18n::get("welcome.language.system")),
	continueButton(8, I18n::get("gui.continue")),
	languageMode("system"),
	touchControls(true),
	defaultProfile(Options::DefaultProfile::DesktopDefault),
	guiScale(0),
	scrollY(0),
	contentHeight(0),
	dragging(false),
	dragMoved(false),
	dragLastY(0)
{
}

void WelcomeScreen::init()
{
	touchControls = minecraft->options.useTouchScreen;
	guiScale = minecraft->options.guiScale;
	languageMode = minecraft->options.languageCode == "origin" ? "origin" : "system";

	buttons.push_back(&touchButton);
	buttons.push_back(&keyboardButton);
	buttons.push_back(&defaultProfileButton);
	buttons.push_back(&lowPerformanceProfileButton);
	buttons.push_back(&guiScaleButton);
	buttons.push_back(&originLanguageButton);
	buttons.push_back(&systemLanguageButton);
	buttons.push_back(&continueButton);
	updateSelection();
}

void WelcomeScreen::setupPositions()
{
	// touchgui's button strip is only 66px wide before scaling.  Keeping the
	// controls compact avoids stretched/cut-off ends on large GUI scales.
	int buttonWidth = width - 24;
	if (buttonWidth > 220) buttonWidth = 220;
	const int buttonHeight = 26;
	const int x = (width - buttonWidth) / 2;

	touchButton.x = keyboardButton.x = defaultProfileButton.x =
		lowPerformanceProfileButton.x = guiScaleButton.x = continueButton.x = x;
	touchButton.width = keyboardButton.width = defaultProfileButton.width =
		lowPerformanceProfileButton.width = guiScaleButton.width = continueButton.width = buttonWidth;
	touchButton.height = keyboardButton.height = defaultProfileButton.height =
		lowPerformanceProfileButton.height = guiScaleButton.height = continueButton.height = buttonHeight;

	const int languageButtonWidth = (buttonWidth - 4) / 2;
	originLanguageButton.x = x;
	systemLanguageButton.x = x + languageButtonWidth + 4;
	originLanguageButton.width = systemLanguageButton.width = languageButtonWidth;
	originLanguageButton.height = systemLanguageButton.height = buttonHeight;
	originLanguageButton.y = systemLanguageButton.y = 58;
	touchButton.y = 128;
	keyboardButton.y = 160;
	defaultProfileButton.y = 230;
	lowPerformanceProfileButton.y = 262;
	guiScaleButton.y = 320;
	continueButton.y = 372;
	contentHeight = continueButton.y + continueButton.height + 48;
	clampScroll();
}

void WelcomeScreen::render(int xm, int ym, float a)
{
	// Match Screen::mouseEvent's coordinate conversion for desktop pointers.
	// GameRenderer's touch coordinate may intentionally be -9999 when no
	// touch pointer is active, so keep that path unchanged.
	// On a touch-capable Windows device useTouchscreen() can remain true even
	// after choosing keyboard/mouse mode.  In that case GameRenderer supplies
	// -9999 while no finger is down, which prevents mouse hover highlighting.
	// Use the real mouse position whenever it is available, and override it
	// only while an actual touch pointer is active.
	int hoverX = Mouse::getX() * width / minecraft->width;
	int hoverY = Mouse::getY() * height / minecraft->height - 1;
	if (Multitouch::getFirstActivePointerIdExThisUpdate() >= 0) {
		hoverX = xm;
		hoverY = ym;
	}
	renderDirtBackground(0);
	const int headerHeight = 30;
	fill(0, 0, width, headerHeight, 0x99000000);
	drawCenteredString(font, I18n::get("welcome.title"), width / 2, 10, 0xffffff);

	const int visibleHeight = height - headerHeight;
	GLint scissorY = minecraft->height - (GLint)(Gui::GuiScale * height);
	glEnable2(GL_SCISSOR_TEST);
	glScissor(0, scissorY, (GLsizei)(Gui::GuiScale * width), (GLsizei)(Gui::GuiScale * visibleHeight));
	glPushMatrix2();
	glTranslatef2(0, -scrollY, 0);

	drawCenteredString(font, I18n::get("welcome.language.title"), width / 2, 38, 0xffffff);
	drawCenteredString(font, I18n::get("welcome.controls.title"), width / 2, 100, 0xffffff);
	drawCenteredString(font, I18n::get("welcome.controls.info"), width / 2, 114, 0xc0c0c0);
	drawCenteredString(font, I18n::get("welcome.profile.title"), width / 2, 202, 0xffffff);
	drawCenteredString(font, I18n::get("welcome.profile.info"), width / 2, 216, 0xc0c0c0);
	drawCenteredString(font, I18n::get("welcome.scale.title"), width / 2, 304, 0xffffff);
	drawCenteredString(font, I18n::get("welcome.options.info"), width / 2, 406, 0xc0c0c0);
	drawCenteredString(font, I18n::get("welcome.additional.info"), width / 2, 418, 0xc0c0c0);
	for (unsigned int i = 0; i < buttons.size(); ++i)
		buttons[i]->render(minecraft, hoverX, hoverY + (int)scrollY);

	glPopMatrix2();
	const int maxScroll = contentHeight - visibleHeight;
	if (maxScroll > 0) {
		float barHeight = (float)visibleHeight * visibleHeight / contentHeight;
		if (barHeight < 8) barHeight = 8;
		float barY = headerHeight + (scrollY / maxScroll) * (visibleHeight - barHeight);
		fill(width - 4, (int)barY, width - 2, (int)(barY + barHeight), 0x88ffffff);
	}
	glDisable2(GL_SCISSOR_TEST);
}

bool WelcomeScreen::handleBackEvent(bool isDown)
{
	return true;
}

void WelcomeScreen::mouseEvent()
{
	const MouseAction& event = Mouse::getEvent();
	if (event.action == MouseAction::ACTION_MOVE && Mouse::isButtonDown(MouseAction::ACTION_LEFT) && event.dy != 0) {
		scrollY -= event.dy * height / minecraft->height;
		clampScroll();
		dragging = true;
		dragMoved = true;
		return;
	}
	Screen::mouseEvent();
}

void WelcomeScreen::mouseClicked(int x, int y, int buttonNum)
{
	dragging = true;
	dragMoved = false;
	dragLastY = y;
	Screen::mouseClicked(x, y + (int)scrollY, buttonNum);
}

void WelcomeScreen::mouseReleased(int x, int y, int buttonNum)
{
	if (dragMoved) {
		if (clickedButton != NULL) {
			clickedButton->released(x, y + (int)scrollY);
			clickedButton = NULL;
		}
		dragging = false;
		return;
	}
	dragging = false;
	Screen::mouseReleased(x, y + (int)scrollY, buttonNum);
}

void WelcomeScreen::mouseScrolled(int x, int y, int delta)
{
	scrollY -= delta * 18;
	clampScroll();
}

void WelcomeScreen::tick()
{
	// Touch input is not guaranteed to be mirrored into the legacy Mouse
	// queue on Windows.  Read the active pointer directly so the welcome page
	// can be dragged on a touchscreen just like the options page.
	const int* pointerIds = NULL;
	const int pointerCount = Multitouch::getActivePointerIdsThisUpdate(&pointerIds);
	for (int i = 0; i < pointerCount; ++i) {
		const int pointerId = pointerIds[i];
		if (Multitouch::isPointerDown(pointerId)) {
			const int dy = Multitouch::getDY(pointerId);
			if (dy != 0) {
				scrollY -= (float)dy * Gui::InvGuiScale;
				clampScroll();
				dragging = true;
				dragMoved = true;
			}
		}
	}
	// Some Windows touch drivers expose the contact through the legacy Mouse
	// state only.  Keep a drag baseline so those contacts scroll as well.
	if (dragging && Mouse::isButtonDown(MouseAction::ACTION_LEFT)) {
		int my = Mouse::getY() * height / minecraft->height - 1;
		const int dy = my - dragLastY;
		if (dy != 0) {
			scrollY -= (float)dy;
			clampScroll();
			dragMoved = true;
			dragLastY = my;
		}
	}
	super::tick();
}

void WelcomeScreen::keyPressed(int eventKey)
{
	// The welcome screen must not acquire the desktop menu's initial focus or
	// be dismissed with Escape before its two choices have been made.
}

void WelcomeScreen::buttonClicked(Button* button)
{
	if (button == &touchButton) {
		touchControls = true;
		updateSelection();
	} else if (button == &keyboardButton) {
		touchControls = false;
		updateSelection();
	} else if (button == &defaultProfileButton) {
		defaultProfile = Options::DefaultProfile::DesktopDefault;
		updateSelection();
	} else if (button == &lowPerformanceProfileButton) {
		defaultProfile = Options::DefaultProfile::DesktopLowPerformance;
		updateSelection();
	} else if (button == &guiScaleButton) {
		guiScale = (guiScale + 1) % 5;
		updateSelection();
		// Apply immediately so setSize() recalculates Gui::GuiScale and
		// re-lays out this welcome page for the newly selected size.
		minecraft->options.set(&Options::Option::GUI_SCALE, guiScale);
	} else if (button == &originLanguageButton || button == &systemLanguageButton) {
		const std::string selectedLanguage = button == &originLanguageButton ? "origin" : "system";
		if (languageMode != selectedLanguage) {
			languageMode = selectedLanguage;
			minecraft->options.languageCode = languageMode;
			I18n::loadLanguage(minecraft->platform(), languageMode);
			updateSelection();
		}
	} else if (button == &continueButton) {
		// Apply the selected base profile first, then restore the two choices
		// made explicitly on this page.  The profile therefore controls only
		// the remaining settings and never overrides Input Mode or GUI Scale.
		const bool inputModeChanged = minecraft->options.useTouchScreen != touchControls;
		minecraft->options.applyDefaultProfile(defaultProfile);
		minecraft->options.useTouchScreen = touchControls;
		minecraft->options.guiScale = guiScale;
		minecraft->options.save();
		// reloadOptions() sees the already-updated value and therefore cannot
		// tell that the existing TouchInputHolder must be replaced.  Use the
		// same transition path as the Controls setting so D-Pad/touch look is
		// fully torn down before the desktop start menu is opened.
		if (inputModeChanged)
			minecraft->optionUpdated(&Options::Option::INPUT_MODE, touchControls ? 1 : 0);
		minecraft->reloadOptions();
		minecraft->screenChooser.setScreen(SCREEN_STARTMENU);
	}
}

void WelcomeScreen::updateSelection()
{
	touchButton.msg = I18n::get("welcome.touchControls");
	keyboardButton.msg = I18n::get("welcome.keyboardMouse");
	defaultProfileButton.msg = I18n::get("welcome.desktopDefault");
	lowPerformanceProfileButton.msg = I18n::get("welcome.desktopLow");
	originLanguageButton.msg = I18n::get("welcome.language.origin");
	systemLanguageButton.msg = I18n::get("welcome.language.system");
	continueButton.msg = I18n::get("gui.continue");
	touchButton.selected = touchControls;
	keyboardButton.selected = !touchControls;
	defaultProfileButton.selected =
		defaultProfile == Options::DefaultProfile::DesktopDefault;
	lowPerformanceProfileButton.selected =
		defaultProfile == Options::DefaultProfile::DesktopLowPerformance;
	originLanguageButton.selected = languageMode == "origin";
	systemLanguageButton.selected = languageMode == "system";
	guiScaleButton.selected = false;
	guiScaleButton.msg = I18n::get("welcome.guiScale") + ": " +
		I18n::get(Options::GUI_SCALE[guiScale]);
}

void WelcomeScreen::clampScroll()
{
	const int maxScroll = contentHeight - (height - 30);
	if (scrollY < 0) scrollY = 0;
	if (maxScroll <= 0) scrollY = 0;
	else if (scrollY > maxScroll) scrollY = (float)maxScroll;
}
