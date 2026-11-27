#ifndef NET_MINECRAFT_CLIENT_GUI_SCREENS__JoinGameScreen_H__
#define NET_MINECRAFT_CLIENT_GUI_SCREENS__JoinGameScreen_H__

#include <string>
#include "../Screen.h"
#include "../components/Button.h"
#include "../components/SmallButton.h"
#include "../components/TextBox.h"
#include "../components/ScrolledSelectionList.h"
#include "../../Minecraft.h"
#include "../../../network/RakNetInstance.h"


class JoinGameScreen;
class OptionButton;

class AvailableGamesList : public ScrolledSelectionList
{
	int selectedItem;
	ServerList copiedServerList;

	friend class JoinGameScreen;

public:

	AvailableGamesList(Minecraft* _minecraft, int _width, int _height)
	:	ScrolledSelectionList(_minecraft, _width, _height, 24, _height > 82 ? _height - 58 : 24, 28),
		selectedItem(-1)
	{
	}

protected:

	virtual int getNumberOfItems() { return (int)copiedServerList.size(); }

	virtual void selectItem(int item, bool doubleClick);
	virtual bool isSelectedItem(int item) { return item == selectedItem; }

	virtual void renderBackground() {}
	virtual void renderItem(int i, int x, int y, int h, Tesselator& t)
	{
		const PingedCompatibleServer& s = copiedServerList[i];
		unsigned int color = s.isSpecial? 0xff00b0 : 0xffffa0;
		drawString(minecraft->font, s.name.C_String(), x, y + 2, color);
		drawString(minecraft->font, s.address.ToString(false), x, y + 16, 0xffffa0);
	}
};

class JoinGameScreen: public Screen
{
public:
	JoinGameScreen();
	virtual ~JoinGameScreen();

	void init();
	void setupPositions();

	virtual bool handleBackEvent(bool isDown);

	virtual bool isIndexValid(int index);

	virtual void tick();

	void render(int xm, int ym, float a);

	void buttonClicked(Button* button);
	virtual void mouseScrolled(int x, int y, int delta) {
		if (gamesList != NULL) gamesList->mouseScrolled(delta);
	}

	bool isInGameScreen();
private:
	Button bBack;
	Button bMode;
	Button bServer;
	AvailableGamesList* gamesList;
	void joinSelectedServer();
};

// Manual address entry is shared by Java and PE layouts; button styles are
// selected at runtime to match the active UI.
class DirectConnectScreen: public Screen
{
public:
	DirectConnectScreen();
	virtual ~DirectConnectScreen();
	void init();
	void setupPositions();
	void tick();
	void render(int xm, int ym, float a);
	void buttonClicked(Button* button);
	void keyPressed(int eventKey);
	bool handleBackEvent(bool isDown);
	bool isInGameScreen();
private:
	Button* bBack;
	Button* bTitle;
	Button* bConnect;
	OptionButton* bPe08Recipes;
	TextBox addressBox;
	TextBox portBox;
	std::string errorMessage;
};

#endif /*NET_MINECRAFT_CLIENT_GUI_SCREENS__JoinGameScreen_H__*/
