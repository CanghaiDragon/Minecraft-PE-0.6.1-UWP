//
// MainPage.xaml.h
// MainPage 类的声明。
//

#pragma once

#include "MainPage.g.h"
#include "KeyMappingPage.xaml.h"
#include "WorldEditShellPage.xaml.h"
#include "WorldImportPage.xaml.h"
#include "../../../../src/AppPlatform_uwp.h"
#include "../../../../src/platform/uwp/UwpAngleContext.h"
#include "../../../../src/platform/uwp/UwpCoreSession.h"
#include "../../../../src/platform/uwp/UwpGameLoop.h"
#include "../../../../src/platform/uwp/UwpOriginalClientHost.h"
#include "../../../../src/platform/uwp/UwpStoragePaths.h"
#include "../../../../src/platform/uwp/UwpUiRenderer.h"
#include "../../../../src/platform/uwp/UwpWorldRenderer.h"
#include <windows.devices.input.h>
#include <windows.ui.core.h>
#include <windows.ui.xaml.media.animation.h>
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace MCPEUWP
{
	/// <summary>
	/// 可用于自身或导航至 Frame 内部的空白页。
	/// </summary>
	public ref class MainPage sealed
	{
	public:
		MainPage();
		void OnNavigatedTo(Windows::UI::Xaml::Navigation::NavigationEventArgs^ e) override;
		void OnNavigatedFrom(Windows::UI::Xaml::Navigation::NavigationEventArgs^ e) override;
		// Called by the UWP suspension handler while the page still owns the
		// active session.  It flushes the original Level before suspension;
		// it does not leave the world or replace the current screen.
		void SaveActiveWorld();

	private:
		void OnLoaded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void OnRendering(Platform::Object^ sender, Platform::Object^ e);
		void OnPointerMoved(Platform::Object^ sender, Windows::UI::Xaml::Input::PointerRoutedEventArgs^ e);
		void OnPointerPressed(Platform::Object^ sender, Windows::UI::Xaml::Input::PointerRoutedEventArgs^ e);
		void OnPointerReleased(Platform::Object^ sender, Windows::UI::Xaml::Input::PointerRoutedEventArgs^ e);
		void OnPointerCanceled(Platform::Object^ sender, Windows::UI::Xaml::Input::PointerRoutedEventArgs^ e);
		void OnPointerCaptureLost(Platform::Object^ sender, Windows::UI::Xaml::Input::PointerRoutedEventArgs^ e);
		void OnPointerWheelChanged(Platform::Object^ sender, Windows::UI::Xaml::Input::PointerRoutedEventArgs^ e);
		void OnSkinImportClick(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void OnSkinClearClick(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void OnSkinBackClick(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void OnSkinArmTypeChanged(Platform::Object^ sender,
			Windows::UI::Xaml::Controls::SelectionChangedEventArgs^ e);
		void OnRelativeMouseMoved(Windows::Devices::Input::MouseDevice^ sender,
			Windows::Devices::Input::MouseEventArgs^ e);
		void OnKeyDown(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::KeyEventArgs^ e);
		void OnKeyUp(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::KeyEventArgs^ e);
		void OnSystemBackRequested(
			Platform::Object^ sender,
			Windows::UI::Core::BackRequestedEventArgs^ e);
		void OnCharacterReceived(Windows::UI::Core::CoreWindow^ sender,
			Windows::UI::Core::CharacterReceivedEventArgs^ e);
		void UpdateMoveInput();
		void AttachKeyboard();
		void DetachKeyboard();
		void LoadMenuTexture(Platform::String^ uri, UwpUiRenderer::TextureSlot slot);
		void LoadTerrainTexture();
		void BeginOriginalClientTexturePreload();
		void LoadOriginalClientStartupTexture(const std::string& filename);
		void RecordOriginalClientTexturePreload(const std::string& filename, bool succeeded);
		void RunOriginalTexturesSmoke();
		void RunOriginalFontSmoke();
		void RunOriginalTesselatorSmoke();
		void RunOriginalGlesSmoke();
		void LoadMenuFont();
		void RunPlatformSmokeTest();
		void OnGameTick(std::uint64_t tick);
		void OnGameRender(float alpha);
		void RenderSavingWorldFrame(bool frameAlreadyBegun);
		void SyncOriginalMouseCursor();
		void BeginWorldLoad(const std::wstring& worldId,
			const std::wstring& worldName, bool createNew, long seed,
			int gameType, int worldType);
		void ShowOriginalWorldSelection();
		bool AttachOriginalWorldForTick();
		void ShowSkinPage();
		void HideSkinPage();
		void ShowKeyMappingPage();
		void HideKeyMappingPage();
		void ShowWorldEditPage(const std::string& worldId);
		void HideWorldEditPage();
		void ShowWorldImportPage();
		void HideWorldImportPage();
		void ScheduleWorldEditDirtBackgroundRebuild();
		void AnimateSkinPage(bool entering);
		void AnimateOverlay(Windows::UI::Xaml::Controls::Grid^ overlay, bool entering);
		void OnSkinBackgroundSizeChanged(Platform::Object^ sender,
			Windows::UI::Xaml::SizeChangedEventArgs^ e);
		void RebuildSkinDirtBackground();
		void LoadSkinPreview();
		concurrency::task<void> ImportSkinFileAsync(Windows::Storage::StorageFile^ file);
		void SetSkinStatus(const std::wstring& text);
		void ShutdownAngle();
		void SetStartupDiagnostic(const std::wstring& stage, const std::wstring& details);
		int TouchSlotForPointer(std::uint32_t pointerId);
		int ExistingTouchSlotForPointer(std::uint32_t pointerId) const;
		void ReleaseTouchSlot(std::uint32_t pointerId);
		void ResetTouchSlots();
		void ReleasePointerInput(Windows::UI::Xaml::Input::PointerRoutedEventArgs^ e);

		UwpAngleContext m_angle;
		std::unique_ptr<AppPlatform_uwp> m_platform;
		UwpOriginalClientHost m_originalClientHost;
		UwpCoreSession m_core;
		UwpGameLoop m_gameLoop;
		UwpUiRenderer m_ui;
		UwpWorldRenderer m_worldRenderer;
		Windows::Foundation::EventRegistrationToken m_renderToken{};
		std::chrono::steady_clock::time_point m_lastFrameTime{};
		bool m_frameClockStarted = false;
		bool m_worldCreationPending = false;
		bool m_originalMenuActive = false;
		bool m_skinPageVisible = false;
		bool m_keyMappingPageVisible = false;
		bool m_worldEditPageVisible = false;
		bool m_worldImportPageVisible = false;
		bool m_worldImportSuspendedOriginalLoop = false;
		bool m_worldEditDirtRebuildPending = false;
		int m_worldEditDirtRebuildAttempts = 0;
		bool m_worldEditSuspendedOriginalLoop = false;
		bool m_skinOperationPending = false;
		bool m_skinArmTypeLoading = false;
		std::uint64_t m_skinPreviewGeneration = 0;
		Windows::UI::Xaml::Media::Animation::Storyboard^ m_skinPageTransition;
		KeyMappingPage^ m_keyMappingPage;
		WorldEditShellPage^ m_worldEditShellPage;
		WorldImportPage^ m_worldImportPage;
		Windows::Foundation::EventRegistrationToken m_skinPageTransitionCompletedToken{};
		int m_skinDirtColumns = 0;
		int m_skinDirtRows = 0;
		enum class PendingSkinAction
		{
			None,
			Apply,
			Clear
		};
		PendingSkinAction m_pendingSkinAction = PendingSkinAction::None;
		bool m_rebuildingSkinDirtBackground = false;
	bool m_firstFramePresented = false;
	std::uint64_t m_lastPerformanceDisplayTick = 0;
	double m_maxFrameMilliseconds = 0.0;
		std::vector<std::uint32_t> m_touchPointerIds;
		bool m_keyW = false;
		bool m_keyA = false;
		bool m_keyS = false;
		bool m_keyD = false;
		bool m_keySpace = false;
		float m_originalPointerX = -1.0f;
		float m_originalPointerY = -1.0f;
		bool m_originalPointerCursorHidden = false;
		Windows::Foundation::EventRegistrationToken m_keyDownToken{};
		Windows::Foundation::EventRegistrationToken m_keyUpToken{};
		Windows::Foundation::EventRegistrationToken m_systemBackToken{};
		Windows::Foundation::EventRegistrationToken m_characterReceivedToken{};
		Windows::Foundation::EventRegistrationToken m_mouseMovedToken{};
		Windows::UI::Core::SystemNavigationManager^ m_systemNavigationManager;
		std::wstring m_platformSmokeResult;
		unsigned int m_clientTexturePreloadCompleted = 0;
		unsigned int m_clientTexturePreloadSucceeded = 0;
		unsigned int m_clientTexturePreloadExpected = 0;
		bool m_originalClientInitAttempted = false;
		std::vector<std::wstring> m_clientTexturePreloadFailures;
		bool m_originalTexturesSmokeDone = false;
		bool m_originalFontSmokeDone = false;
		bool m_originalTesselatorSmokeDone = false;
		bool m_originalGlesSmokeDone = false;
	};
}
