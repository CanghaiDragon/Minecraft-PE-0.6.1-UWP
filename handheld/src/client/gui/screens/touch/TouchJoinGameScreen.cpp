#include "TouchJoinGameScreen.h"
#include "../StartMenuScreen.h"
#include "../ProgressScreen.h"
#include "../../Font.h"
#include "../../../../locale/I18n.h"
#include "../../../Minecraft.h"
#include "../../../renderer/Textures.h"
#include <vector>

namespace Touch {

//
// Games list
//

void AvailableGamesList::selectStart( int item) {
	startSelected = item;
}

void AvailableGamesList::selectCancel() {
	startSelected = -1;
}

void AvailableGamesList::selectItem( int item, bool doubleClick ) {
	LOGI("selected an item! %d\n", item);
	selectedItem = item;
}

void AvailableGamesList::renderItem( int i, int x, int y, int h, Tesselator& t )
{
	if (startSelected == i && Multitouch::getFirstActivePointerIdEx() >= 0) {
		fill((int)x0, y, (int)x1, y+h, 0x809E684F);
	}

	//static int colors[2] = {0xffffb0, 0xcccc90};
	const PingedCompatibleServer& s = copiedServerList[i];
	unsigned int color  = s.isSpecial? 0x6090a0 : 0xffffb0;
	unsigned int color2 = 0xffffa0;//colors[i&1];

	int xx1 = (int)x0 + 24;
	int xx2 = xx1;

	if (s.isSpecial) {
		xx1 += 50;

		glEnable2(GL_TEXTURE_2D);
        glColor4f2(1,1,1,1);
        glEnable2(GL_BLEND);
		minecraft->textures->loadAndBindTexture("gui/badge/minecon140.png");
		blit(xx2, y + 6, 0, 0, 37, 8, 140, 240);
	}

	drawString(minecraft->font, s.name.C_String(), xx1, y + 4 + 2, color);
	drawString(minecraft->font, s.address.ToString(false), xx2, y + 18, color2);

	/*
	drawString(minecraft->font, copiedServerList[i].name.C_String(), (int)x0 + 24, y + 4, color);
	drawString(minecraft->font, copiedServerList[i].address.ToString(false), (int)x0 + 24, y + 18, color);
	*/
}


//
// Join Game screen
//
JoinGameScreen::JoinGameScreen()
:	bJoin(  2, I18n::get("menu.multiplayer")),
	bBack(  3, I18n::get("gui.back")),
	bHeader(0, I18n::get("menu.multiplayer")),
	bMode(4, I18n::get("multiplayer.switchMode")),
	bServer(5, I18n::get("multiplayer.server")),
	gamesList(NULL)
{
	bJoin.active = false;
	//gamesList->yInertia = 0.5f;
}

JoinGameScreen::~JoinGameScreen()
{
	delete gamesList;
}

void JoinGameScreen::init()
{
	//buttons.push_back(&bJoin);
	buttons.push_back(&bBack);
	buttons.push_back(&bHeader);
	buttons.push_back(&bMode);
	buttons.push_back(&bServer);

	minecraft->raknetInstance->clearServerList();
	// Reserve the bottom strip for the mode button and its two description lines.
	gamesList = new AvailableGamesList(minecraft, width, height > 88 ? height - 64 : height);

#ifdef ANDROID
	//tabButtons.push_back(&bJoin);
	tabButtons.push_back(&bBack);
#endif
}

void JoinGameScreen::setupPositions() {
	const int topButtonWidth = width >= 240 ? 88 : width / 3;
	bBack.x = 0;
	bBack.y = 0;
	bBack.width = topButtonWidth;
	bBack.height = 26;
	bServer.width = topButtonWidth;
	bServer.height = 26;
	bServer.x = width - bServer.width;
	bServer.y = 0;
	// Keep the title between Back and the Server button at the top right.
	bHeader.x = bBack.width;
	bHeader.y = 0;
	bHeader.width = width - bBack.width - bServer.width;
	bHeader.height = 26;
	bMode.width = width >= 220 ? 128 : width - 16;
	bMode.height = 24;
	bMode.x = (width - bMode.width) / 2;
	bMode.y = height - 58;
}

void JoinGameScreen::buttonClicked(Button* button)
{
	if (button->id == bMode.id)
	{
		minecraft->setExtendedMultiplayer(!minecraft->isExtendedMultiplayer());
	}
	if (button->id == bServer.id)
		minecraft->screenChooser.setScreen(SCREEN_SERVERCONNECT);
	if (button->id == bJoin.id)
	{
		if (isIndexValid(gamesList->selectedItem))
		{
			PingedCompatibleServer selectedServer = gamesList->copiedServerList[gamesList->selectedItem];
			minecraft->joinMultiplayer(selectedServer);
			{
				bJoin.active = false;
				bBack.active = false;
				minecraft->setScreen(new ProgressScreen());
			}
		}
		//minecraft->locateMultiplayer();
		//minecraft->setScreen(new JoinGameScreen());
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

void JoinGameScreen::tick()
{
	if (isIndexValid(gamesList->selectedItem)) {
		buttonClicked(&bJoin);
		return;
	}

	//gamesList->tick();

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

	bJoin.active = isIndexValid(gamesList->selectedItem);
}

void JoinGameScreen::render( int xm, int ym, float a )
{
	bool hasNetwork = minecraft->platform()->isNetworkEnabled(true);
#if defined(WIN32) && !defined(MCPE_UWP)
	hasNetwork = hasNetwork && !GetAsyncKeyState(VK_TAB);
#endif

	renderBackground();
	if (hasNetwork) gamesList->render(xm, ym, a);
	else gamesList->renderDirtBackground();
	Screen::render(xm, ym, a);

	const int baseX = width / 2;

	if (hasNetwork) {
		std::string s = I18n::get("multiplayer.scanningWifi");
		drawCenteredString(minecraft->font, s, baseX, 30, 0xffffffff);

		const int textWidth = minecraft->font->width(s);
		const int spinnerX = baseX + textWidth / 2 + 6;

		static const char* spinnerTexts[] = {"-", "\\", "|", "/"};
		int n = ((int)(5.5f * getTimeS()) % 4);
		drawCenteredString(minecraft->font, spinnerTexts[n], spinnerX, 30, 0xffffffff);
	} else {
		drawCenteredString(minecraft->font, I18n::get("multiplayer.wifiDisabled"), baseX, 30, 0xffffffff);
	}

	const bool extended = minecraft->isExtendedMultiplayer();
	const std::string modeLine = extended
		? (width < 360 ? I18n::get("multiplayer.extendedMode.short")
			: I18n::get("multiplayer.extendedMode.long"))
		: (width < 360 ? I18n::get("multiplayer.compatibleMode.short")
			: I18n::get("multiplayer.compatibleMode.long"));
	const int modeLineX = (width - minecraft->font->width(modeLine)) / 2;
	minecraft->font->draw(modeLine, (float)modeLineX,
		(float)(height - 26), 0xffc0c0c0);
	drawCenteredString(minecraft->font,
		extended ? (width < 360 ? I18n::get("multiplayer.projectClients.short")
			: I18n::get("multiplayer.projectClients.long"))
			: I18n::get("multiplayer.originalClients"),
		width / 2, height - 12, 0xffc0c0c0);

}

bool JoinGameScreen::isInGameScreen() { return false; }

};
