#pragma once

#include "WorldEditShellPage.g.h"
#include "WorldTileIconPage.xaml.h"
#include "WorldConvertPage.xaml.h"

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
		event Windows::Foundation::EventHandler<Platform::String^>^ LegacyFormatConversionRequested;
		event Windows::Foundation::EventHandler<Platform::String^>^ FiniteWorldUpgradeRequested;

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
		void OnConvertTo09Click(Platform::Object^ sender,
			Windows::UI::Xaml::RoutedEventArgs^ e);
		void OnUpgradeToInfiniteClick(Platform::Object^ sender,
			Windows::UI::Xaml::RoutedEventArgs^ e);
		void OnConvertToNewFormatClick(Platform::Object^ sender,
			Windows::UI::Xaml::RoutedEventArgs^ e);
		void OnViewAchievementsClick(Platform::Object^ sender,
			Windows::UI::Xaml::RoutedEventArgs^ e);
		void OnAchievementsBackClick(Platform::Object^ sender,
			Windows::UI::Xaml::RoutedEventArgs^ e);
		void OnSecondaryDialogPrimaryClick(Platform::Object^ sender,
			Windows::UI::Xaml::RoutedEventArgs^ e);
		void OnSecondaryDialogCancelClick(Platform::Object^ sender,
			Windows::UI::Xaml::RoutedEventArgs^ e);
		void HideAchievements();
		void AnimateAchievementsOverlay(bool entering);
		void ScheduleLoadWorldData();
		void LoadWorldData();
		void ShowSecondaryDialog(int kind, Platform::String^ title,
			Platform::String^ message, Platform::String^ initialText);
		void HideSecondaryDialog();
		void AnimateSecondaryDialog(bool entering);
		void ShowWorldTileIconPage();
		void HideWorldTileIconPage();
		void AnimateWorldTileIconPage(bool entering);
		void ShowWorldConvertPage();
		void HideWorldConvertPage();
		void AnimateWorldConvertPage(bool entering);
		void PopulateAchievements();
		void PinWorldShortcut(Platform::String^ iconName);
		void SetStatus(const std::wstring& text);
		std::string WorldDirectory() const;
		std::string Utf8(Platform::String^ value) const;
		void ClosePage();

		Platform::String^ m_worldId;
		Platform::String^ m_selectedTileIcon;
		WorldTileIconPage^ m_worldTileIconPage;
		WorldConvertPage^ m_worldConvertPage;
		int m_worldType = 0;
		int m_worldIndevType = 0;
		int m_gameType = -1;
		bool m_alphaWinterMode = false;
		bool m_worldStorageFormatCurrent = false;
		bool m_legacyFormatConversionAvailable = false;
		bool m_finiteWorldUpgradeAvailable = false;
		bool m_operationPending = false;
		bool m_pinOperationPending = false;
		int m_secondaryDialogKind = 0;
		Windows::UI::Xaml::Media::Animation::Storyboard^ m_secondaryDialogTransition;
		Windows::Foundation::EventRegistrationToken m_secondaryDialogTransitionCompletedToken{};
		Windows::UI::Xaml::Media::Animation::Storyboard^ m_worldTileIconTransition;
		Windows::Foundation::EventRegistrationToken m_worldTileIconTransitionCompletedToken{};
		Windows::UI::Xaml::Media::Animation::Storyboard^ m_worldConvertTransition;
		Windows::Foundation::EventRegistrationToken m_worldConvertTransitionCompletedToken{};
		Windows::UI::Xaml::Media::Animation::Storyboard^ m_achievementsTransition;
		Windows::Foundation::EventRegistrationToken m_achievementsTransitionCompletedToken{};
	};
}
