#include "pch.h"
#include "SettingsPage.xaml.h"
#include <cstdint>
#include <string>
using namespace MCPEUWP;
using namespace Platform;
using namespace Windows::System;
using namespace Windows::UI::Xaml::Input;
using namespace Windows::UI::Xaml::Controls;
SettingsPage::SettingsPage() { InitializeComponent(); }

void SettingsPage::SetClientHandle(unsigned long long handleValue)
{
	m_clientHandle = reinterpret_cast<UwpOriginalClientHandle>(
		static_cast<uintptr_t>(handleValue));
	RefreshFromOriginalOptions();
}

namespace
{
	int ClampIndex(int value, int maximum)
	{
		if (value < 0)
			return 0;
		return value > maximum ? maximum : value;
	}

	double AsPercent(float value)
	{
		if (value < 0.0f)
			value = 0.0f;
		if (value > 1.0f)
			value = 1.0f;
		return static_cast<double>(value) * 100.0;
	}

	int RenderDistanceIndex(int value)
	{
		// Original values are Far=0, Normal=1, Short=2, Tiny=3.
		if (value <= 0)
			return 3;
		if (value == 1)
			return 2;
		if (value == 2)
			return 1;
		return 0;
	}

	int OriginalRenderDistanceValue(int index)
	{
		// XAML values are Tiny=0, Short=1, Normal=2, Far=3.
		if (index <= 0)
			return 3;
		if (index == 1)
			return 2;
		if (index == 2)
			return 1;
		return 0;
	}

	bool CheckBoxValue(CheckBox^ checkBox)
	{
		return checkBox != nullptr && checkBox->IsChecked != nullptr
			&& checkBox->IsChecked->Value;
	}
}

void SettingsPage::RefreshFromOriginalOptions()
{
	if (m_clientHandle == nullptr)
		return;
	m_refreshingOptions = true;

	const auto getBool = [this](UwpOptionId id)
	{
		return UwpOriginalClientGetOptionBool(m_clientHandle, id);
	};
	const auto getInt = [this](UwpOptionId id)
	{
		return UwpOriginalClientGetOptionInt(m_clientHandle, id);
	};
	const auto getFloat = [this](UwpOptionId id)
	{
		return UwpOriginalClientGetOptionFloat(m_clientHandle, id);
	};

	if (UsernameBox != nullptr)
	{
		const std::string username = UwpOriginalClientGetUsername(m_clientHandle);
		const std::wstring wideUsername(username.begin(), username.end());
		UsernameBox->Text = ref new String(wideUsername.c_str());
	}
	if (DifficultyComboBox != nullptr)
		DifficultyComboBox->SelectedIndex = getInt(UwpOptionId::Difficulty) == 0 ? 0 : 1;
	if (CameraComboBox != nullptr)
		CameraComboBox->SelectedIndex = ClampIndex(
			getInt(UwpOptionId::ThirdPerson), 2);
	if (ServerVisibleCheckBox != nullptr)
		ServerVisibleCheckBox->IsChecked = getBool(UwpOptionId::ServerVisible);
	if (DynamicTileComboBox != nullptr)
	{
		int dynamicTileMode = getInt(UwpOptionId::DynamicTile);
		if (dynamicTileMode < 0) dynamicTileMode = 0;
		if (dynamicTileMode > 2) dynamicTileMode = 2;
		DynamicTileComboBox->SelectedIndex = dynamicTileMode;
	}

	if (InputModeComboBox != nullptr)
		InputModeComboBox->SelectedIndex = ClampIndex(
			getInt(UwpOptionId::InputMode), 1);
	if (SensitivitySlider != nullptr)
		SensitivitySlider->Value = AsPercent(getFloat(UwpOptionId::Sensitivity));
	if (DpadSizeComboBox != nullptr)
		DpadSizeComboBox->SelectedIndex = ClampIndex(
			getInt(UwpOptionId::DpadSize), 2);
	if (LeftHandedCheckBox != nullptr)
		LeftHandedCheckBox->IsChecked = getBool(UwpOptionId::LeftHanded);
	if (SplitTouchCheckBox != nullptr)
		SplitTouchCheckBox->IsChecked = getBool(UwpOptionId::SplitTouch);
	if (InvertMouseCheckBox != nullptr)
		InvertMouseCheckBox->IsChecked = getBool(UwpOptionId::InvertMouse);
	if (DestroyVibrationCheckBox != nullptr)
		DestroyVibrationCheckBox->IsChecked = getBool(UwpOptionId::DestroyVibration);

	if (GraphicsModeComboBox != nullptr)
		GraphicsModeComboBox->SelectedIndex = getInt(UwpOptionId::FancyGraphics) != 0 ? 0 : 1;
	if (AmbientOcclusionCheckBox != nullptr)
		AmbientOcclusionCheckBox->IsChecked = getBool(UwpOptionId::AmbientOcclusion);
	if (RenderDistanceComboBox != nullptr)
		RenderDistanceComboBox->SelectedIndex = RenderDistanceIndex(
			getInt(UwpOptionId::RenderDistance));
	if (GuiScaleComboBox != nullptr)
		GuiScaleComboBox->SelectedIndex = ClampIndex(
			getInt(UwpOptionId::GuiScale), 4);
	if (FieldOfViewSlider != nullptr)
		FieldOfViewSlider->Value = getFloat(UwpOptionId::FieldOfView);
	if (ViewBobbingCheckBox != nullptr)
		ViewBobbingCheckBox->IsChecked = getBool(UwpOptionId::ViewBobbing);
	if (HideGuiCheckBox != nullptr)
		HideGuiCheckBox->IsChecked = getBool(UwpOptionId::HideGui);

	if (MusicSlider != nullptr)
		MusicSlider->Value = AsPercent(getFloat(UwpOptionId::Music));
	if (SoundSlider != nullptr)
		SoundSlider->Value = AsPercent(getFloat(UwpOptionId::Sound));

	if (InfiniteWorldsCheckBox != nullptr)
		InfiniteWorldsCheckBox->IsChecked = getBool(UwpOptionId::InfiniteWorlds);
	if (JavaUiCheckBox != nullptr)
		JavaUiCheckBox->IsChecked = getBool(UwpOptionId::JavaUi);
	if (SkinMenuCheckBox != nullptr)
		SkinMenuCheckBox->IsChecked = getBool(UwpOptionId::SkinMenu);
	if (TouchSneakCheckBox != nullptr)
		TouchSneakCheckBox->IsChecked = getBool(UwpOptionId::TouchSneak);
	if (BetaVisualsCheckBox != nullptr)
		BetaVisualsCheckBox->IsChecked = getBool(UwpOptionId::BetaVisuals);
	if (DebugScreenCheckBox != nullptr)
		DebugScreenCheckBox->IsChecked = getBool(UwpOptionId::DebugScreen);
	m_refreshingOptions = false;
}

