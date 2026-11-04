#pragma once
#include "SettingsPage.g.h"
#include "../../../../src/platform/uwp/UwpOriginalClientBridge.h"
namespace MCPEUWP
{
	public ref class SettingsPage sealed
	{
	public:
		SettingsPage();
		void SetClientHandle(unsigned long long handleValue);
		event Windows::Foundation::EventHandler<Platform::Object^>^ BackRequested;
	private:
		UwpOriginalClientHandle m_clientHandle = nullptr;
		bool m_refreshingOptions = false;
		void OnBackClick(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void OnRestoreDefaultsClick(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void OnPageKeyDown(Platform::Object^ sender, Windows::UI::Xaml::Input::KeyRoutedEventArgs^ e);
		void OnUsernameLostFocus(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void OnOptionSelectionChanged(Platform::Object^ sender,
			Windows::UI::Xaml::Controls::SelectionChangedEventArgs^ e);
		void OnOptionSliderChanged(Platform::Object^ sender,
			Windows::UI::Xaml::Controls::Primitives::RangeBaseValueChangedEventArgs^ e);
		void OnOptionCheckBoxChanged(Platform::Object^ sender,
			Windows::UI::Xaml::RoutedEventArgs^ e);
		void OnGameCategoryClick(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void OnControlsCategoryClick(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void OnGraphicsCategoryClick(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void OnAudioCategoryClick(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void OnAdditionalCategoryClick(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void ShowCategory(Windows::UI::Xaml::Controls::Border^ panel);
		void RefreshFromOriginalOptions();
	};
}
