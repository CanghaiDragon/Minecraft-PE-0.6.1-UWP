#ifndef NET_MINECRAFT_CLIENT_GUI_SCREENS__AchievementsScreen_H__
#define NET_MINECRAFT_CLIENT_GUI_SCREENS__AchievementsScreen_H__

#include "../Screen.h"

class Button;

class AchievementsScreen : public Screen
{
	typedef Screen super;
public:
	AchievementsScreen();
	~AchievementsScreen();
	virtual void init();
	virtual void setupPositions();
	virtual void render(int xm, int ym, float a);
	virtual void mouseClicked(int x, int y, int buttonNum);
	virtual void mouseReleased(int x, int y, int buttonNum);
	virtual void mouseScrolled(int x, int y, int delta);
	virtual bool renderGameBehind();
	virtual bool handleBackEvent(bool isDown);
	virtual void keyPressed(int eventKey);
protected:
	virtual void buttonClicked(Button* button);
private:
	void clampPan();
	int hoveredAchievement(int mouseX, int mouseY) const;
	virtual void mouseMoved(int x, int y);
	Button* doneButton;
	float panX;
	float panY;
	int dragX;
	int dragY;
	int dragStartX;
	int dragStartY;
	bool dragging;
	bool dragMoved;
	int selectedAchievement;
	bool ownsPause;
	bool previousPause;
};

#endif
