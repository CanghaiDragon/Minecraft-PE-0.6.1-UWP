#include "pch.h"
#include "UwpSettingsModel.h"

#include <windows.foundation.h>
#include <windows.storage.h>

using namespace Platform;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::Storage;

namespace
{
Windows::Foundation::Collections::IPropertySet^ values()
{
	return ApplicationData::Current->LocalSettings->Values;
}

String^ key(const wchar_t* name)
{
	return ref new String(name);
}

void loadBool(IPropertySet^ store, const wchar_t* name, bool& value)
{
	if (store->HasKey(key(name)))
		value = safe_cast<bool>(store->Lookup(key(name)));
}

void loadInt(IPropertySet^ store, const wchar_t* name, int& value)
{
	if (store->HasKey(key(name)))
		value = safe_cast<int>(store->Lookup(key(name)));
}

void loadFloat(IPropertySet^ store, const wchar_t* name, float& value)
{
	if (store->HasKey(key(name)))
		value = static_cast<float>(safe_cast<double>(store->Lookup(key(name))));
}

void loadString(IPropertySet^ store, const wchar_t* name, std::wstring& value)
{
	if (store->HasKey(key(name)))
	{
		String^ stored = safe_cast<String^>(store->Lookup(key(name)));
		if (stored != nullptr)
			value.assign(stored->Data());
	}
}

void saveBool(IPropertySet^ store, const wchar_t* name, bool value)
{
	store->Insert(key(name), PropertyValue::CreateBoolean(value));
}

void saveInt(IPropertySet^ store, const wchar_t* name, int value)
{
	store->Insert(key(name), PropertyValue::CreateInt32(value));
}

void saveFloat(IPropertySet^ store, const wchar_t* name, float value)
{
	store->Insert(key(name), PropertyValue::CreateDouble(static_cast<double>(value)));
}

void saveString(IPropertySet^ store, const wchar_t* name, const std::wstring& value)
{
	store->Insert(key(name), ref new String(value.c_str()));
}

int clampInt(int value, int minimum, int maximum)
{
	return value < minimum ? minimum : (value > maximum ? maximum : value);
}

float clampFloat(float value, float minimum, float maximum)
{
	return value < minimum ? minimum : (value > maximum ? maximum : value);
}
}

UwpSettingsModel::UwpSettingsModel()
	: username(L"Steve"),
	  difficulty(2),
	  thirdPersonView(false),
	  serverVisible(true),
	  inputMode(1),
	  sensitivity(0.5f),
	  dpadSize(0),
	  leftHanded(false),
	  splitTouchControls(false),
	  invertMouse(false),
	  destroyVibration(true),
	  fancyGraphics(true),
	  ambientOcclusion(false),
	  renderDistance(0),
	  guiScale(0),
	  viewBobbing(true),
	  fieldOfView(70.0f),
	  hideGui(false),
	  music(1.0f),
	  sound(1.0f),
	  infiniteWorlds(false),
	  skinMenu(false),
	  touchSneak(false),
	  betaVisuals(false),
	  debugScreen(false),
	  skinArmType(0)
{
}

void UwpSettingsModel::clamp()
{
	// Options::DIFFICULY_LEVELS contains only 0 (Peaceful) and 2 (Normal).
	difficulty = difficulty == 0 ? 0 : 2;
	inputMode = clampInt(inputMode, 0, 1);
	dpadSize = clampInt(dpadSize, 0, 2);
	renderDistance = clampInt(renderDistance, 0, 3);
	guiScale = clampInt(guiScale, 0, 4);
	skinArmType = clampInt(skinArmType, 0, 1);
	sensitivity = clampFloat(sensitivity, 0.0f, 1.0f);
	music = clampFloat(music, 0.0f, 1.0f);
	sound = clampFloat(sound, 0.0f, 1.0f);
	fieldOfView = clampFloat(fieldOfView, 30.0f, 110.0f);
}

