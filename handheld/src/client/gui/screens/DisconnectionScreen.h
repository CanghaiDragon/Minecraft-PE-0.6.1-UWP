#ifndef NET_MINECRAFT_CLIENT_GUI_SCREENS__DisconnectionScreen_H__
#define NET_MINECRAFT_CLIENT_GUI_SCREENS__DisconnectionScreen_H__

#include "../Screen.h"
#include "../Font.h"
#include "../components/Button.h"
#include "../../Minecraft.h"
#include "../../../network/RakNetInstance.h"
#include <string>

class DisconnectionScreen: public Screen
{
	typedef Screen super;
public:
	DisconnectionScreen(const std::string& msg)
	:	_msg(msg),
		_back(NULL)
	{}

	~DisconnectionScreen() {
		delete _back;
	}

	void init() {
		if (minecraft->options.javaUi)
			_back = new Button(1, "Ok");
		else
			_back = new Touch::TButton(1, "Ok");

		buttons.push_back(_back);
		tabButtons.push_back(_back);

		_back->width = minecraft->options.javaUi ? 200 : 128;
		_back->height = minecraft->options.javaUi ? 20 : 26;
		_back->x = (width - _back->width) / 2;
		_back->y = height / 2 + (minecraft->options.javaUi ? 16 : 8);
	}

	void render( int xm, int ym, float a ) {
		renderBackground();
		super::render(xm, ym, a);

		int center = (width - minecraft->font->width(_msg)) / 2;
		minecraft->font->drawShadow(_msg, (float)center, (float)(height / 2 - 24), 0xffffffff);
	}

	void buttonClicked(Button* button) {
		if (button->id == _back->id) {
			if (minecraft->level != NULL) {
				minecraft->leaveGame();
			} else {
				if (minecraft->raknetInstance != NULL)
					minecraft->raknetInstance->disconnect();
				minecraft->cancelLocateMultiplayer();
				minecraft->screenChooser.setScreen(SCREEN_STARTMENU);
			}
		}
	}
	bool isInGameScreen() { return false; }

private:
	std::string _msg;
	Button* _back;
};

#endif /*NET_MINECRAFT_CLIENT_GUI_SCREENS__DisconnectionScreen_H__*/
