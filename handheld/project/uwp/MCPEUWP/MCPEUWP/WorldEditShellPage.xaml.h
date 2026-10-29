#pragma once

#include "WorldEditShellPage.g.h"

#include <windows.ui.xaml.media.animation.h>

#include <string>

namespace MCPEUWP
{
	public ref class WorldEditShellPage sealed
	{
	public:
		WorldEditShellPage();
		void SetWorldId(Platform::String^ worldId);

		event Windows::Foundation::EventHandler<Platform::Object^>^ BackRequested;

	private:
		void OnBackClick(Platform::Object^ sender,
			Windows::UI::Xaml::RoutedEventArgs^ e);
		void OnDeleteWorldClick(Platform::Object^ sender,
			Windows::UI::Xaml::RoutedEventArgs^ e);
		void OnExportWorldClick(Platform::Object^ sender,
			Windows::UI::Xaml::RoutedEventArgs^ e);
		void OnChangeFolderNameClick(Platform::Object^ sender,
			Windows::UI::Xaml::RoutedEventArgs^ e);
		void OnChangeWorldNameClick(Platform::Object^ sender,
			Windows::UI::Xaml::RoutedEventArgs^ e);
		void OnSecondaryDialogPrimaryClick(Platform::Object^ sender,
			Windows::UI::Xaml::RoutedEventArgs^ e);
		void OnSecondaryDialogCancelClick(Platform::Object^ sender,
			Windows::UI::Xaml::RoutedEventArgs^ e);
		void ScheduleLoadWorldData();
		void LoadWorldData();
		void ShowSecondaryDialog(int kind, Platform::String^ title,
			Platform::String^ message, Platform::String^ initialText);
		void HideSecondaryDialog();
		void AnimateSecondaryDialog(bool entering);
		void SetStatus(const std::wstring& text);
		std::string WorldDirectory() const;
		std::string Utf8(Platform::String^ value) const;
		void ClosePage();

		Platform::String^ m_worldId;
		bool m_operationPending = false;
		int m_secondaryDialogKind = 0;
		Windows::UI::Xaml::Media::Animation::Storyboard^ m_secondaryDialogTransition;
		Windows::Foundation::EventRegistrationToken m_secondaryDialogTransitionCompletedToken{};
	};
}
