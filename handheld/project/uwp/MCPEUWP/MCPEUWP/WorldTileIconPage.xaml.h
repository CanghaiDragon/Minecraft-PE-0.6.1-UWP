#pragma once

#include "WorldTileIconPage.g.h"

namespace MCPEUWP
{
	public ref class WorldTileIconPage sealed
	{
	public:
		WorldTileIconPage();

		event Windows::Foundation::EventHandler<Platform::String^>^ IconSelected;
		event Windows::Foundation::EventHandler<Platform::Object^>^ BackRequested;

	private:
		void OnIconClick(Platform::Object^ sender,
			Windows::UI::Xaml::RoutedEventArgs^ e);
		void OnBackClick(Platform::Object^ sender,
			Windows::UI::Xaml::RoutedEventArgs^ e);
	};
}
