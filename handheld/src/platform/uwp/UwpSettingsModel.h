#pragma once

#include <string>

// UWP-side representation of the existing MCPE Options fields.  The key
// names intentionally match OptionStrings so this can be bridged to the
// real OptionsFile by the UWP shell and skin/settings integration.
class UwpSettingsModel final
{
public:
	std::wstring username;
	int difficulty;
	bool thirdPersonView;
	bool serverVisible;

	int inputMode;
	float sensitivity;
	int dpadSize;
	bool leftHanded;
	bool splitTouchControls;
	bool invertMouse;
	bool destroyVibration;

	bool fancyGraphics;
	bool ambientOcclusion;
	int renderDistance;
	int guiScale;
	bool viewBobbing;
	float fieldOfView;
	bool hideGui;

	float music;
	float sound;

	bool infiniteWorlds;
	bool skinMenu;
	bool touchSneak;
	bool betaVisuals;
	bool debugScreen;
	int skinArmType;

	UwpSettingsModel();

	void load();
	void save() const;

	void clamp();
};
