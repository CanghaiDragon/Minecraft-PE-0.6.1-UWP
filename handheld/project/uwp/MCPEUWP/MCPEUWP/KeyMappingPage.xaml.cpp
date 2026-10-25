#include "pch.h"
#include "KeyMappingPage.xaml.h"
#include "../../../../src/client/Minecraft.h"
#include "../../../../src/client/Options.h"
#include "../../../../src/platform/input/Keyboard.h"

#include <cwchar>
#include <cstdint>
#include <string>
#include <windows.ui.h>
#include <windows.ui.xaml.media.h>

using namespace MCPEUWP;
using namespace Platform;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;

namespace
{
	enum BindingId
	{
		MoveForward = 0,
		MoveLeft,
		MoveBack,
		MoveRight,
		Jump,
		Sneak,
		Inventory,
		Run,
		Spectator,
		DropItem,
		Craft,
		HideHud,
		DebugScreen,
		ThirdPerson,
		Pause,
		Hotbar1,
		Hotbar2,
		Hotbar3,
		Hotbar4,
		Hotbar5,
		Hotbar6,
		Hotbar7,
		Hotbar8
	};

	struct BindingRow
	{
		const wchar_t* label;
		int id;
	};

	const BindingRow kKeyboardRows[] =
	{
		{ L"Move Forward", MoveForward },
		{ L"Move Left", MoveLeft },
		{ L"Move Back", MoveBack },
		{ L"Move Right", MoveRight },
		{ L"Jump", Jump },
		{ L"Sneak", Sneak },
		{ L"Open Inventory", Inventory },
		{ L"Toggle Run (Creative)", Run },
		{ L"Toggle Spectator (Creative)", Spectator },
		{ L"Drop Item", DropItem },
		{ L"Crafting (Survival)", Craft },
		{ L"Toggle Hide HUD", HideHud },
		{ L"Toggle Debug Screen", DebugScreen },
		{ L"Toggle Third-person View", ThirdPerson },
		{ L"Pause / Back", Pause },
		{ L"Hotbar Slot 1", Hotbar1 },
		{ L"Hotbar Slot 2", Hotbar2 },
		{ L"Hotbar Slot 3", Hotbar3 },
		{ L"Hotbar Slot 4", Hotbar4 },
		{ L"Hotbar Slot 5", Hotbar5 },
		{ L"Hotbar Slot 6", Hotbar6 },
		{ L"Hotbar Slot 7", Hotbar7 },
		{ L"Hotbar Slot 8", Hotbar8 }
	};

	const wchar_t* kMouseRows[] =
	{
		L"Mine / Attack",
		L"Place Block / Interact",
		L"Scroll Hotbar"
	};
}

