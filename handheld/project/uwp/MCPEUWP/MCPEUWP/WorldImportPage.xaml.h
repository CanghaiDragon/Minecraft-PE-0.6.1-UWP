#pragma once

#include "WorldImportPage.g.h"
#include <windows.ui.xaml.media.animation.h>
#include <windows.storage.h>

namespace MCPEUWP
{
	public ref class WorldImportPage sealed
	{
	public:
		WorldImportPage();
		void HandleEnterKey();
		void HandleEscapeKey();

	private:
		void OnSelectZipClick(Platform::Object^ sender,
			Windows::UI::Xaml::RoutedEventArgs^ e);
		void OnBackClick(Platform::Object^ sender,
			Windows::UI::Xaml::RoutedEventArgs^ e);
		void OnRejectDialogOkClick(Platform::Object^ sender,
			Windows::UI::Xaml::RoutedEventArgs^ e);
		void ShowRejectDialog(Platform::String^ title, Platform::String^ message);
		void HideRejectDialog();
		void AnimateRejectDialog(bool entering);
		void ShowTooNewVersionDialog(unsigned int formatVersion);
		void ShowMultipleLevelDatDialog();
		void ShowUnsupportedArchiveNameDialog();
		void ShowInvalidArchiveDialog(Platform::String^ error);
		void ShowImportSuccess(unsigned int formatVersion, Platform::String^ worldRoot);
		Windows::Storage::StorageFile^ m_importTemporaryFile;
		Platform::String^ m_importWorldName;
		Platform::String^ m_importArchiveName;
		Windows::UI::Xaml::Media::Animation::Storyboard^ m_rejectDialogTransition;
		Windows::Foundation::EventRegistrationToken m_rejectDialogTransitionCompletedToken{};
	};
}