void UwpSettingsModel::load()
{
	IPropertySet^ store = values();
	loadString(store, L"mp_username", username);
	loadInt(store, L"game_difficulty", difficulty);
	loadBool(store, L"game_thirdperson", thirdPersonView);
	loadBool(store, L"mp_server_visible_default", serverVisible);

	bool useTouchScreen = inputMode != 0;
	loadBool(store, L"ctrl_usetouchscreen", useTouchScreen);
	inputMode = useTouchScreen ? 1 : 0;
	loadFloat(store, L"ctrl_sensitivity", sensitivity);
	loadInt(store, L"ctrl_dpad_size", dpadSize);
	loadBool(store, L"ctrl_islefthanded", leftHanded);
	loadBool(store, L"ctrl_usetouchjoypad", splitTouchControls);
	loadBool(store, L"ctrl_invertmouse", invertMouse);
	loadBool(store, L"feedback_vibration", destroyVibration);

	loadBool(store, L"gfx_fancygraphics", fancyGraphics);
	loadBool(store, L"gfx_ao", ambientOcclusion);
	loadInt(store, L"gfx_renderdistance", renderDistance);
	loadInt(store, L"gfx_guiscale", guiScale);
	loadBool(store, L"gfx_viewbobbing", viewBobbing);
	loadFloat(store, L"gfx_fov", fieldOfView);
	loadBool(store, L"game_hidegui", hideGui);

	loadFloat(store, L"audio_music", music);
	loadFloat(store, L"audio_sound", sound);

	loadBool(store, L"extra_infinite_worlds", infiniteWorlds);
	loadBool(store, L"skin_menu", skinMenu);
	loadBool(store, L"touch_sneak", touchSneak);
	loadBool(store, L"gfx_beta_visuals", betaVisuals);
	loadBool(store, L"extra_debug_screen", debugScreen);
	loadInt(store, L"skin_arm_type", skinArmType);
	clamp();
}

void UwpSettingsModel::save() const
{
	UwpSettingsModel normalized = *this;
	normalized.clamp();
	IPropertySet^ store = values();
	saveString(store, L"mp_username", normalized.username);
	saveInt(store, L"game_difficulty", normalized.difficulty);
	saveBool(store, L"game_thirdperson", normalized.thirdPersonView);
	saveBool(store, L"mp_server_visible_default", normalized.serverVisible);

	saveBool(store, L"ctrl_usetouchscreen", normalized.inputMode != 0);
	saveFloat(store, L"ctrl_sensitivity", normalized.sensitivity);
	saveInt(store, L"ctrl_dpad_size", normalized.dpadSize);
	saveBool(store, L"ctrl_islefthanded", normalized.leftHanded);
	saveBool(store, L"ctrl_usetouchjoypad", normalized.splitTouchControls);
	saveBool(store, L"ctrl_invertmouse", normalized.invertMouse);
	saveBool(store, L"feedback_vibration", normalized.destroyVibration);

	saveBool(store, L"gfx_fancygraphics", normalized.fancyGraphics);
	saveBool(store, L"gfx_ao", normalized.ambientOcclusion);
	saveInt(store, L"gfx_renderdistance", normalized.renderDistance);
	saveInt(store, L"gfx_guiscale", normalized.guiScale);
	saveBool(store, L"gfx_viewbobbing", normalized.viewBobbing);
	saveFloat(store, L"gfx_fov", normalized.fieldOfView);
	saveBool(store, L"game_hidegui", normalized.hideGui);

	saveFloat(store, L"audio_music", normalized.music);
	saveFloat(store, L"audio_sound", normalized.sound);

	saveBool(store, L"extra_infinite_worlds", normalized.infiniteWorlds);
	saveBool(store, L"skin_menu", normalized.skinMenu);
	saveBool(store, L"touch_sneak", normalized.touchSneak);
	saveBool(store, L"gfx_beta_visuals", normalized.betaVisuals);
	saveBool(store, L"extra_debug_screen", normalized.debugScreen);
	saveInt(store, L"skin_arm_type", normalized.skinArmType);
}
