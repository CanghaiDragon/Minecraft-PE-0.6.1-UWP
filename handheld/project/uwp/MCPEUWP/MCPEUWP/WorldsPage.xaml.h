#pragma once

#include "WorldsPage.g.h"

namespace MCPEUWP
{
	public ref class WorldsPage sealed
	{
	public:
		WorldsPage();

	private:
		void OnLoaded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void OnBackClick(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void OnWorldSelected(Platform::Object^ sender, Windows::UI::Xaml::Controls::SelectionChangedEventArgs^ e);
		void OnCreateWorldClick(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void LoadWorldList();
	};
}