void SettingsPage::OnUsernameLostFocus(Object^ sender, Windows::UI::Xaml::RoutedEventArgs^)
{
	if (m_clientHandle == nullptr || m_refreshingOptions)
		return;
	TextBox^ textBox = safe_cast<TextBox^>(sender);
	if (textBox == nullptr || textBox->Text == nullptr)
		return;
	const std::wstring username(textBox->Text->Data());
	UwpOriginalClientSetUsername(m_clientHandle,
		std::string(username.begin(), username.end()));
}

void SettingsPage::OnOptionSelectionChanged(
	Object^ sender, Windows::UI::Xaml::Controls::SelectionChangedEventArgs^)
{
	if (m_clientHandle == nullptr || m_refreshingOptions)
		return;
	ComboBox^ comboBox = safe_cast<ComboBox^>(sender);
	if (comboBox == nullptr || comboBox->SelectedIndex < 0)
		return;

	const int index = comboBox->SelectedIndex;
	if (comboBox == DifficultyComboBox)
		UwpOriginalClientSetOptionInt(m_clientHandle, UwpOptionId::Difficulty,
			index == 0 ? 0 : 2);
	else if (comboBox == CameraComboBox)
		UwpOriginalClientSetOptionInt(m_clientHandle, UwpOptionId::ThirdPerson, index);
	else if (comboBox == InputModeComboBox)
		UwpOriginalClientSetOptionInt(m_clientHandle, UwpOptionId::InputMode, index);
	else if (comboBox == DpadSizeComboBox)
		UwpOriginalClientSetOptionInt(m_clientHandle, UwpOptionId::DpadSize, index);
	else if (comboBox == GraphicsModeComboBox)
		UwpOriginalClientSetOptionInt(m_clientHandle, UwpOptionId::FancyGraphics,
			index == 0 ? 1 : 0);
	else if (comboBox == RenderDistanceComboBox)
		UwpOriginalClientSetOptionInt(m_clientHandle, UwpOptionId::RenderDistance,
			OriginalRenderDistanceValue(index));
	else if (comboBox == GuiScaleComboBox)
		UwpOriginalClientSetOptionInt(m_clientHandle, UwpOptionId::GuiScale, index);
	else if (comboBox == DynamicTileComboBox)
		UwpOriginalClientSetOptionInt(m_clientHandle, UwpOptionId::DynamicTile, index);
}

void SettingsPage::OnOptionSliderChanged(
	Object^ sender,
	Windows::UI::Xaml::Controls::Primitives::RangeBaseValueChangedEventArgs^ e)
{
	if (m_clientHandle == nullptr || m_refreshingOptions || e == nullptr)
		return;
	Slider^ slider = safe_cast<Slider^>(sender);
	if (slider == nullptr)
		return;

	const float value = static_cast<float>(e->NewValue);
	if (slider == SensitivitySlider || slider == MusicSlider || slider == SoundSlider)
	{
		const float normalized = value / 100.0f;
		if (slider == SensitivitySlider)
			UwpOriginalClientSetOptionFloat(m_clientHandle, UwpOptionId::Sensitivity, normalized);
		else if (slider == MusicSlider)
			UwpOriginalClientSetOptionFloat(m_clientHandle, UwpOptionId::Music, normalized);
		else
			UwpOriginalClientSetOptionFloat(m_clientHandle, UwpOptionId::Sound, normalized);
	}
	else if (slider == FieldOfViewSlider)
		UwpOriginalClientSetOptionFloat(m_clientHandle, UwpOptionId::FieldOfView, value);
}

