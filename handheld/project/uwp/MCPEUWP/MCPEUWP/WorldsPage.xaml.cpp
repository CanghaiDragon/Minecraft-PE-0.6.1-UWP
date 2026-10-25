#include "pch.h"
#include "WorldsPage.xaml.h"
#include "../../../../src/platform/uwp/UwpStoragePaths.h"

using namespace MCPEUWP;

using namespace concurrency;
using namespace Platform;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::Storage;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;

WorldsPage::WorldsPage()
{
	InitializeComponent();
	Loaded += ref new RoutedEventHandler(this, &WorldsPage::OnLoaded);
}

void WorldsPage::OnLoaded(Object^, RoutedEventArgs^)
{
	LoadWorldList();
}

void WorldsPage::LoadWorldList()
{
	WorldList->Items->Clear();
	WorldList->SelectedIndex = -1;
	WorldList->IsEnabled = false;
	EmptyText->Visibility = Windows::UI::Xaml::Visibility::Collapsed;
	StatusText->Text = "Loading saved worlds...";

	UwpStoragePaths::ensureWorldsFolderAsync()
		.then([](StorageFolder^ folder)
		{
			return create_task(folder->GetFoldersAsync());
		})
		.then([this](IVectorView<StorageFolder^>^ folders)
		{
			for (StorageFolder^ folder : folders)
			{
				WorldList->Items->Append(ref new String(folder->Name->Data()));
			}

			WorldList->IsEnabled = true;
			if (folders->Size == 0)
			{
				EmptyText->Visibility = Windows::UI::Xaml::Visibility::Visible;
				StatusText->Text = "No saved worlds found in the UWP storage folder.";
			}
			else
			{
				EmptyText->Visibility = Windows::UI::Xaml::Visibility::Collapsed;
				StatusText->Text = "Saved worlds found.";
			}
		}, task_continuation_context::use_current())
		.then([this](task<void> result)
		{
			try
			{
				result.get();
			}
			catch (Exception^)
			{
				WorldList->IsEnabled = false;
				EmptyText->Visibility = Windows::UI::Xaml::Visibility::Visible;
				StatusText->Text = "Failed to read saved worlds.";
			}
		}, task_continuation_context::use_current());
}

void WorldsPage::OnBackClick(Object^, RoutedEventArgs^)
{
	if (Frame != nullptr && Frame->CanGoBack)
	{
		Frame->GoBack();
	}
}

void WorldsPage::OnWorldSelected(Object^, SelectionChangedEventArgs^)
{
	if (WorldList->SelectedItem == nullptr)
	{
		return;
	}

	auto selected = safe_cast<String^>(WorldList->SelectedItem);
	StatusText->Text = "Selected world: " + selected
		+ " (Minecraft::selectLevel will be connected next)";
}

void WorldsPage::OnCreateWorldClick(Object^, RoutedEventArgs^)
{
	StatusText->Text = "World generation will be connected after the UWP core is linked.";
}