KeyMappingPage::KeyMappingPage()
{
	InitializeComponent();

	for (const BindingRow& row : kKeyboardRows)
	{
		Grid^ line = ref new Grid();
		line->Height = 48;
		line->Margin = Thickness(0, 0, 0, 5);
		line->Background = ref new Windows::UI::Xaml::Media::SolidColorBrush(
			Windows::UI::ColorHelper::FromArgb(150, 35, 35, 35));

		ColumnDefinition^ labelColumn = ref new ColumnDefinition();
		labelColumn->Width = Windows::UI::Xaml::GridLength(1.0, Windows::UI::Xaml::GridUnitType::Star);
		ColumnDefinition^ keyColumn = ref new ColumnDefinition();
		keyColumn->Width = Windows::UI::Xaml::GridLength(
			220.0, Windows::UI::Xaml::GridUnitType::Pixel);
		line->ColumnDefinitions->Append(labelColumn);
		line->ColumnDefinitions->Append(keyColumn);

		TextBlock^ label = ref new TextBlock();
		label->Text = ref new String(row.label);
		label->FontFamily = ref new Windows::UI::Xaml::Media::FontFamily(
			L"ms-appx:///data/fonts/minecraft.ttf#Minecraft");
		label->Foreground = ref new Windows::UI::Xaml::Media::SolidColorBrush(
			Windows::UI::Colors::White);
		label->FontSize = 17;
		label->VerticalAlignment = Windows::UI::Xaml::VerticalAlignment::Center;
		label->Margin = Thickness(16, 0, 8, 0);
		Grid::SetColumn(label, 0);
		line->Children->Append(label);

		Button^ button = ref new Button();
		button->Tag = ref new String(std::to_wstring(row.id).c_str());
		button->Width = 190;
		button->Height = 38;
		button->Margin = Thickness(0, 5, 14, 5);
		button->FontSize = 16;
		button->FontFamily = ref new Windows::UI::Xaml::Media::FontFamily(
			L"ms-appx:///data/fonts/minecraft.ttf#Minecraft");
		button->Foreground = ref new Windows::UI::Xaml::Media::SolidColorBrush(
			Windows::UI::Colors::White);
		button->Click += ref new RoutedEventHandler(this, &KeyMappingPage::OnBindingClick);
		Grid::SetColumn(button, 1);
		line->Children->Append(button);
		BindingList->Children->Append(line);
	}

	for (const wchar_t* labelText : kMouseRows)
	{
		Grid^ line = ref new Grid();
		line->Height = 48;
		line->Margin = Thickness(0, 0, 0, 5);
		line->Background = ref new Windows::UI::Xaml::Media::SolidColorBrush(
			Windows::UI::ColorHelper::FromArgb(100, 35, 35, 35));

		ColumnDefinition^ labelColumn = ref new ColumnDefinition();
		labelColumn->Width = Windows::UI::Xaml::GridLength(1.0, Windows::UI::Xaml::GridUnitType::Star);
		ColumnDefinition^ keyColumn = ref new ColumnDefinition();
		keyColumn->Width = Windows::UI::Xaml::GridLength(
			220.0, Windows::UI::Xaml::GridUnitType::Pixel);
		line->ColumnDefinitions->Append(labelColumn);
		line->ColumnDefinitions->Append(keyColumn);

		TextBlock^ label = ref new TextBlock();
		label->Text = ref new String(labelText);
		label->FontFamily = ref new Windows::UI::Xaml::Media::FontFamily(
			L"ms-appx:///data/fonts/minecraft.ttf#Minecraft");
		label->Foreground = ref new Windows::UI::Xaml::Media::SolidColorBrush(
			Windows::UI::Colors::White);
		label->FontSize = 17;
		label->VerticalAlignment = Windows::UI::Xaml::VerticalAlignment::Center;
		label->Margin = Thickness(16, 0, 8, 0);
		Grid::SetColumn(label, 0);
		line->Children->Append(label);

		TextBlock^ mouse = ref new TextBlock();
		mouse->Text = ref new String(
			std::wstring(labelText) == L"Mine / Attack" ? L"Mouse Left" :
			std::wstring(labelText) == L"Place Block / Interact" ? L"Mouse Right" : L"Mouse Wheel");
		mouse->FontFamily = ref new Windows::UI::Xaml::Media::FontFamily(
			L"ms-appx:///data/fonts/minecraft.ttf#Minecraft");
		mouse->Foreground = ref new Windows::UI::Xaml::Media::SolidColorBrush(
			Windows::UI::ColorHelper::FromArgb(220, 255, 208, 128));
		mouse->FontSize = 16;
		mouse->HorizontalAlignment = Windows::UI::Xaml::HorizontalAlignment::Center;
		mouse->VerticalAlignment = Windows::UI::Xaml::VerticalAlignment::Center;
		Grid::SetColumn(mouse, 1);
		line->Children->Append(mouse);
		BindingList->Children->Append(line);
	}
}

void KeyMappingPage::SetClientHandle(unsigned long long handleValue)
{
	m_clientHandle = reinterpret_cast<UwpOriginalClientHandle>(
		static_cast<uintptr_t>(handleValue));
	RefreshBindings();
}

Minecraft* KeyMappingPage::GetMinecraft()
{
	return UwpOriginalClientGetMinecraft(m_clientHandle);
}

int KeyMappingPage::GetBindingKey(int bindingId)
{
	Minecraft* minecraft = GetMinecraft();
	if (minecraft == nullptr)
		return -1;
	const Options& options = minecraft->options;
	switch (bindingId)
	{
	case MoveForward: return options.keyUp.key;
	case MoveLeft: return options.keyLeft.key;
	case MoveBack: return options.keyDown.key;
	case MoveRight: return options.keyRight.key;
	case Jump: return options.keyJump.key;
	case Sneak: return options.keySneak.key;
	case Inventory: return options.keyBuild.key;
	case Run: return options.keyRun.key;
	case Spectator: return options.keySpectator.key;
	case DropItem: return options.keyDrop.key;
	case Craft: return options.keyCraft.key;
	case HideHud: return options.keyHideHud.key;
	case DebugScreen: return options.keyDebugScreen.key;
	case ThirdPerson: return options.keyThirdPerson.key;
	case Pause: return options.keyPause.key;
	case Hotbar1: case Hotbar2: case Hotbar3: case Hotbar4:
	case Hotbar5: case Hotbar6: case Hotbar7: case Hotbar8:
		return options.keyHotbar[bindingId - Hotbar1].key;
	default: return -1;
	}
}

