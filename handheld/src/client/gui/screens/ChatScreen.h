#ifndef NET_MINECRAFT_CLIENT_GUI_SCREENS__ChatScreen_H__
#define NET_MINECRAFT_CLIENT_GUI_SCREENS__ChatScreen_H__

#include "../Screen.h"
#include "../components/Button.h"
#include "../components/TextBox.h"

class ChatScreen: public Screen
{
public:
	ChatScreen();
	virtual ~ChatScreen();

	void init();
	void removed();
	void setupPositions();

	void render(int xm, int ym, float a);
	void buttonClicked(Button* button);
	void keyPressed(int eventKey);
	bool handleBackEvent(bool isDown);
	bool isPauseScreen();
	bool isInGameScreen();
	bool shouldSleepAfterRender();

private:
	void submitMessage();
	Button* bCancel;
	Button* bSend;
	TextBox messageBox;
};

#endif /*NET_MINECRAFT_CLIENT_GUI_SCREENS__ChatScreen_H__*/
