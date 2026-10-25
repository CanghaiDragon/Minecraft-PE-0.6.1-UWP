#pragma once

#include "KeyMappingPage.g.h"
#include "../../../../src/platform/uwp/UwpOriginalClientBridge.h"
#include <windows.ui.xaml.controls.h>

namespace MCPEUWP
{
	public ref class KeyMappingPage sealed
	{
	public:
		KeyMappingPage();
		void SetClientHandle(unsigned long long handleValue);
		bool HandleKeyDown(int keyCode);
		bool IsCapturing() { return m_pendingBinding >= 0; }

		event Windows::Foundation::EventHandler<Platform::Object^>^ BackRequested;

	private:
		void OnBindingClick(Platform::Object^ sender,
			Windows::UI::Xaml::RoutedEventArgs^ e);
		void OnResetClick(Platform::Object^ sender,
			Windows::UI::Xaml::RoutedEventArgs^ e);
		void OnBackClick(Platform::Object^ sender,
			Windows::UI::Xaml::RoutedEventArgs^ e);
		void RefreshBindings();
		void SetStatus(const std::wstring& text);
		void SetButtonText(Windows::UI::Xaml::Controls::Button^ button,
			int keyCode);
		int GetBindingKey(int bindingId);
		void SetBindingKey(int bindingId, int keyCode);
		Minecraft* GetMinecraft();
		Platform::String^ FormatKey(int keyCode);

		UwpOriginalClientHandle m_clientHandle = nullptr;
		int m_pendingBinding = -1;
	};
}