void KeyMappingPage::SetBindingKey(int bindingId, int keyCode)
{
	Minecraft* minecraft = GetMinecraft();
	if (minecraft == nullptr)
		return;
	Options& options = minecraft->options;
	switch (bindingId)
	{
	case MoveForward: options.keyUp.key = keyCode; break;
	case MoveLeft: options.keyLeft.key = keyCode; break;
	case MoveBack: options.keyDown.key = keyCode; break;
	case MoveRight: options.keyRight.key = keyCode; break;
	case Jump: options.keyJump.key = keyCode; break;
	case Sneak: options.keySneak.key = keyCode; break;
	case Inventory: options.keyBuild.key = keyCode; break;
	case Run: options.keyRun.key = keyCode; break;
	case Spectator: options.keySpectator.key = keyCode; break;
	case DropItem: options.keyDrop.key = keyCode; break;
	case Craft: options.keyCraft.key = keyCode; break;
	case HideHud: options.keyHideHud.key = keyCode; break;
	case DebugScreen: options.keyDebugScreen.key = keyCode; break;
	case ThirdPerson: options.keyThirdPerson.key = keyCode; break;
	case Pause: options.keyPause.key = keyCode; break;
	case Hotbar1: case Hotbar2: case Hotbar3: case Hotbar4:
	case Hotbar5: case Hotbar6: case Hotbar7: case Hotbar8:
		options.keyHotbar[bindingId - Hotbar1].key = keyCode;
		break;
	default: return;
	}
	options.save();
}

String^ KeyMappingPage::FormatKey(int keyCode)
{
	if (keyCode >= 'A' && keyCode <= 'Z')
		return ref new String(std::wstring(1, static_cast<wchar_t>(keyCode)).c_str());
	if (keyCode >= '0' && keyCode <= '9')
		return ref new String(std::wstring(1, static_cast<wchar_t>(keyCode)).c_str());
	if (keyCode >= Keyboard::KEY_F1 && keyCode <= Keyboard::KEY_F12)
	{
		std::wstring value = L"F" + std::to_wstring(keyCode - Keyboard::KEY_F1 + 1);
		return ref new String(value.c_str());
	}
	switch (keyCode)
	{
	case Keyboard::KEY_SPACE: return ref new String(L"Space");
	case Keyboard::KEY_LSHIFT: return ref new String(L"Left Shift");
	case Keyboard::KEY_LEFT_CTRL: return ref new String(L"Left Ctrl");
	case Keyboard::KEY_ESCAPE: return ref new String(L"Escape");
	case Keyboard::KEY_RETURN: return ref new String(L"Enter");
	case Keyboard::KEY_BACKSPACE: return ref new String(L"Backspace");
	default:
		return ref new String(L"Key");
	}
}

void KeyMappingPage::SetButtonText(Button^ button, int keyCode)
{
	if (button != nullptr)
		button->Content = FormatKey(keyCode);
}

void KeyMappingPage::RefreshBindings()
{
	if (BindingList == nullptr || GetMinecraft() == nullptr)
		return;
	int buttonIndex = 0;
	for (const BindingRow& row : kKeyboardRows)
	{
		Grid^ line = dynamic_cast<Grid^>(BindingList->Children->GetAt(buttonIndex++));
		if (line == nullptr || line->Children->Size < 2)
			continue;
		SetButtonText(dynamic_cast<Button^>(line->Children->GetAt(1)),
			GetBindingKey(row.id));
	}
}

void KeyMappingPage::SetStatus(const std::wstring& text)
{
	if (MappingStatus != nullptr)
		MappingStatus->Text = ref new String(text.c_str());
}

void KeyMappingPage::OnBindingClick(Object^ sender, RoutedEventArgs^)
{
	Button^ button = dynamic_cast<Button^>(sender);
	if (button == nullptr || button->Tag == nullptr)
		return;
	String^ tag = dynamic_cast<String^>(button->Tag);
	if (tag == nullptr)
		return;
	m_pendingBinding = _wtoi(tag->Data());
	SetStatus(L"Press a key to assign it, or press Escape to cancel.");
		button->Focus(Windows::UI::Xaml::FocusState::Programmatic);
}

bool KeyMappingPage::HandleKeyDown(int keyCode)
{
	if (m_pendingBinding < 0)
		return false;
	if (keyCode == Keyboard::KEY_ESCAPE)
	{
		m_pendingBinding = -1;
		SetStatus(L"Keyboard bindings are saved automatically.");
		return true;
	}
	if (keyCode >= 0)
	{
		SetBindingKey(m_pendingBinding, keyCode);
		m_pendingBinding = -1;
		RefreshBindings();
		SetStatus(L"Key assigned. Keyboard bindings are saved automatically.");
	}
	return true;
}

void KeyMappingPage::OnResetClick(Object^, RoutedEventArgs^)
{
	Minecraft* minecraft = GetMinecraft();
	if (minecraft == nullptr)
		return;
	minecraft->options.resetKeyMappings();
	minecraft->options.save();
	m_pendingBinding = -1;
	RefreshBindings();
	SetStatus(L"Keyboard bindings restored to defaults.");
}

void KeyMappingPage::OnBackClick(Object^, RoutedEventArgs^)
{
	m_pendingBinding = -1;
	this->BackRequested(this, nullptr);
}
