#pragma once

#include "WorldEditPage.g.h"
#include <string>

class LevelData;

namespace MCPEUWP
{
	public ref class WorldEditPage sealed
	{
	public:
		WorldEditPage();
		void SetWorldId(Platform::String^ worldId);

		event Windows::Foundation::EventHandler<Platform::Object^>^ BackRequested;

	private:
		void OnChangeFolderNameClick(Platform::Object^ sender,
			Windows::UI::Xaml::RoutedEventArgs^ e);
		void OnChangeWorldNameClick(Platform::Object^ sender,
			Windows::UI::Xaml::RoutedEventArgs^ e);
		void OnDeleteWorldClick(Platform::Object^ sender,
			Windows::UI::Xaml::RoutedEventArgs^ e);
		void OnExportWorldClick(Platform::Object^ sender,
			Windows::UI::Xaml::RoutedEventArgs^ e);
		void OnBackClick(Platform::Object^ sender,
			Windows::UI::Xaml::RoutedEventArgs^ e);
		void OnLoaded(Platform::Object^ sender,
			Windows::UI::Xaml::RoutedEventArgs^ e);
		void LoadWorldData();
		void SetStatus(const std::wstring& text);
		std::string WorldDirectory() const;
		std::string Utf8(Platform::String^ value) const;
		void ApplyWorldData(const LevelData& data);
		void ClosePage();

		Platform::String^ m_worldId;
		bool m_operationPending = false;
		bool m_pageLoaded = false;
	};
}
