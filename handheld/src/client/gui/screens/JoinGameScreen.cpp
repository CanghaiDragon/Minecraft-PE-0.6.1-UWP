#include "JoinGameScreen.h"
#include "StartMenuScreen.h"
#include "ProgressScreen.h"
#include "../Font.h"
#include "../../../network/RakNetInstance.h"
#include "../../../locale/I18n.h"
#include <vector>

JoinGameScreen::JoinGameScreen()
:	bBack(  3, I18n::get("gui.back")),
	bMode(  4, I18n::get("multiplayer.switchMode")),
	gamesList(NULL)
{
	//gamesList->yInertia = 0.5f;
}

JoinGameScreen::~JoinGameScreen()
{
	delete gamesList;
}

void JoinGameScreen::buttonClicked(Button* button)
{
	if (button->id == bMode.id)
	{
		minecraft->setExtendedMultiplayer(!minecraft->isExtendedMultiplayer());
	}
	if (button->id == bBack.id)
	{
		minecraft->cancelLocateMultiplayer();
		minecraft->screenChooser.setScreen(SCREEN_STARTMENU);
	}
}

bool JoinGameScreen::handleBackEvent(bool isDown)
{
	if (!isDown)
	{
		minecraft->cancelLocateMultiplayer();
		minecraft->screenChooser.setScreen(SCREEN_STARTMENU);
	}
	return true;
}


bool JoinGameScreen::isIndexValid( int index )
{
	return gamesList && index >= 0 && index < gamesList->getNumberOfItems();
}

void AvailableGamesList::selectItem(int item, bool)
{
	selectedItem = item;
}

void JoinGameScreen::joinSelectedServer()
{
	if (!isIndexValid(gamesList->selectedItem)) return;
	PingedCompatibleServer selectedServer = gamesList->copiedServerList[gamesList->selectedItem];
	minecraft->joinMultiplayer(selectedServer);
	bMode.active = false;
	bBack.active = false;
	minecraft->setScreen(new ProgressScreen());
}

void JoinGameScreen::tick()
{
	if (isIndexValid(gamesList->selectedItem)) {
		joinSelectedServer();
		return;
	}

	const ServerList& orgServerList = minecraft->raknetInstance->getServerList();
	ServerList serverList;
	for (unsigned int i = 0; i < orgServerList.size(); ++i)
		if (orgServerList[i].name.GetLength() > 0)
			serverList.push_back(orgServerList[i]);

	if (serverList.size() != gamesList->copiedServerList.size())
	{
		// copy the currently selected item
		PingedCompatibleServer selectedServer;
		bool hasSelection = false;
		if (isIndexValid(gamesList->selectedItem))
		{
			selectedServer = gamesList->copiedServerList[gamesList->selectedItem];
			hasSelection = true;
		}

		gamesList->copiedServerList = serverList;
		gamesList->selectItem(-1, false);

		// re-select previous item if it still exists
		if (hasSelection)
		{
			for (unsigned int i = 0; i < gamesList->copiedServerList.size(); i++)
			{
				if (gamesList->copiedServerList[i].address == selectedServer.address)
				{
					gamesList->selectItem(i, false);
					break;
				}
			}
		}
	} else {
		for (int i = (int)gamesList->copiedServerList.size()-1; i >= 0 ; --i) {
			for (int j = 0; j < (int) serverList.size(); ++j)
				if (serverList[j].address == gamesList->copiedServerList[i].address)
					gamesList->copiedServerList[i].name = serverList[j].name;
		}
	}

}

void JoinGameScreen::init()
{
	buttons.push_back(&bBack);
	buttons.push_back(&bMode);

	minecraft->raknetInstance->clearServerList();
	gamesList = new AvailableGamesList(minecraft, width, height);

#ifdef ANDROID
	tabButtons.push_back(&bBack);
#endif
}

void JoinGameScreen::setupPositions() {
	int yBase = height - 26;

	//#ifdef ANDROID
	bMode.y =	yBase;
	bBack.y =   yBase;

	bBack.width = bMode.width = 120;
	bMode.height = bBack.height = 24;
	//#endif

	// Server selection continues to start joining; Switch occupies the old
	// Join Game button position.
	bMode.x = width / 2 - 4 - bMode.width;
	bBack.x = width / 2 + 4;
}

void JoinGameScreen::render( int xm, int ym, float a )
{
	bool hasNetwork = minecraft->platform()->isNetworkEnabled(true);
#if defined(WIN32) && !defined(MCPE_UWP)
	hasNetwork = hasNetwork && !GetAsyncKeyState(VK_TAB);
#endif

	renderBackground();
	if (hasNetwork) gamesList->render(xm, ym, a);
	Screen::render(xm, ym, a);

	if (hasNetwork) {
#ifdef RPI
		std::string s = I18n::get("multiplayer.scanningLocal");
#else
		std::string s = I18n::get("multiplayer.scanningWifi");
#endif
		drawCenteredString(minecraft->font, s, width / 2, 8, 0xffffffff);

		const int textWidth = minecraft->font->width(s);
		const int spinnerX = width/2 + textWidth / 2 + 6;

		static const char* spinnerTexts[] = {"-", "\\", "|", "/"};
		int n = ((int)(5.5f * getTimeS()) % 4);
		drawCenteredString(minecraft->font, spinnerTexts[n], spinnerX, 8, 0xffffffff);
	} else {
		std::string s = I18n::get("multiplayer.wifiDisabled");
		const int yy = height / 2 - 8;
		drawCenteredString(minecraft->font, s, width / 2, yy, 0xffffffff);
	}

	const bool extended = minecraft->isExtendedMultiplayer();
	const std::string modeLine = extended
		? (width < 360 ? I18n::get("multiplayer.extendedMode.short")
			: I18n::get("multiplayer.extendedMode.long"))
		: (width < 360 ? I18n::get("multiplayer.compatibleMode.short")
			: I18n::get("multiplayer.compatibleMode.long"));
	const int modeLineX = (width - minecraft->font->width(modeLine)) / 2;
	minecraft->font->draw(modeLine, (float)modeLineX,
		(float)(height - 54), 0xffc0c0c0);
	drawCenteredString(minecraft->font,
		extended ? (width < 360 ? I18n::get("multiplayer.projectClients.short")
			: I18n::get("multiplayer.projectClients.long"))
			: I18n::get("multiplayer.originalClients"),
		width / 2, height - 44, 0xffc0c0c0);

}

bool JoinGameScreen::isInGameScreen() { return false; }
