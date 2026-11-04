#pragma once

#include "WorldEditShellPage.g.h"
#include "WorldTileIconPage.xaml.h"

#include <windows.ui.xaml.media.animation.h>

#include <string>

namespace MCPEUWP
{
	public ref class WorldEditShellPage sealed
	{
	public:
		WorldEditShellPage();
		void SetWorldId(Platform::String^ worldId);
		bool HandleEscapeKey();

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
		void OnChooseStartIconClick(Platform::Object^ sender,
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
		void ShowWorldTileIconPage();
		void HideWorldTileIconPage();
		void AnimateWorldTileIconPage(bool entering);
		void PinWorldShortcut(Platform::String^ iconName);
		void SetStatus(const std::wstring& text);
		std::string WorldDirectory() const;
		std::string Utf8(Platform::String^ value) const;
		void ClosePage();

		Platform::String^ m_worldId;
		Platform::String^ m_selectedTileIcon;
		WorldTileIconPage^ m_worldTileIconPage;
		bool m_operationPending = false;
		bool m_pinOperationPending = false;
		int m_secondaryDialogKind = 0;
		Windows::UI::Xaml::Media::Animation::Storyboard^ m_secondaryDialogTransition;
		Windows::Foundation::EventRegistrationToken m_secondaryDialogTransitionCompletedToken{};
		Windows::UI::Xaml::Media::Animation::Storyboard^ m_worldTileIconTransition;
		Windows::Foundation::EventRegistrationToken m_worldTileIconTransitionCompletedToken{};
	};
}
