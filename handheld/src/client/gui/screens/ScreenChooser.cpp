#include "ScreenChooser.h"
#include "StartMenuScreen.h"
#include "SelectWorldScreen.h"
#include "JoinGameScreen.h"
#include "PauseScreen.h"
#include "RenameMPLevelScreen.h"
#include "IngameBlockSelectionScreen.h"
#include "touch/TouchStartMenuScreen.h"
#include "touch/TouchSelectWorldScreen.h"
#include "touch/TouchJoinGameScreen.h"
#include "touch/TouchIngameBlockSelectionScreen.h"

#include "../../Minecraft.h"

Screen* ScreenChooser::createScreen( ScreenId id )
{
	Screen* screen = NULL;

	// UI presentation is deliberately independent from input mode. Java UI is
	// opt-in; the default is the PE/touch presentation even with a mouse and
	// keyboard.
	const bool useJavaUi = _mc->options.javaUi;

	switch (id) {
	case SCREEN_STARTMENU:
		screen = useJavaUi ? static_cast<Screen*>(new StartMenuScreen())
		                   : static_cast<Screen*>(new Touch::StartMenuScreen());
		break;
	case SCREEN_SELECTWORLD:
		screen = useJavaUi ? static_cast<Screen*>(new SelectWorldScreen())
		                   : static_cast<Screen*>(new Touch::SelectWorldScreen());
		break;
	case SCREEN_JOINGAME:
		screen = useJavaUi ? static_cast<Screen*>(new JoinGameScreen())
		                        : static_cast<Screen*>(new Touch::JoinGameScreen());
		break;
	case SCREEN_PAUSE:
		screen = new PauseScreen(false);
		break;
	case SCREEN_PAUSEPREV:
		screen = new PauseScreen(true);
		break;
	case SCREEN_BLOCKSELECTION:
		screen = useJavaUi ? static_cast<Screen*>(new IngameBlockSelectionScreen())
		                   : static_cast<Screen*>(new Touch::IngameBlockSelectionScreen());
		break;

	case SCREEN_NONE:
	default:
		// Do nothing
		break;
	}
	return screen;
}

Screen* ScreenChooser::setScreen(ScreenId id)
{
	Screen* screen = createScreen(id);
	_mc->setScreen(screen);
	return screen;
}
