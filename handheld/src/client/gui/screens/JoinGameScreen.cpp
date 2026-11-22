#include "JoinGameScreen.h"
#include "StartMenuScreen.h"
#include "ProgressScreen.h"
#include "../Font.h"
#include "../components/TextBox.h"
#include "../../../network/RakNetInstance.h"
#include "../../../locale/I18n.h"
#include "../../../platform/input/Keyboard.h"
#include <cctype>
#include <vector>

JoinGameScreen::JoinGameScreen()
:	bBack(  3, I18n::get("gui.back")),
	bMode(  4, I18n::get("multiplayer.switchMode")),
	bServer(5, I18n::get("multiplayer.server")),
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
	if (button->id == bServer.id)
		minecraft->screenChooser.setScreen(SCREEN_SERVERCONNECT);
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
	buttons.push_back(&bServer);

	minecraft->raknetInstance->clearServerList();
	gamesList = new AvailableGamesList(minecraft, width, height);

#ifdef ANDROID
	tabButtons.push_back(&bBack);
#endif
}

void JoinGameScreen::setupPositions() {
	const int buttonWidth = (width - 32) / 3;
	const int yBase = height - 26;
	bBack.width = bMode.width = bServer.width = buttonWidth;
	bBack.height = bMode.height = bServer.height = 20;
	bBack.x = 8;
	bMode.x = (width - buttonWidth) / 2;
	bServer.x = width - buttonWidth - 8;
	bBack.y = bMode.y = bServer.y = yBase;
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

DirectConnectScreen::DirectConnectScreen()
:	bBack(NULL),
	bTitle(NULL),
	bConnect(NULL),
	addressBox(1, I18n::get("multiplayer.serverAddress.hint")),
	portBox(2, I18n::get("multiplayer.serverPort.hint"))
{
}

DirectConnectScreen::~DirectConnectScreen()
{
	delete bBack;
	delete bTitle;
	delete bConnect;
}

void DirectConnectScreen::init()
{
	addressBox.text = minecraft->options.lastServerAddress;
	portBox.text = std::to_string(minecraft->options.lastServerPort);
	const bool javaUi = minecraft->options.javaUi;
	if (javaUi) {
		bBack = new Button(10, I18n::get("gui.back"));
		bConnect = new Button(12, I18n::get("multiplayer.connect"));
	} else {
		bBack = new Touch::TButton(10, I18n::get("gui.back"));
		bTitle = new Touch::THeader(11, I18n::get("multiplayer.server.title"));
		bConnect = new Touch::TButton(12, I18n::get("multiplayer.connect"));
		buttons.push_back(bTitle);
	}
	buttons.push_back(bBack);
	buttons.push_back(bConnect);
	textBoxes.push_back(&addressBox);
	textBoxes.push_back(&portBox);
	tabButtons.push_back(bBack);
	tabButtons.push_back(bConnect);
}

void DirectConnectScreen::setupPositions()
{
	const bool javaUi = minecraft->options.javaUi;
	const int headerButtonWidth = width >= 240 ? 88 : width / 3;
	bBack->x = 0;
	bBack->y = 0;
	bBack->width = headerButtonWidth;
	bBack->height = javaUi ? 20 : 26;
	if (bTitle != NULL) {
		bTitle->x = headerButtonWidth;
		bTitle->y = 0;
		bTitle->width = width - headerButtonWidth;
		bTitle->height = 26;
	}

	const int fieldWidth = width > 40 ? width - 32 : width;
	int addressY = height / 2 - 47;
	if (addressY < 34) addressY = 34;
	addressBox.x = (width - fieldWidth) / 2;
	addressBox.y = addressY;
	addressBox.width = fieldWidth;
	addressBox.height = 24;
	portBox.width = width >= 200 ? 112 : width - 32;
	portBox.height = 24;
	portBox.x = (width - portBox.width) / 2;
	portBox.y = addressY + 48;

	bConnect->width = width >= 200 ? 128 : width - 24;
	bConnect->height = javaUi ? 20 : 26;
	bConnect->x = (width - bConnect->width) / 2;
	bConnect->y = height - bConnect->height - 4;
}

void DirectConnectScreen::tick()
{
	addressBox.tick(minecraft);
	portBox.tick(minecraft);
}

void DirectConnectScreen::render(int xm, int ym, float a)
{
	renderBackground();
	Screen::render(xm, ym, a);
	if (bTitle == NULL)
		drawCenteredString(minecraft->font,
			I18n::get("multiplayer.server.title"), width / 2, 8, 0xffffffff);
	drawString(minecraft->font, I18n::get("multiplayer.serverAddress"),
		addressBox.x, addressBox.y - minecraft->font->lineHeight - 2, 0xffffffff);
	drawString(minecraft->font, I18n::get("multiplayer.serverPort"),
		portBox.x, portBox.y - minecraft->font->lineHeight - 2, 0xffffffff);
	if (errorMessage.empty()) {
		drawCenteredString(minecraft->font,
			I18n::get("multiplayer.server.compatible"), width / 2,
			portBox.y + portBox.height + 4, 0xffc0c0c0);
	} else {
		drawCenteredString(minecraft->font, errorMessage, width / 2,
			bConnect->y - minecraft->font->lineHeight - 4, 0xffff6060);
	}
}

void DirectConnectScreen::buttonClicked(Button* button)
{
	if (button == bBack) {
		minecraft->cancelLocateMultiplayer();
		minecraft->screenChooser.setScreen(SCREEN_JOINGAME);
		minecraft->locateMultiplayer();
		return;
	}
	if (button != bConnect)
		return;

	std::string host = addressBox.text;
	while (!host.empty() && std::isspace(static_cast<unsigned char>(host[0])))
		host.erase(host.begin());
	while (!host.empty() && std::isspace(static_cast<unsigned char>(host[host.size() - 1])))
		host.erase(host.size() - 1);
	if (host.empty() || host.find_first_of(" \t\r\n") != std::string::npos) {
		errorMessage = I18n::get("multiplayer.server.invalidAddress");
		return;
	}

	int port = 19132;
	if (!portBox.text.empty()) {
		port = 0;
		for (std::string::const_iterator it = portBox.text.begin();
			it != portBox.text.end(); ++it) {
			if (*it < '0' || *it > '9') {
				errorMessage = I18n::get("multiplayer.server.invalidPort");
				return;
			}
			const int digit = *it - '0';
			if (port > (65535 - digit) / 10) {
				errorMessage = I18n::get("multiplayer.server.invalidPort");
				return;
			}
			port = port * 10 + digit;
		}
		if (port < 1) {
			errorMessage = I18n::get("multiplayer.server.invalidPort");
			return;
		}
	}

	errorMessage.clear();
	if (minecraft->joinMultiplayer(host, port, true))
		minecraft->setScreen(new ProgressScreen());
	else
		errorMessage = I18n::get("multiplayer.server.connectFailed");
}

void DirectConnectScreen::keyPressed(int eventKey)
{
	if (eventKey == Keyboard::KEY_ESCAPE) {
		buttonClicked(bBack);
		return;
	}
	if (eventKey == Keyboard::KEY_RETURN) {
		buttonClicked(bConnect);
		return;
	}
	Screen::keyPressed(eventKey);
}

bool DirectConnectScreen::handleBackEvent(bool isDown)
{
	if (!isDown)
		buttonClicked(bBack);
	return true;
}

bool DirectConnectScreen::isInGameScreen() { return false; }