void SettingsPage::OnOptionCheckBoxChanged(
	Object^ sender, Windows::UI::Xaml::RoutedEventArgs^)
{
	if (m_clientHandle == nullptr || m_refreshingOptions)
		return;
	CheckBox^ checkBox = safe_cast<CheckBox^>(sender);
	if (checkBox == nullptr)
		return;

	const bool value = CheckBoxValue(checkBox);
	if (checkBox == ServerVisibleCheckBox)
		UwpOriginalClientSetOptionBool(m_clientHandle, UwpOptionId::ServerVisible, value);
	else if (checkBox == LeftHandedCheckBox)
		UwpOriginalClientSetOptionBool(m_clientHandle, UwpOptionId::LeftHanded, value);
	else if (checkBox == SplitTouchCheckBox)
		UwpOriginalClientSetOptionBool(m_clientHandle, UwpOptionId::SplitTouch, value);
	else if (checkBox == InvertMouseCheckBox)
		UwpOriginalClientSetOptionBool(m_clientHandle, UwpOptionId::InvertMouse, value);
	else if (checkBox == DestroyVibrationCheckBox)
		UwpOriginalClientSetOptionBool(m_clientHandle, UwpOptionId::DestroyVibration, value);
	else if (checkBox == AmbientOcclusionCheckBox)
		UwpOriginalClientSetOptionBool(m_clientHandle, UwpOptionId::AmbientOcclusion, value);
	else if (checkBox == ViewBobbingCheckBox)
		UwpOriginalClientSetOptionBool(m_clientHandle, UwpOptionId::ViewBobbing, value);
	else if (checkBox == HideGuiCheckBox)
		UwpOriginalClientSetOptionBool(m_clientHandle, UwpOptionId::HideGui, value);
	else if (checkBox == InfiniteWorldsCheckBox)
		UwpOriginalClientSetOptionBool(m_clientHandle, UwpOptionId::InfiniteWorlds, value);
	else if (checkBox == JavaUiCheckBox)
		UwpOriginalClientSetOptionBool(m_clientHandle, UwpOptionId::JavaUi, value);
	else if (checkBox == SkinMenuCheckBox)
		UwpOriginalClientSetOptionBool(m_clientHandle, UwpOptionId::SkinMenu, value);
	else if (checkBox == TouchSneakCheckBox)
		UwpOriginalClientSetOptionBool(m_clientHandle, UwpOptionId::TouchSneak, value);
	else if (checkBox == BetaVisualsCheckBox)
		UwpOriginalClientSetOptionBool(m_clientHandle, UwpOptionId::BetaVisuals, value);
	else if (checkBox == DebugScreenCheckBox)
		UwpOriginalClientSetOptionBool(m_clientHandle, UwpOptionId::DebugScreen, value);
}

void SettingsPage::OnBackClick(Object^, Windows::UI::Xaml::RoutedEventArgs^)
{
	BackRequested(this, nullptr);
}

void SettingsPage::OnRestoreDefaultsClick(
	Object^, Windows::UI::Xaml::RoutedEventArgs^)
{
	if (m_clientHandle != nullptr)
		UwpOriginalClientRestoreDefaultOptions(m_clientHandle);
	BackRequested(this, nullptr);
}

void SettingsPage::OnPageKeyDown(Object^, KeyRoutedEventArgs^ e)
{
	if (e != nullptr && e->Key == VirtualKey::Escape)
	{
		BackRequested(this, nullptr);
		e->Handled = true;
	}
}

void SettingsPage::ShowCategory(Border^ panel)
{
	GamePanel->Visibility = Windows::UI::Xaml::Visibility::Collapsed;
	ControlsPanel->Visibility = Windows::UI::Xaml::Visibility::Collapsed;
	GraphicsPanel->Visibility = Windows::UI::Xaml::Visibility::Collapsed;
	AudioPanel->Visibility = Windows::UI::Xaml::Visibility::Collapsed;
	AdditionalPanel->Visibility = Windows::UI::Xaml::Visibility::Collapsed;
	if (panel != nullptr)
		panel->Visibility = Windows::UI::Xaml::Visibility::Visible;
}

void SettingsPage::OnGameCategoryClick(Object^, Windows::UI::Xaml::RoutedEventArgs^) { ShowCategory(GamePanel); }
void SettingsPage::OnControlsCategoryClick(Object^, Windows::UI::Xaml::RoutedEventArgs^) { ShowCategory(ControlsPanel); }
void SettingsPage::OnGraphicsCategoryClick(Object^, Windows::UI::Xaml::RoutedEventArgs^) { ShowCategory(GraphicsPanel); }
void SettingsPage::OnAudioCategoryClick(Object^, Windows::UI::Xaml::RoutedEventArgs^) { ShowCategory(AudioPanel); }
void SettingsPage::OnAdditionalCategoryClick(Object^, Windows::UI::Xaml::RoutedEventArgs^) { ShowCategory(AdditionalPanel); }
