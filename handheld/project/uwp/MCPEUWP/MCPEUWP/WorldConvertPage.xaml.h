#pragma once

#include "WorldConvertPage.g.h"

#include <string>

namespace MCPEUWP
{
	public ref class WorldConvertPage sealed
	{
	public:
		WorldConvertPage();
		void SetWorldContext(Platform::String^ sourceDirectory,
			Platform::String^ worldId,
			Platform::String^ worldName,
			int worldType,
			int indevType,
			bool alphaWinter,
			bool currentStorageFormat);

		event Windows::Foundation::EventHandler<Platform::Object^>^ BackRequested;

	private:
		void On17VoidWorldToggleClick(Platform::Object^ sender,
			Windows::UI::Xaml::RoutedEventArgs^ e);
		void OnConvertClick(Platform::Object^ sender,
			Windows::UI::Xaml::RoutedEventArgs^ e);
		void OnBackClick(Platform::Object^ sender,
			Windows::UI::Xaml::RoutedEventArgs^ e);
		void UpdateConversionMode();
		void SetStatus(const std::wstring& text);

		Platform::String^ m_sourceDirectory;
		Platform::String^ m_worldId;
		Platform::String^ m_worldName;
		int m_worldType = 0;
		int m_indevType = 0;
		bool m_alphaWinter = false;
		bool m_currentStorageFormat = false;
		bool m_convertTo17VoidWorld = false;
		bool m_conversionRunning = false;
	};
}
