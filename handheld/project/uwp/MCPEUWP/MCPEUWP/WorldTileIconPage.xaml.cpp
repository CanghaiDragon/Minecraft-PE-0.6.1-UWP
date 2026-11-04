#include "pch.h"
#include "WorldTileIconPage.xaml.h"

using namespace MCPEUWP;
using namespace Platform;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;

WorldTileIconPage::WorldTileIconPage()
{
	InitializeComponent();
}

void WorldTileIconPage::OnIconClick(
	Object^ sender,
	RoutedEventArgs^)
{
	Button^ button = dynamic_cast<Button^>(sender);
	String^ iconName = button == nullptr
		? nullptr
		: dynamic_cast<String^>(button->Tag);
	if (iconName == nullptr || iconName->Length() == 0)
		return;

	this->IconSelected(this, iconName);
}

void WorldTileIconPage::OnBackClick(
	Object^, RoutedEventArgs^)
{
	this->BackRequested(this, nullptr);
}
