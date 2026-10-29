//
// MainPage.xaml.cpp
// UWP platform shell for the first MCPE ANGLE migration step.
//

#include "pch.h"
#include "MainPage.xaml.h"
#include "../../../../src/platform/uwp/UwpLevelDatSmoke.h"
#include "../../../../src/platform/uwp/UwpOriginalClientHost.h"
#include "../../../../src/platform/uwp/UwpSettingsModel.h"
#include "../../../../src/platform/input/Keyboard.h"
#include "../../../../src/platform/input/Mouse.h"
#include "../../../../src/platform/input/Multitouch.h"
#include "../../../../src/client/Minecraft.h"
#include "../../../../src/client/Options.h"
#include "../../../../src/client/gui/Font.h"
#include "../../../../src/client/renderer/Tesselator.h"
#include "../../../../src/client/renderer/Textures.h"
#include "../../../../src/client/renderer/gles.h"
#include "../../../../src/world/level/LevelSettings.h"
#include <ppltasks.h>
#include <windows.applicationmodel.core.h>
#include <windows.devices.input.h>
#include <windows.graphics.imaging.h>
#include <windows.storage.h>
#include <windows.storage.pickers.h>
#include <windows.storage.streams.h>
#include <windows.h>
#include <windows.ui.xaml.media.imaging.h>
#include <windows.ui.core.h>
#include <windows.ui.input.h>
#include <algorithm>
#include <iomanip>
#include <limits>
#include <sstream>

using namespace MCPEUWP;

using namespace Platform;
using namespace concurrency;
using namespace Windows::Foundation;

namespace
{
	void WorldEditDiagnostic(const std::wstring& message)
	{
#if defined(_DEBUG)
		const std::wstring line = L"[WorldEditDiag] Main: " + message + L"\n";
		OutputDebugStringW(line.c_str());
#else
		(void)message;
#endif
	}

	void WorldEditExceptionDiagnostic(Exception^ error)
	{
#if defined(_DEBUG)
		std::wstringstream message;
		message << L"exception HRESULT=0x" << std::hex
			<< static_cast<unsigned int>(error == nullptr ? 0 : error->HResult);
		if (error != nullptr && error->Message != nullptr)
			message << L" message=" << error->Message->Data();
		WorldEditDiagnostic(message.str());
#else
		(void)error;
#endif
	}
}
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Controls::Primitives;
using namespace Windows::UI::Xaml::Data;
using namespace Windows::UI::Xaml::Input;
using namespace Windows::UI::Xaml::Interop;
using namespace Windows::UI::Xaml::Media;
using namespace Windows::UI::Xaml::Media::Animation;
using namespace Windows::UI::Xaml::Media::Imaging;
using namespace Windows::UI::Xaml::Navigation;
using namespace Windows::UI::Core;
using namespace Windows::Graphics::Imaging;
using namespace Windows::Storage;
using namespace Windows::Storage::Streams;

namespace
{
	// Keep the desktop key values at the platform boundary.  Options and the
	// future Minecraft::tickInput() consumer already use these original values.
	int ToOriginalKeyboardKey(Windows::System::VirtualKey key)
	{
		using Windows::System::VirtualKey;
		switch (key)
		{
		case VirtualKey::Up: return Keyboard::KEY_W;
		case VirtualKey::Left: return Keyboard::KEY_A;
		case VirtualKey::Down: return Keyboard::KEY_S;
		case VirtualKey::Right: return Keyboard::KEY_D;
		case VirtualKey::Space: return Keyboard::KEY_SPACE;
		case VirtualKey::Escape: return Keyboard::KEY_ESCAPE;
		case VirtualKey::Enter: return Keyboard::KEY_RETURN;
		case VirtualKey::Back: return Keyboard::KEY_BACKSPACE;
		case VirtualKey::Shift: return Keyboard::KEY_LSHIFT;
		case VirtualKey::Control: return Keyboard::KEY_LEFT_CTRL;
		case VirtualKey::Number0: return '0';
		case VirtualKey::Number1: return '1';
		case VirtualKey::Number2: return '2';
		case VirtualKey::Number3: return '3';
		case VirtualKey::Number4: return '4';
		case VirtualKey::Number5: return '5';
		case VirtualKey::Number6: return '6';
		case VirtualKey::Number7: return '7';
		case VirtualKey::Number8: return '8';
		case VirtualKey::Number9: return '9';
		default: break;
		}

		const auto value = static_cast<unsigned int>(key);
		if ((value >= Keyboard::KEY_A && value <= Keyboard::KEY_Z) ||
			(value >= Keyboard::KEY_F1 && value <= Keyboard::KEY_F12))
			return static_cast<int>(value);
		return -1;
	}

	bool FeedOriginalKeyboard(Windows::System::VirtualKey key, bool pressed)
	{
		const int originalKey = ToOriginalKeyboardKey(key);
		if (originalKey < 0)
			return false;
		Keyboard::feed(static_cast<unsigned char>(originalKey),
			pressed ? KeyboardAction::KEYDOWN : KeyboardAction::KEYUP);
		return true;
	}

	bool IsMousePointer(Windows::UI::Input::PointerPoint^ point)
	{
		return point != nullptr && point->PointerDevice != nullptr &&
			point->PointerDevice->PointerDeviceType ==
			Windows::Devices::Input::PointerDeviceType::Mouse;
	}
}

MainPage::MainPage()
{
	InitializeComponent();
	m_touchPointerIds.assign(Multitouch::MAX_POINTERS,
		(std::numeric_limits<std::uint32_t>::max)());
	// StartupDiagnostic is a developer aid.  Release packages should present
	// the normal Mojang/loading surface without an always-on diagnostic card.
#if defined(MCPE_UWP_RELEASE)
	if (StartupDiagnostic != nullptr)
		StartupDiagnostic->Visibility = Windows::UI::Xaml::Visibility::Collapsed;
#endif
	// Keep the original platform contract bound to the UWP page lifecycle now;
	// a later bootstrap stage will pass it to the original client context.
	m_platform.reset(new AppPlatform_uwp(1, 1));
	m_gameLoop.setTickHandler([this](std::uint64_t tick)
	{
		OnGameTick(tick);
	});
	m_gameLoop.setRenderHandler([this](float alpha)
	{
		OnGameRender(alpha);
	});
	Loaded += ref new RoutedEventHandler(this, &MainPage::OnLoaded);
	AnglePanel->PointerMoved += ref new PointerEventHandler(this, &MainPage::OnPointerMoved);
	AnglePanel->PointerPressed += ref new PointerEventHandler(this, &MainPage::OnPointerPressed);
	AnglePanel->PointerReleased += ref new PointerEventHandler(this, &MainPage::OnPointerReleased);
	AnglePanel->PointerCanceled += ref new PointerEventHandler(this, &MainPage::OnPointerCanceled);
	AnglePanel->PointerCaptureLost += ref new PointerEventHandler(this, &MainPage::OnPointerCaptureLost);
	AnglePanel->PointerWheelChanged += ref new PointerEventHandler(this, &MainPage::OnPointerWheelChanged);
}

int MainPage::TouchSlotForPointer(std::uint32_t pointerId)
{
	const std::uint32_t unused = (std::numeric_limits<std::uint32_t>::max)();
	for (int slot = 0; slot < static_cast<int>(m_touchPointerIds.size()); ++slot)
		if (m_touchPointerIds[slot] == pointerId)
			return slot;

	for (int slot = 0; slot < static_cast<int>(m_touchPointerIds.size()); ++slot)
	{
		if (m_touchPointerIds[slot] == unused)
		{
			m_touchPointerIds[slot] = pointerId;
			return slot;
		}
	}
	return -1;
}

int MainPage::ExistingTouchSlotForPointer(std::uint32_t pointerId) const
{
	for (int slot = 0; slot < static_cast<int>(m_touchPointerIds.size()); ++slot)
		if (m_touchPointerIds[slot] == pointerId)
			return slot;
	return -1;
}

void MainPage::ReleaseTouchSlot(std::uint32_t pointerId)
{
	for (std::uint32_t& mapped : m_touchPointerIds)
	{
		if (mapped == pointerId)
		{
			mapped = (std::numeric_limits<std::uint32_t>::max)();
			return;
		}
	}
}

void MainPage::ResetTouchSlots()
{
	std::fill(m_touchPointerIds.begin(), m_touchPointerIds.end(),
		(std::numeric_limits<std::uint32_t>::max)());
	Multitouch::reset();
}

void MainPage::OnLoaded(Object^, RoutedEventArgs^)
{
	SetStartupDiagnostic(L"MainPage.OnLoaded reached", L"Creating ANGLE EGL display...");
	if (m_angle.isInitialized())
	{
		SetStartupDiagnostic(L"ANGLE already initialized", L"Resuming render callback");
		return;
	}

	if (!m_angle.initialize(AnglePanel))
	{
		std::wostringstream error;
		error << L"stage: " << m_angle.lastStage()
			<< L" | EGL error: 0x" << std::hex << std::uppercase
			<< static_cast<unsigned int>(m_angle.lastError());
		if (!m_angle.lastDetails().empty())
			error << L" | " << m_angle.lastDetails();
		SetStartupDiagnostic(L"ANGLE initialization failed", error.str());
		return;
	}
	if (!m_originalClientHost.bind(m_platform.get(), m_angle))
	{
		SetStartupDiagnostic(L"Original client host binding failed",
			L"UWP EGL/AppPlatform or NinecraftApp construction is unavailable");
		return;
	}
	if (!m_originalClientHost.initializeLocale())
	{
		SetStartupDiagnostic(L"Original I18n initialization failed",
			L"Could not load data/lang/en_US.lang through AppPlatform");
		return;
	}
	AttachKeyboard();
	SetStartupDiagnostic(L"ANGLE initialized", L"Creating GLES2 UI renderer...");
	if (!m_ui.initialize())
	{
		SetStartupDiagnostic(L"GLES2 UI initialization failed",
			L"Shader program or GLES attribute setup failed");
		return;
	}
	if (!m_worldRenderer.initialize())
	{
		SetStartupDiagnostic(L"GLES2 world renderer initialization failed",
			L"Terrain shader program or vertex setup failed");
		return;
	}
	SetStartupDiagnostic(L"GLES2 UI initialized", L"Loading MCPE textures and font...");
	LoadMenuTexture(ref new Platform::String(L"ms-appx:///data/images/gui/background.png"), UwpUiRenderer::TextureSlot::Background);
	LoadMenuTexture(ref new Platform::String(L"ms-appx:///data/images/gui/title.png"), UwpUiRenderer::TextureSlot::Title);
	LoadMenuTexture(ref new Platform::String(L"ms-appx:///data/images/gui/gui.png"), UwpUiRenderer::TextureSlot::Buttons);
	LoadMenuTexture(ref new Platform::String(L"ms-appx:///data/images/font/default8.png"), UwpUiRenderer::TextureSlot::Font);
	LoadMenuTexture(ref new Platform::String(L"ms-appx:///data/images/gui/default_world.png"), UwpUiRenderer::TextureSlot::DefaultWorld);
	LoadMenuTexture(ref new Platform::String(L"ms-appx:///data/images/gui/mojang_logo.png"), UwpUiRenderer::TextureSlot::MojangLogo);
	BeginOriginalClientTexturePreload();
	LoadTerrainTexture();
	LoadMenuFont();

	m_gameLoop.reset();
	m_frameClockStarted = false;
	m_renderToken = CompositionTarget::Rendering += ref new EventHandler<Object^>(this, &MainPage::OnRendering);
	// The original Minecraft client is the sole owner of both the main menu
	// and SelectWorld screens.  UwpUiRenderer remains available for the
	// in-world diagnostic/HUD overlay, but its legacy menu screens are no
	// longer presented during startup.
	m_ui.setScreen(UwpUiRenderer::Screen::Game);
	m_ui.setRealWorldRendering(false);
	// Use the original MCPE 0.6.1 bitmap-font metrics as the UWP baseline.
	m_ui.setFontProfile(UwpUiRenderer::FontProfile::Mcpe061);
	m_ui.setStatusText(L"EGL ready | NinecraftApp host + AppContext + I18n bound");
	m_platformSmokeResult = L"AppPlatform smoke: terrain preload pending";
	m_ui.setStatusDetailText(m_platformSmokeResult);
	SetStartupDiagnostic(L"Waiting for first EGL frame", L"Textures load asynchronously");
	UwpStoragePaths::ensureWorldsFolderAsync()
		.then([](Windows::Storage::StorageFolder^)
		{
			return UwpStoragePaths::ensureTemporaryWorldsFolderAsync();
		})
		.then([](Windows::Storage::StorageFolder^)
		{
			return UwpStoragePaths::ensureOptionsFileAsync();
		})
		.then([this](Windows::Storage::StorageFile^)
		{
			const UwpLevelDatSmokeResult levelDatResult = UwpLevelDatSmoke::run();
			if (!m_core.isActive())
			{
				m_ui.setStatusText(levelDatResult.succeeded
					? L"EGL ready | storage + level.dat ready | AppContext + I18n bound"
					: L"UWP level.dat smoke test failed");
			}
			SetStartupDiagnostic(L"Original level.dat storage smoke", levelDatResult.detail);
		}, task_continuation_context::use_current())
		.then([this](task<void> result)
		{
			try
			{
				result.get();
			}
			catch (Exception^)
			{
				m_ui.setStatusText(L"UWP save storage initialization failed");
			}
		}, task_continuation_context::use_current());
}

void MainPage::RunPlatformSmokeTest()
{
	if (!m_platform)
	{
		m_platformSmokeResult = L"AppPlatform smoke: unavailable";
		m_ui.setStatusDetailText(m_platformSmokeResult);
		return;
	}

	// Exercise the two original-client resource contracts without constructing
	// Minecraft: a byte asset relative to data/ and an RGBA texture relative to
	// data/images/.  Both allocations use the ownership expected by the
	// original callers, so release them here after validating their contents.
	BinaryBlob language = m_platform->readAssetFile("lang/en_US.lang");
	TextureData terrain = m_platform->loadTexture("terrain.png", true);
	const bool languageLoaded = language.data != nullptr && language.size > 0;
	const bool textureLoaded = terrain.data != nullptr
		&& terrain.w > 0
		&& terrain.h > 0
		&& terrain.numBytes == terrain.w * terrain.h * 4;

	std::wostringstream result;
	result << L"AppPlatform smoke: lang=" << (languageLoaded ? L"OK" : L"FAIL")
		<< L", terrain=" << (textureLoaded ? L"OK" : L"FAIL");
	if (textureLoaded)
		result << L" (" << terrain.w << L"x" << terrain.h << L")";
	else
	{
		result << L" [";
		for (unsigned char ch : m_platform->lastTextureLoadError())
			result << static_cast<wchar_t>(ch);
		result << L"]";
	}
	m_platformSmokeResult = result.str();

	delete[] language.data;
	delete[] terrain.data;
	m_ui.setStatusDetailText(m_platformSmokeResult);
	SetStartupDiagnostic(L"AppPlatform UWP smoke test", m_platformSmokeResult);
}

void MainPage::RunOriginalTexturesSmoke()
{
	m_originalTexturesSmokeDone = true;
	if (!m_platform)
	{
		m_ui.setStatusDetailText(L"Original Textures: platform unavailable");
		SetStartupDiagnostic(L"Original Textures smoke test",
			L"AppPlatform_uwp is unavailable");
		return;
	}

	// The EGL context is current because this is called from the frame callback
	// after beginFrame().  The renderer now owns the original Textures object
	// so its GL name remains valid for subsequent world draw calls.
	const bool succeeded = m_worldRenderer.loadOriginalTerrainTexture(m_platform.get());

	std::wostringstream status;
	status << L"Original Textures: terrain "
		<< (succeeded ? L"OK" : L"FAIL");
	if (succeeded)
		status << L" (" << m_worldRenderer.originalTerrainWidth()
		<< L"x" << m_worldRenderer.originalTerrainHeight() << L")";
	else
	{
		status << L" [";
		for (const unsigned char ch : m_platform->lastTextureLoadError())
			status << static_cast<wchar_t>(ch);
		status << L"]";
	}
	m_ui.setStatusDetailText(status.str());
	SetStartupDiagnostic(L"Original Textures smoke test", status.str());
}

void MainPage::RunOriginalFontSmoke()
{
	m_originalFontSmokeDone = true;
	if (!m_platform)
	{
		m_ui.setStatusDetailText(L"Original Font: platform unavailable");
		SetStartupDiagnostic(L"Original Font smoke test",
			L"AppPlatform_uwp is unavailable");
		return;
	}

	// Construct the original bitmap Font on the renderer-owned Textures object.
	// UwpUiRenderer will use it for one real game status-detail line; all other
	// menu and HUD elements remain on the existing UWP path.
	const bool loaded = m_worldRenderer.loadOriginalFont();
	m_ui.setOriginalFont(loaded ? m_worldRenderer.originalFont() : nullptr);
	const int width = m_worldRenderer.originalFontWidth("Singleplayer");
	const bool succeeded = loaded && width > 0;

	std::wostringstream status;
	status << L"Original Font: " << (succeeded ? L"OK" : L"FAIL");
	if (succeeded)
		status << L" (width=" << width << L", HUD draw armed)";
	else
		status << L" [font/default8.png unavailable or glyph metrics empty]";
	m_ui.setStatusDetailText(status.str());
	SetStartupDiagnostic(L"Original Font smoke test", status.str());
}

void MainPage::RunOriginalTesselatorSmoke()
{
	m_originalTesselatorSmokeDone = true;

	// Use the original CPU extraction contract first.  It expands one original
	// GL_QUADS primitive to six triangle vertices without touching the current
	// ANGLE shader or framebuffer, making this a safe isolated seam test.
	Tesselator& tesselator = Tesselator::instance;
	std::vector<std::uint8_t> vertexBytes;
	int vertexCount = 0;
	tesselator.begin();
	tesselator.color(255, 255, 255, 255);
	tesselator.vertexUV(0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
	tesselator.vertexUV(1.0f, 0.0f, 0.0f, 1.0f, 0.0f);
	tesselator.vertexUV(1.0f, 1.0f, 0.0f, 1.0f, 1.0f);
	tesselator.vertexUV(0.0f, 1.0f, 0.0f, 0.0f, 1.0f);
	tesselator.endToCPU(vertexBytes, vertexCount);

	const bool succeeded = vertexCount == 6 &&
		vertexBytes.size() == static_cast<std::size_t>(vertexCount) * sizeof(VERTEX);
	std::wostringstream status;
	status << L"Original Tesselator: "
		<< (succeeded ? L"OK" : L"FAIL");
	if (succeeded)
		status << L" (vertices=" << vertexCount << L")";
	else
		status << L" [quad expansion or vertex buffer size mismatch]";
	m_ui.setStatusDetailText(status.str());
	SetStartupDiagnostic(L"Original Tesselator smoke test", status.str());
}

void MainPage::RunOriginalGlesSmoke()
{
	m_originalGlesSmokeDone = true;

	// Initialization-only probe.  It deliberately does not bind the original
	// shader for a draw, touch MatrixStack, allocate Tesselator buffers, or
	// alter the current UWP terrain/UI programs.
	glInit();
	const bool succeeded = g_shader_program != 0 && g_shaderEnabled;
	const GLuint program = g_shader_program;
	shaderUnbind();

	std::wostringstream status;
	status << L"Original GLES2: " << (succeeded ? L"OK" : L"FAIL");
	if (succeeded)
		status << L" (program=" << program << L")";
	else
		status << L" [shader init/link failed]";
	m_ui.setStatusDetailText(status.str());
	SetStartupDiagnostic(L"Original GLES2 smoke test", status.str());
}

void MainPage::OnNavigatedTo(NavigationEventArgs^ e)
{
	(void)e;
	if (m_angle.isInitialized() && m_renderToken.Value == 0)
	{
		if (!m_originalClientHost.bind(m_platform.get(), m_angle))
		{
			SetStartupDiagnostic(L"Original client host binding failed",
				L"UWP EGL/AppPlatform or NinecraftApp construction is unavailable after navigation");
			return;
		}
		m_originalClientInitAttempted = false;
		AttachKeyboard();
		m_ui.setScreen(UwpUiRenderer::Screen::Game);
		m_frameClockStarted = false;
		m_renderToken = CompositionTarget::Rendering += ref new EventHandler<Object^>(this, &MainPage::OnRendering);
	}
}

void MainPage::OnNavigatedFrom(NavigationEventArgs^ e)
{
	(void)e;
	ResetTouchSlots();
	// The settings page is native XAML.  Stop the ANGLE frame callback while
	// it is visible, then resume it when this page comes back.
	if (m_renderToken.Value != 0)
	{
		CompositionTarget::Rendering -= m_renderToken;
		m_renderToken.Value = 0;
	}
	m_frameClockStarted = false;
	m_gameLoop.reset();
	m_originalClientHost.setWorldTickEnabled(false);
	m_originalClientHost.detachWorld();
	if (m_core.isActive())
	{
		m_worldRenderer.setLevel(nullptr);
		m_core.stop();
	}
	m_originalClientHost.clear();
	SyncOriginalMouseCursor();
	DetachKeyboard();
	m_core.setMoveInput(0.0f, 0.0f);
}

void MainPage::SaveActiveWorld()
{
	if (!m_core.isActive())
		return;

	// Suspension does not necessarily arrive from the CompositionTarget frame
	// callback.  Paint one complete save surface here before the synchronous
	// chunk/player flush so a suspension-triggered save never exposes the blue
	// ANGLE clear colour.
	RenderSavingWorldFrame(false);
	// Keep the world alive during suspension.  UwpMinecraftCore::saveGame()
	// performs the original chunk-source saveAll(true) before level/player
	// metadata, including the final population drain.
	m_core.saveGame();
}

void MainPage::RenderSavingWorldFrame(bool frameAlreadyBegun)
{
	if (!m_angle.isInitialized())
		return;

	const float width = static_cast<float>(AnglePanel->ActualWidth);
	const float height = static_cast<float>(AnglePanel->ActualHeight);
	if (width <= 0.0f || height <= 0.0f)
		return;
	if (!frameAlreadyBegun)
		m_angle.beginFrame(width, height);
	m_ui.renderSavingWorld(width, height);
	m_angle.present();
}

void MainPage::OnRendering(Object^, Object^)
{
	const auto renderCallbackStart = std::chrono::steady_clock::now();
	if (!m_angle.isInitialized())
	{
		SetStartupDiagnostic(L"Render callback skipped", L"ANGLE is not initialized");
		return;
	}

	const auto now = std::chrono::steady_clock::now();
	double elapsedSeconds = 0.0;
	if (m_frameClockStarted)
	{
		elapsedSeconds = std::chrono::duration<double>(now - m_lastFrameTime).count();
	}
	m_lastFrameTime = now;
	m_frameClockStarted = true;

	const float width = static_cast<float>(AnglePanel->ActualWidth);
	const float height = static_cast<float>(AnglePanel->ActualHeight);
	if (m_platform && width > 0.0f && height > 0.0f)
		m_platform->setScreenSize(static_cast<int>(width), static_cast<int>(height));
	// GameRenderer reads Minecraft::width/height for its projection, viewport,
	// GUI scale, picking and camera setup.  Keep that original client state in
	// lockstep with the current ANGLE surface before its update/render call.
	m_originalClientHost.setSize(static_cast<int>(width), static_cast<int>(height));
	m_angle.beginFrame(width, height);
	if (!m_originalTexturesSmokeDone &&
		m_clientTexturePreloadExpected > 0 &&
		m_clientTexturePreloadCompleted >= m_clientTexturePreloadExpected)
		RunOriginalTexturesSmoke();
	if (!m_originalFontSmokeDone && m_originalTexturesSmokeDone &&
		m_clientTexturePreloadCompleted >= m_clientTexturePreloadExpected)
		RunOriginalFontSmoke();
	if (!m_originalTesselatorSmokeDone && m_originalFontSmokeDone &&
		m_clientTexturePreloadCompleted >= m_clientTexturePreloadExpected)
		RunOriginalTesselatorSmoke();
	if (!m_originalGlesSmokeDone && m_originalTesselatorSmokeDone &&
		m_clientTexturePreloadCompleted >= m_clientTexturePreloadExpected)
		RunOriginalGlesSmoke();
	if (!m_originalClientInitAttempted &&
		m_clientTexturePreloadExpected > 0 &&
		m_clientTexturePreloadCompleted >= m_clientTexturePreloadExpected)
	{
		m_originalClientInitAttempted = true;
		if (m_clientTexturePreloadSucceeded >= m_clientTexturePreloadExpected &&
			m_originalClientHost.initializeClient())
		{
			m_originalMenuActive = m_originalClientHost.showStartMenu();
			m_core.bindOriginalClient(m_originalClientHost.minecraft());
			m_originalClientHost.setLoopEnabled(true);
			m_ui.setStatusText(L"Original Minecraft client loop enabled");
			SetStartupDiagnostic(L"Original Minecraft init OK",
				L"NinecraftApp::init + Minecraft::init; world tick handoff ready");
		}
		else
		{
			m_originalClientHost.setLoopEnabled(false);
			m_ui.setStatusText(L"Original Minecraft init deferred");
			SetStartupDiagnostic(L"Original Minecraft init skipped",
				L"Client texture preload did not complete successfully");
		}
	}
	// Advance while the EGL context is current.  In an attached world this
	// invokes the original Minecraft -> GameRenderer -> LevelRenderer chain;
	// OnGameRender only prepares the UWP HUD overlay afterward.
	m_gameLoop.advance(elapsedSeconds);
	SyncOriginalMouseCursor();
	if (m_core.isActive() && !m_originalMenuActive)
	{
		// Once the original client opens PauseScreen (or another in-game
		// Screen), it owns the entire in-game UI.  Do not paint the temporary
		// UWP HUD over the original screen.
		const Minecraft* minecraft = m_originalClientHost.minecraft();
		if (minecraft == nullptr || minecraft->screen == nullptr)
			m_ui.render(width, height);
	}
	else if (!m_core.isActive() && !m_worldCreationPending &&
		!m_originalMenuActive)
	{
		// Before the original client has created its first Screen, keep the
		// transition on the same MCPE dirt texture rather than exposing the
		// ANGLE clear color. Once showStartMenu() succeeds, the original frame
		// path owns the surface and this branch stops drawing.
		const Minecraft* minecraft = m_originalClientHost.minecraft();
		if (minecraft == nullptr || minecraft->screen == nullptr)
		{
			if (!m_originalClientInitAttempted)
				m_ui.renderStartupLoading(width, height);
			else
				m_ui.renderLoadingBackground(width, height);
		}
	}
	m_angle.present();
	const double frameMilliseconds = std::chrono::duration<double, std::milli>(
		std::chrono::steady_clock::now() - renderCallbackStart).count();
	if (frameMilliseconds > m_maxFrameMilliseconds)
		m_maxFrameMilliseconds = frameMilliseconds;
	if (m_angle.lastError() != EGL_SUCCESS)
	{
		SetStartupDiagnostic(L"EGL present failed",
			L"eglSwapBuffers error: 0x" +
			std::to_wstring(static_cast<unsigned int>(m_angle.lastError())));
	}
	if (!m_firstFramePresented)
	{
		m_firstFramePresented = true;
		SetStartupDiagnostic(L"First EGL frame presented", L"If the screen is still white, this is likely a surface/overlay or stale-package issue");
	}

	// Keep diagnostics to the existing lower second line.  Update once per
	// second so the measurement itself does not add noticeable UI churn.
	if (m_core.isActive() && m_core.tickCount() >= m_lastPerformanceDisplayTick + 20)
	{
		m_lastPerformanceDisplayTick = m_core.tickCount();
		std::wostringstream performance;
		performance << std::fixed << std::setprecision(1)
			<< L"max ms  L:" << m_core.maxLevelTickMilliseconds()
			<< L" P:" << m_core.maxPlayerTickMilliseconds()
			<< L" M:" << m_worldRenderer.maxMeshBuildMilliseconds()
			<< L" R:" << m_worldRenderer.maxRenderMilliseconds()
			<< L" F:" << m_maxFrameMilliseconds;
		m_ui.setStatusDetailText(performance.str());
	}
}

void MainPage::SetStartupDiagnostic(const std::wstring& stage, const std::wstring& details)
{
	if (StartupStage != nullptr)
		StartupStage->Text = ref new Platform::String(stage.c_str());
	if (StartupDetails != nullptr)
		StartupDetails->Text = ref new Platform::String(details.c_str());
}

void MainPage::SyncOriginalMouseCursor()
{
	if (Window::Current == nullptr || Window::Current->CoreWindow == nullptr)
		return;

	Minecraft* minecraft = m_originalClientHost.minecraft();
	const bool shouldHide =
		m_originalClientHost.isOriginalWorldTickEnabled() &&
		minecraft != nullptr && minecraft->mouseGrabbed &&
		minecraft->screen == nullptr;
	if (shouldHide == m_originalPointerCursorHidden)
		return;

	// A menu/pause cursor position is screen-space state, whereas the original
	// in-world mouse path consumes relative deltas.  Discard the old position
	// whenever ownership changes so returning from PauseScreen cannot produce a
	// camera jump from the distance between the button and the crosshair.
	m_originalPointerX = -1.0f;
	m_originalPointerY = -1.0f;
	if (shouldHide)
	{
		// Relative MouseDevice events drive the original Mouse delta queue
		// while the cursor is hidden.  Do not warp the pointer here: a warp
		// generates an absolute PointerMoved event and can be interpreted as a
		// large pitch change when the world is first entered.
		Window::Current->CoreWindow->PointerCursor = nullptr;
		m_originalPointerCursorHidden = true;
	}
	else
	{
		// Do not release all XAML pointer captures from the CompositionTarget
		// Rendering callback.  This callback can run while XAML is dispatching a
		// pointer event (for example when WorkbenchScreen opens), and releasing
		// captures re-entrantly can raise a fail-fast COM/XAML exception.  The
		// normal pointer-release handler releases the individual capture; menu
		// screens do not need a blanket release here.
		Window::Current->CoreWindow->PointerCursor =
			ref new CoreCursor(CoreCursorType::Arrow, 0);
		m_originalPointerCursorHidden = false;
	}
}

void MainPage::BeginWorldLoad(const std::wstring& worldId,
	const std::wstring& worldName, bool createNew, long seed,
	int gameType, int worldType)
{
	// The menu cursor position is not a valid first in-world delta.  Reset it
	// before the next mouse move so entering a world cannot rotate the camera
	// by the distance from the old menu pointer location.
	m_originalPointerX = -1.0f;
	m_originalPointerY = -1.0f;
	m_originalMenuActive = false;
	// Keep the original client frame loop alive while UwpCoreSession performs
	// the asynchronous storage/world handoff. ProgressScreen is the original
	// loading surface for this no-Level interval; attachUwpWorld() removes it
	// once the real Level and LocalPlayer are ready.
	m_originalClientHost.setLoopEnabled(
		m_originalClientHost.showProgressScreen());
	m_ui.setScreen(UwpUiRenderer::Screen::Game);
	m_ui.setRealWorldRendering(false);
	m_worldRenderer.setLevel(nullptr);
	m_worldCreationPending = true;
	m_ui.setStatusText(createNew ? L"Generating original MCPE world..." :
		L"Loading original MCPE save...");
	create_task([this, worldId, worldName, createNew, seed, gameType, worldType]()
	{
		if (createNew)
			m_core.startNewWorld(worldId, worldName, seed, gameType, worldType);
		else
			m_core.startSavedWorld(worldId, worldName);
	}).then([this](task<void> result)
	{
		try
		{
			result.get();
			if (!m_core.isActive() || !m_core.nativeWorldGenerated())
			{
				if (m_core.legacyStorageRejected())
				{
					m_ui.setRealWorldRendering(false);
					m_ui.setScreen(UwpUiRenderer::Screen::Game);
					m_originalPointerX = -1.0f;
					m_originalPointerY = -1.0f;
					m_originalClientHost.setLoopEnabled(true);
					m_originalMenuActive = m_originalClientHost.showLegacyWorldFormatError();
					if (!m_originalMenuActive)
						ShowOriginalWorldSelection();
				}
				else
				{
					ShowOriginalWorldSelection();
				}
				m_ui.setStatusText(m_core.errorText().empty() ?
					L"MCPE world load failed" : m_core.errorText());
			}
			else if (!AttachOriginalWorldForTick())
			{
				m_worldRenderer.setLevel(nullptr);
				m_core.stop();
				ShowOriginalWorldSelection();
				m_ui.setStatusText(L"Original Minecraft world attach failed");
			}
			else
			{
				m_ui.setGameTelemetry(m_core.worldName(), m_core.tickCount(),
					m_core.playerX(), m_core.playerZ(), m_core.dayTime());
				m_ui.setStatusText(L"MCPE saved world loaded | original tick active");
			}
		}
		catch (...)
		{
			ShowOriginalWorldSelection();
				m_ui.setStatusText(L"MCPE world load failed");
		}
		m_worldCreationPending = false;
	}, task_continuation_context::use_current());
}

void MainPage::ShowOriginalWorldSelection()
{
	// Keep the custom UWP world list out of the normal flow.  The original
	// SelectWorldScreen reads the same canonical minecraftWorlds directory and
	// remains the only visible world-selection UI.
	m_ui.setRealWorldRendering(false);
	m_ui.setScreen(UwpUiRenderer::Screen::Game);
	m_originalPointerX = -1.0f;
	m_originalPointerY = -1.0f;
	m_originalClientHost.setLoopEnabled(true);
	m_originalMenuActive = m_originalClientHost.showSelectWorld();
	if (!m_originalMenuActive)
	{
		m_originalClientHost.setLoopEnabled(false);
		m_ui.setStatusText(L"Original world-selection screen unavailable");
	}
}

void MainPage::SetSkinStatus(const std::wstring& text)
{
	if (SkinStatusText != nullptr)
		SkinStatusText->Text = ref new String(text.c_str());
}

void MainPage::ShowSkinPage()
{
	if (SkinOverlay == nullptr)
		return;

	m_skinPageVisible = true;
	m_skinArmTypeLoading = true;
	Minecraft* minecraft = m_originalClientHost.minecraft();
	SkinArmTypeComboBox->SelectedIndex =
		minecraft != nullptr && minecraft->options.slimSkin ? 1 : 0;
	m_skinArmTypeLoading = false;
	SkinOverlay->Visibility = Windows::UI::Xaml::Visibility::Visible;
	RebuildSkinDirtBackground();
	AnimateSkinPage(true);
	SetSkinStatus(UwpStoragePaths::localSkinExistsBlocking()
		? L"Custom skin is active."
		: L"Using the default Steve skin.");
	LoadSkinPreview();
}

void MainPage::HideSkinPage()
{
	m_skinPageVisible = false;
	AnimateSkinPage(false);
}

void MainPage::ShowKeyMappingPage()
{
	if (KeyMappingOverlay == nullptr || KeyMappingFrame == nullptr)
		return;

	if (m_keyMappingPage == nullptr)
	{
		m_keyMappingPage = ref new KeyMappingPage();
		m_keyMappingPage->BackRequested +=
			ref new EventHandler<Object^>([this](Object^, Object^)
			{
				HideKeyMappingPage();
			});
	}
	m_keyMappingPage->SetClientHandle(
		reinterpret_cast<unsigned long long>(m_originalClientHost.handle()));
	KeyMappingFrame->Content = m_keyMappingPage;
	m_keyMappingPageVisible = true;
	KeyMappingOverlay->Visibility = Windows::UI::Xaml::Visibility::Visible;
	RebuildSkinDirtBackground();
	AnimateOverlay(KeyMappingOverlay, true);
	Keyboard::reset();
}

void MainPage::HideKeyMappingPage()
{
	m_keyMappingPageVisible = false;
	if (KeyMappingOverlay != nullptr)
		AnimateOverlay(KeyMappingOverlay, false);
	Keyboard::reset();
	UpdateMoveInput();
}

void MainPage::ShowWorldEditPage(const std::string& worldId)
{
	WorldEditDiagnostic(L"ShowWorldEditPage entered");
	if (WorldEditOverlay == nullptr || WorldEditFrame == nullptr || worldId.empty())
	{
		WorldEditDiagnostic(L"ShowWorldEditPage returned: missing overlay/frame/world id");
		return;
	}

	// The action is consumed from the original client's tick.  Defer the XAML
	// tree mutation to the next dispatcher turn so it cannot happen while the
	// original screen is still processing the same button event.
	const std::string requestedWorldId = worldId;
	// The request arrives from the original client menu tick.  Keep that
	// screen in place while the XAML tree is switched, but stop its update and
	// render callbacks immediately.  Without this pause the non-debug build
	// can process the same menu transition while the dispatcher is mounting
	// the overlay, leaving the UI apparently frozen.
	m_worldEditSuspendedOriginalLoop =
		m_originalClientHost.isOriginalClientLoopEnabled();
	if (m_worldEditSuspendedOriginalLoop)
	{
		m_originalClientHost.setLoopEnabled(false);
		WorldEditDiagnostic(L"original client loop paused for world edit");
	}
	WorldEditDiagnostic(L"before Dispatcher->RunAsync");
	Dispatcher->RunAsync(CoreDispatcherPriority::Normal,
		ref new DispatchedHandler([this, requestedWorldId]()
		{
			try
			{
				WorldEditDiagnostic(L"dispatcher callback begin");
				if (WorldEditOverlay == nullptr || WorldEditFrame == nullptr)
				{
					WorldEditDiagnostic(L"callback returned: missing overlay/frame");
					return;
				}

				if (m_worldEditShellPage == nullptr)
				{
					WorldEditDiagnostic(L"before new WorldEditShellPage");
					m_worldEditShellPage = ref new WorldEditShellPage();
					WorldEditDiagnostic(L"after new WorldEditShellPage");
					WorldEditDiagnostic(L"before BackRequested subscription");
					m_worldEditShellPage->BackRequested +=
						ref new EventHandler<Object^>([this](Object^, Object^)
						{
							HideWorldEditPage();
						});
					WorldEditDiagnostic(L"after BackRequested subscription");
				}
				const std::wstring wideId(requestedWorldId.begin(), requestedWorldId.end());
				WorldEditDiagnostic(L"before WorldEditFrame->Content");
				WorldEditFrame->Content = m_worldEditShellPage;
				WorldEditDiagnostic(L"after WorldEditFrame->Content");
				// Mount the page before populating it.  This is important on the older
				// UWP XAML runtime: touching child controls while a new Page is not
				// yet in its Frame can fail-fast inside Windows.UI.Xaml.
				WorldEditDiagnostic(L"before SetWorldId");
				m_worldEditShellPage->SetWorldId(ref new String(wideId.c_str()));
				WorldEditDiagnostic(L"after SetWorldId");
				m_worldEditPageVisible = true;
				WorldEditDiagnostic(L"before WorldEditOverlay visible");
				WorldEditOverlay->Visibility = Windows::UI::Xaml::Visibility::Visible;
				WorldEditDiagnostic(L"after WorldEditOverlay visible");
				// WorldEditDirtBackground is intentionally not rebuilt from its own
				// SizeChanged event.  Older UWP XAML versions can fail-fast when that
				// handler changes Canvas.Children during layout.  Wait until this
				// dispatcher turn has completed layout instead.
				WorldEditDiagnostic(L"before dirt background rebuild scheduling");
				ScheduleWorldEditDirtBackgroundRebuild();
				WorldEditDiagnostic(L"after dirt background rebuild scheduling");
				WorldEditDiagnostic(L"before AnimateOverlay");
				AnimateOverlay(WorldEditOverlay, true);
				WorldEditDiagnostic(L"after AnimateOverlay");
				Keyboard::reset();
				WorldEditDiagnostic(L"dispatcher callback complete");
			}
			catch (Exception^ error)
			{
				WorldEditExceptionDiagnostic(error);
				m_worldEditPageVisible = false;
				if (WorldEditOverlay != nullptr)
					WorldEditOverlay->Visibility = Windows::UI::Xaml::Visibility::Collapsed;
				if (m_worldEditSuspendedOriginalLoop)
				{
					m_originalClientHost.setLoopEnabled(true);
					m_originalMenuActive = m_originalClientHost.showSelectWorld();
					m_worldEditSuspendedOriginalLoop = false;
				}
				m_ui.setStatusText(L"World editor could not be opened");
			}
			catch (...)
			{
				WorldEditDiagnostic(L"unknown non-WinRT exception");
				m_worldEditPageVisible = false;
				if (WorldEditOverlay != nullptr)
					WorldEditOverlay->Visibility = Windows::UI::Xaml::Visibility::Collapsed;
				if (m_worldEditSuspendedOriginalLoop)
				{
					m_originalClientHost.setLoopEnabled(true);
					m_originalMenuActive = m_originalClientHost.showSelectWorld();
					m_worldEditSuspendedOriginalLoop = false;
				}
				m_ui.setStatusText(L"World editor could not be opened");
			}
		}));
}

void MainPage::HideWorldEditPage()
{
	m_worldEditPageVisible = false;
	if (WorldEditOverlay != nullptr)
		AnimateOverlay(WorldEditOverlay, false);
	Keyboard::reset();
	UpdateMoveInput();

	// Recreate the original selection screen so folder/name/deletion changes
	// are reflected immediately when the XAML page closes.
	if (m_worldEditSuspendedOriginalLoop)
	{
		m_originalClientHost.setLoopEnabled(true);
		m_originalMenuActive = m_originalClientHost.showSelectWorld();
		m_worldEditSuspendedOriginalLoop = false;
	}
}

void MainPage::ShowWorldImportPage()
{
	if (WorldImportOverlay == nullptr || WorldImportFrame == nullptr)
		return;
	if (m_worldImportPage == nullptr)
		m_worldImportPage = ref new WorldImportPage();
	WorldImportFrame->Content = m_worldImportPage;
	m_worldImportPageVisible = true;
	m_worldImportSuspendedOriginalLoop = m_originalMenuActive;
	if (m_worldImportSuspendedOriginalLoop)
	{
		m_originalClientHost.setLoopEnabled(false);
		m_originalMenuActive = false;
	}
	WorldImportOverlay->Visibility = Windows::UI::Xaml::Visibility::Visible;
	RebuildSkinDirtBackground();
	AnimateOverlay(WorldImportOverlay, true);
	Keyboard::reset();
}

void MainPage::HideWorldImportPage()
{
	m_worldImportPageVisible = false;
	if (WorldImportOverlay != nullptr)
		AnimateOverlay(WorldImportOverlay, false);
	Keyboard::reset();
	UpdateMoveInput();
	if (m_worldImportSuspendedOriginalLoop)
	{
		m_originalClientHost.setLoopEnabled(true);
		m_originalMenuActive = m_originalClientHost.showSelectWorld();
		m_worldImportSuspendedOriginalLoop = false;
	}
}

void MainPage::ScheduleWorldEditDirtBackgroundRebuild()
{
	if (m_worldEditDirtRebuildPending || Dispatcher == nullptr)
		return;

	m_worldEditDirtRebuildPending = true;
	Dispatcher->RunAsync(CoreDispatcherPriority::Low,
		ref new DispatchedHandler([this]()
		{
			m_worldEditDirtRebuildPending = false;
			if (!m_worldEditPageVisible || WorldEditDirtBackground == nullptr)
			{
				m_worldEditDirtRebuildAttempts = 0;
				return;
			}

			// The first dispatcher turn can run before the newly visible overlay
			// has received its final layout size.  Retry a few later turns instead
			// of rebuilding an empty Canvas and waiting for the next page visit.
			if (WorldEditDirtBackground->ActualWidth <= 0.0 ||
				WorldEditDirtBackground->ActualHeight <= 0.0)
			{
				if (m_worldEditDirtRebuildAttempts++ < 4)
					ScheduleWorldEditDirtBackgroundRebuild();
				else
					m_worldEditDirtRebuildAttempts = 0;
				return;
			}

			m_worldEditDirtRebuildAttempts = 0;
			RebuildSkinDirtBackground();
		}));
}

void MainPage::AnimateSkinPage(bool entering)
{
	AnimateOverlay(SkinOverlay, entering);
}

void MainPage::AnimateOverlay(
	Windows::UI::Xaml::Controls::Grid^ overlay, bool entering)
{
	if (overlay == nullptr)
		return;

	if (m_skinPageTransition != nullptr)
	{
		m_skinPageTransition->Stop();
		if (m_skinPageTransitionCompletedToken.Value != 0)
		{
			m_skinPageTransition->Completed -=
				m_skinPageTransitionCompletedToken;
			m_skinPageTransitionCompletedToken.Value = 0;
		}
		m_skinPageTransition = nullptr;
	}

	if (entering)
		overlay->Opacity = 0.0;

	auto transition = ref new Storyboard();
	auto fade = ref new DoubleAnimation();
	fade->From = entering ? 0.0 : overlay->Opacity;
	fade->To = entering ? 1.0 : 0.0;
	TimeSpan duration;
	duration.Duration = entering ? 2200000 : 1800000;
	fade->Duration = duration;
	Storyboard::SetTarget(fade, overlay);
	Storyboard::SetTargetProperty(fade, ref new String(L"Opacity"));
	transition->Children->Append(fade);

	m_skinPageTransition = transition;
	m_skinPageTransitionCompletedToken = transition->Completed +=
		ref new EventHandler<Object^>([this, transition, overlay, entering](Object^, Object^)
		{
			if (!entering && overlay != nullptr)
			{
				overlay->Visibility = Windows::UI::Xaml::Visibility::Collapsed;
				overlay->Opacity = 1.0;
			}

			if (m_skinPageTransition == transition)
			{
				if (m_skinPageTransitionCompletedToken.Value != 0)
				{
					transition->Completed -=
						m_skinPageTransitionCompletedToken;
					m_skinPageTransitionCompletedToken.Value = 0;
				}
				m_skinPageTransition = nullptr;
			}
		});
	transition->Begin();
}

void MainPage::OnSkinBackgroundSizeChanged(Object^, SizeChangedEventArgs^)
{
	RebuildSkinDirtBackground();
}

void MainPage::RebuildSkinDirtBackground()
{
	if (SkinDirtBackground == nullptr && KeyMappingDirtBackground == nullptr &&
		WorldEditDirtBackground == nullptr && WorldImportDirtBackground == nullptr)
		return;
	if (m_rebuildingSkinDirtBackground)
		return;
	m_rebuildingSkinDirtBackground = true;

	// The source texture is 16x16. XAML uses device-independent pixels while
	// the native menu is rendered in physical pixels, so 64 XAML pixels gives
	// the same visual texture scale as the native menu on the target display.
	const double tileSize = 64.0;
	auto source = ref new BitmapImage();
	source->UriSource = ref new Uri(
		L"ms-appx:///data/images/gui/background.png");
	auto rebuild = [source, tileSize](Canvas^ canvas)
	{
		if (canvas == nullptr)
			return;
		if (canvas->ActualWidth <= 0.0 || canvas->ActualHeight <= 0.0)
			return;
		const int columns = static_cast<int>(canvas->ActualWidth / tileSize) + 1;
		const int rows = static_cast<int>(canvas->ActualHeight / tileSize) + 1;
		canvas->Children->Clear();
		for (int row = 0; row < rows; ++row)
		{
			for (int column = 0; column < columns; ++column)
			{
				auto tile = ref new Image();
				tile->Width = tileSize;
				tile->Height = tileSize;
				tile->Stretch = Stretch::Fill;
				tile->Source = source;
				Canvas::SetLeft(tile, column * tileSize);
				Canvas::SetTop(tile, row * tileSize);
				canvas->Children->Append(tile);
			}
		}
	};
	rebuild(SkinDirtBackground);
	rebuild(KeyMappingDirtBackground);
	rebuild(WorldEditDirtBackground);
	rebuild(WorldImportDirtBackground);
	if (SkinDirtBackground != nullptr)
	{
		m_skinDirtColumns = static_cast<int>(SkinDirtBackground->ActualWidth / tileSize) + 1;
		m_skinDirtRows = static_cast<int>(SkinDirtBackground->ActualHeight / tileSize) + 1;
	}
	m_rebuildingSkinDirtBackground = false;
}

void MainPage::OnSkinBackClick(Object^, RoutedEventArgs^)
{
	if (!m_skinOperationPending)
		HideSkinPage();
}

void MainPage::OnSkinArmTypeChanged(Object^, SelectionChangedEventArgs^)
{
	if (m_skinArmTypeLoading || m_skinOperationPending || SkinArmTypeComboBox == nullptr)
		return;

	const bool slim = SkinArmTypeComboBox->SelectedIndex == 1;
	if (m_originalClientHost.setSkinArmType(slim))
	{
		UwpSettingsModel settings;
		settings.load();
		settings.skinArmType = slim ? 1 : 0;
		settings.save();
		SetSkinStatus(slim ? L"Slim arm type selected." : L"Classic arm type selected.");
	}
}

task<void> MainPage::ImportSkinFileAsync(StorageFile^ file)
{
	return create_task(file->OpenAsync(FileAccessMode::Read))
		.then([](IRandomAccessStream^ stream)
		{
			return create_task(BitmapDecoder::CreateAsync(stream));
		}, task_continuation_context::use_current())
		.then([this, file](BitmapDecoder^ decoder) -> task<void>
		{
			if (decoder->PixelWidth != 64 || decoder->PixelHeight != 64)
			{
				SetSkinStatus(L"Skin must be a 64x64 PNG file.");
				return task_from_result();
			}

			auto transform = ref new BitmapTransform();
			return create_task(decoder->GetPixelDataAsync(
				BitmapPixelFormat::Rgba8,
				BitmapAlphaMode::Straight,
				transform,
				ExifOrientationMode::IgnoreExifOrientation,
				ColorManagementMode::DoNotColorManage))
				.then([this, file](PixelDataProvider^ provider) -> task<void>
				{
					auto rawPixels = provider->DetachPixelData();
					std::vector<unsigned char> pixels(
						rawPixels->Data,
						rawPixels->Data + rawPixels->Length);
					return UwpStoragePaths::ensureSkinFolderAsync()
						.then([this, file, pixels](StorageFolder^ folder)
						{
							return create_task(file->CopyAsync(
								folder,
								ref new String(L"local.png"),
								NameCollisionOption::ReplaceExisting))
								.then([this, pixels](StorageFile^)
								{
									if (m_platform)
										m_platform->cacheTexturePixels(
											"skins/local.png", true, 64, 64,
											pixels.data(),
											static_cast<unsigned int>(pixels.size()));
									// Texture eviction touches OpenGL state.  Queue it for
									// the next UWP game tick, where MainPage has already
									// made the ANGLE context current.
									m_pendingSkinAction = PendingSkinAction::Apply;
									LoadSkinPreview();
									SetSkinStatus(L"Custom skin imported and applied.");
								}, task_continuation_context::use_current());
						}, task_continuation_context::use_current());
				}, task_continuation_context::use_current());
		}, task_continuation_context::use_current());
}

void MainPage::OnSkinImportClick(Object^, RoutedEventArgs^)
{
	if (m_skinOperationPending)
		return;

	m_skinOperationPending = true;
	SetSkinStatus(L"Choose a 64x64 PNG skin...");
	auto picker = ref new Windows::Storage::Pickers::FileOpenPicker();
	picker->SuggestedStartLocation =
		Windows::Storage::Pickers::PickerLocationId::PicturesLibrary;
	picker->ViewMode = Windows::Storage::Pickers::PickerViewMode::Thumbnail;
	picker->FileTypeFilter->Append(ref new String(L".png"));

	create_task(picker->PickSingleFileAsync())
		.then([this](StorageFile^ file) -> task<void>
		{
			if (file == nullptr)
			{
				SetSkinStatus(L"Skin import cancelled.");
				return task_from_result();
			}
			return ImportSkinFileAsync(file);
		}, task_continuation_context::use_current())
		.then([this](task<void> result)
		{
			try
			{
				result.get();
			}
			catch (...)
			{
				SetSkinStatus(L"Skin import failed. Check that the file is a valid PNG.");
			}
			m_skinOperationPending = false;
		}, task_continuation_context::use_current());
}

void MainPage::OnSkinClearClick(Object^, RoutedEventArgs^)
{
	if (m_skinOperationPending)
		return;

	m_skinOperationPending = true;
	// Invalidate an in-flight preview read before deleting its backing file.
	// Otherwise the old BitmapImage task can race the delete and raise a
	// first-chance 0x80070002 COM exception.
	++m_skinPreviewGeneration;
	if (SkinPreview != nullptr)
		SkinPreview->Source = nullptr;
	SetSkinStatus(L"Clearing custom skin...");
	if (!UwpStoragePaths::localSkinExistsBlocking())
	{
		if (m_platform)
			m_platform->clearTexturePreload("skins/local.png", true);
		m_pendingSkinAction = PendingSkinAction::Clear;
		LoadSkinPreview();
		SetSkinStatus(L"Custom skin cleared.");
		m_skinOperationPending = false;
		return;
	}

	create_task(StorageFolder::GetFolderFromPathAsync(
		UwpStoragePaths::skinFolderPath()))
		.then([](StorageFolder^ folder)
		{
			// TryGetItemAsync returns null when local.png disappeared between
			// the existence check and this operation, without throwing COM
			// exception 0x80070002.
			return create_task(folder->TryGetItemAsync(
				ref new String(L"local.png")));
		})
		.then([](IStorageItem^ item) -> task<void>
		{
			if (item == nullptr)
				return task_from_result();
			return create_task(item->DeleteAsync());
		}, task_continuation_context::use_current())
		.then([this](task<void> result)
		{
			try
			{
				result.get();
			}
			catch (...)
			{
				SetSkinStatus(L"Could not clear the custom skin file.");
				m_skinOperationPending = false;
				return;
			}

			if (m_platform)
				m_platform->clearTexturePreload("skins/local.png", true);
			m_pendingSkinAction = PendingSkinAction::Clear;
			LoadSkinPreview();
			SetSkinStatus(L"Custom skin cleared.");
			m_skinOperationPending = false;
		}, task_continuation_context::use_current());
}

void MainPage::LoadSkinPreview()
{
	if (SkinPreview == nullptr)
		return;

	const std::uint64_t previewGeneration = ++m_skinPreviewGeneration;

	if (!UwpStoragePaths::localSkinExistsBlocking())
	{
		auto fallback = ref new BitmapImage();
		fallback->UriSource = ref new Uri(
			L"ms-appx:///data/images/mob/char.png");
		SkinPreview->Source = fallback;
		return;
	}

	auto bitmap = ref new BitmapImage();
	bitmap->CreateOptions = BitmapCreateOptions::IgnoreImageCache;
	SkinPreview->Source = bitmap;
	create_task(StorageFolder::GetFolderFromPathAsync(
		UwpStoragePaths::skinFolderPath()))
		.then([](StorageFolder^ folder)
		{
			return create_task(folder->TryGetItemAsync(
				ref new String(L"local.png")));
		})
		.then([this, bitmap, previewGeneration](IStorageItem^ item) -> task<void>
		{
			if (item == nullptr || previewGeneration != m_skinPreviewGeneration)
				return task_from_result();
			StorageFile^ file = dynamic_cast<StorageFile^>(item);
			if (file == nullptr)
				return task_from_result();
			return create_task(file->OpenAsync(FileAccessMode::Read))
				.then([this, bitmap, previewGeneration](IRandomAccessStream^ stream)
				{
					if (stream == nullptr ||
						previewGeneration != m_skinPreviewGeneration)
						return task_from_result();
					return create_task(bitmap->SetSourceAsync(stream));
				}, task_continuation_context::use_current());
		}, task_continuation_context::use_current())
		.then([](task<void> result)
		{
			try { result.get(); }
			catch (...) { }
		}, task_continuation_context::use_current());
}

void MainPage::OnGameTick(std::uint64_t tick)
{
	// The import page pauses the original menu loop while it is visible, so
	// its Back request must be consumed outside the normal menu-update branch.
	if (!m_originalClientHost.isOriginalClientLoopEnabled())
	{
		const UwpOriginalMenuAction pausedAction =
			m_originalClientHost.consumeMenuAction();
		if (pausedAction == UwpOriginalMenuAction::CloseWorldImport)
		{
			HideWorldImportPage();
			return;
		}
	}

	if (m_pendingSkinAction != PendingSkinAction::None)
	{
		const PendingSkinAction action = m_pendingSkinAction;
		m_pendingSkinAction = PendingSkinAction::None;
		if (action == PendingSkinAction::Apply)
			m_originalClientHost.applyImportedSkin();
		else if (action == PendingSkinAction::Clear)
			m_originalClientHost.clearImportedSkin();
	}

	if (m_originalClientHost.isOriginalClientLoopEnabled() &&
		!m_core.isActive())
	{
		// The original client owns only menu/client state here.  World loading
		// disables this branch before UwpCoreSession becomes active.  Menus keep
		// the normal Timer-driven update path.
		m_originalClientHost.update();
		if (m_originalClientHost.wantsToQuit())
		{
			// The original menu buttons set App::_finished through
			// Minecraft::quit(). Consume that request at the UWP boundary so
			// both PE's X button and Java's Quit Game button close the app.
			SaveActiveWorld();
			Windows::ApplicationModel::Core::CoreApplication::Exit();
			return;
		}
		Minecraft* originalMinecraft = m_originalClientHost.minecraft();
		// A remote JoinGameScreen reaches the normal original
		// ClientSideNetworkHandler -> MultiPlayerLevel -> setLevel path.  It is
		// not a UwpCoreSession world, so switch input/cursor ownership to the
		// original client once its generation callback has completed.
		if (originalMinecraft != nullptr && originalMinecraft->isLevelGenerated())
		{
			m_originalMenuActive = false;
			m_originalClientHost.setWorldTickEnabled(true);
		}
		else if (originalMinecraft != nullptr && originalMinecraft->level == nullptr &&
			!m_originalMenuActive && !m_worldCreationPending)
		{
			m_originalClientHost.setWorldTickEnabled(false);
			m_originalMenuActive = m_originalClientHost.showStartMenu();
		}
		if (m_originalMenuActive)
			m_originalClientHost.ensureStartMenu();
		const UwpOriginalMenuAction menuAction =
			m_originalClientHost.consumeMenuAction();
		if (menuAction == UwpOriginalMenuAction::OpenSkin)
		{
			ShowSkinPage();
			return;
		}
		if (menuAction == UwpOriginalMenuAction::OpenKeyMapping)
		{
			ShowKeyMappingPage();
			return;
		}
		if (menuAction == UwpOriginalMenuAction::OpenWorldEdit)
		{
			std::string worldId;
			if (m_originalClientHost.consumeWorldEdit(worldId))
				ShowWorldEditPage(worldId);
			return;
		}
		if (menuAction == UwpOriginalMenuAction::OpenWorldImport)
		{
			ShowWorldImportPage();
			return;
		}
		if (menuAction == UwpOriginalMenuAction::CloseWorldImport)
		{
			HideWorldImportPage();
			return;
		}
		if (menuAction == UwpOriginalMenuAction::LeaveWorld)
		{
			// Minecraft::leaveGame() has already detached the borrowed pointers
			// without deleting them.  Clear renderer references before the owning
			// UWP session saves and destroys the Level/LocalPlayer.
			m_originalClientHost.setWorldTickEnabled(false);
			m_originalClientHost.detachWorld();
			m_worldRenderer.setLevel(nullptr);
			if (m_core.isActive())
			{
				RenderSavingWorldFrame(true);
				m_core.stop();
			}
			m_worldCreationPending = false;
			m_originalClientHost.setLoopEnabled(true);
			m_originalMenuActive = m_originalClientHost.showStartMenu();
			return;
		}
		if (menuAction == UwpOriginalMenuAction::Singleplayer)
		{
			// Continue through the original SelectWorldScreen.  The UWP shell
			// remains outside the original client screen until that screen
			// selects a level.
			m_originalMenuActive = m_originalClientHost.showSelectWorld();
		}
		else if (menuAction == UwpOriginalMenuAction::SelectWorld)
		{
			std::string levelId;
			std::string levelName;
			long seed = -1;
			int gameType = GameType::Undefined;
			int worldType = WorldType::Old;
			if (m_originalClientHost.consumeLevelSelection(
				levelId, levelName, seed, gameType, worldType))
			{
				const std::wstring worldId(levelId.begin(), levelId.end());
				const std::wstring worldName(levelName.begin(), levelName.end());
				const bool createNew = gameType != GameType::Undefined;
				BeginWorldLoad(worldId, worldName, createNew, seed,
					gameType, worldType);
			}
		}
		return;
	}
	if (m_originalClientHost.isOriginalWorldTickEnabled())
	{
		// The original client now owns Level/LocalPlayer simulation.  Keep the
		// UWP scheduler as the single frame source, but route this tick through
		// NinecraftApp::updateFixedStep -> Minecraft::updateUwpFixedStep ->
		// Minecraft::tick.  This avoids a second wall-clock Timer cadence.
		m_originalClientHost.fixedTick();
		const UwpOriginalMenuAction worldAction =
			m_originalClientHost.consumeMenuAction();
		if (worldAction == UwpOriginalMenuAction::LeaveWorld)
		{
			// Minecraft::leaveGame() detached the borrowed pointers from the
			// original client.  UwpCoreSession remains the owner and performs the
			// save/destruction here, after all renderer references are cleared.
			m_originalClientHost.setWorldTickEnabled(false);
			m_originalClientHost.detachWorld();
			m_worldRenderer.setLevel(nullptr);
			if (m_core.isActive())
			{
				RenderSavingWorldFrame(true);
				m_core.stop();
			}
			m_worldCreationPending = false;
			m_originalClientHost.setLoopEnabled(true);
			m_originalMenuActive = m_originalClientHost.showStartMenu();
			return;
		}
		m_core.syncOriginalTick(tick);
		if (m_core.isActive())
		{
			m_ui.setGameTelemetry(m_core.worldName(), m_core.tickCount(),
				m_core.playerX(), m_core.playerZ(), m_core.dayTime());
		}
		return;
	}
	// Minecraft::tickInput() will own this reset once the original client loop
	// is active.  Until then, bound unconsumed UWP key-repeat events while
	// retaining Keyboard::_states for the temporary movement bridge.
	Keyboard::reset();
	m_core.tick(tick);
	if (tick < m_lastPerformanceDisplayTick)
	{
		// UwpGameLoop resets its tick number between world sessions.
		m_lastPerformanceDisplayTick = 0;
		m_maxFrameMilliseconds = 0.0;
	}
	if (m_core.isActive())
	{
		m_ui.setGameTelemetry(m_core.worldName(), m_core.tickCount(),
			m_core.playerX(), m_core.playerZ(), m_core.dayTime());
	}
}

void MainPage::OnGameRender(float alpha)
{
	if (m_originalClientHost.isOriginalClientLoopEnabled() &&
		!m_core.isActive())
	{
		// Main-menu screens use the original Minecraft -> GameRenderer ->
		// Screen path.  Do not draw the custom UWP menu over it.
		m_originalClientHost.render(alpha);
		// UWP advances simulation at a fixed rate but renders at the display
		// rate. Consume the relative mouse accumulator after this render so a
		// single OS pointer delta is not applied again before the next tick's
		// normal NinecraftApp::update() reset2() call.
		Mouse::reset2();
		m_ui.setRealWorldRendering(false);
		return;
	}
	if (m_ui.screen() != UwpUiRenderer::Screen::Game)
		return;

	Level* level = m_core.nativeLevel();
	m_worldRenderer.setLevel(level);
	if (m_originalClientHost.isOriginalWorldTickEnabled())
	{
		// The fixed-step tick and variable-rate frame draw are separate on UWP.
		// Route every presented frame through the original Minecraft ->
		// GameRenderer -> LevelRenderer chain, without advancing simulation here.
		m_originalClientHost.render(alpha);
		Mouse::reset2();
		// Do not submit the UWP terrain pass on top of the original frame;
		// MainPage only keeps the UWP HUD overlay and final EGL presentation.
		m_ui.setRealWorldRendering(level != nullptr);
		return;
	}
	if (level != nullptr && m_worldRenderer.isInitialized())
	{
		m_worldRenderer.render(
			static_cast<float>(AnglePanel->ActualWidth),
			static_cast<float>(AnglePanel->ActualHeight),
			m_core.playerX(), m_core.playerY(), m_core.playerZ(), m_core.dayTime());
		m_ui.setRealWorldRendering(m_worldRenderer.hasWorld());
	}
	else
	{
		m_ui.setRealWorldRendering(false);
	}
}

void MainPage::OnRelativeMouseMoved(
	Windows::Devices::Input::MouseDevice^,
	Windows::Devices::Input::MouseEventArgs^ e)
{
	if (e == nullptr || Window::Current == nullptr ||
		Window::Current->CoreWindow == nullptr)
		return;

	const Minecraft* minecraft = m_originalClientHost.minecraft();
	if (m_originalMenuActive || minecraft == nullptr ||
		!m_originalClientHost.isOriginalWorldTickEnabled() ||
		!minecraft->mouseGrabbed || minecraft->screen != nullptr)
		return;

	const auto delta = e->MouseDelta;
	if (delta.X == 0 && delta.Y == 0)
		return;

	const Point position = Window::Current->CoreWindow->PointerPosition;
	Mouse::feed(MouseAction::ACTION_MOVE, MouseAction::DATA_UP,
		static_cast<short>(position.X), static_cast<short>(position.Y),
		static_cast<short>(delta.X), static_cast<short>(delta.Y));
}

void MainPage::OnPointerMoved(Object^, PointerRoutedEventArgs^ e)
{
	const auto pointer = e->GetCurrentPoint(AnglePanel);
	const auto point = pointer->Position;
	m_ui.setPointer(static_cast<float>(point.X), static_cast<float>(point.Y), false);
	Minecraft* minecraft = m_originalClientHost.minecraft();
	const bool worldMouseLocked =
		!m_originalMenuActive && minecraft != nullptr &&
		m_originalClientHost.isOriginalWorldTickEnabled() &&
		minecraft->mouseGrabbed && minecraft->screen == nullptr &&
		IsMousePointer(pointer);
	if (worldMouseLocked)
	{
		// World look comes from MouseDevice::MouseMoved below.  Keep this
		// absolute event out of the original delta queue; otherwise a hidden
		// cursor warp or edge event can be counted twice as camera motion.
		AnglePanel->CapturePointer(e->Pointer);
		return;
	}

	// The original touch holder consumes Multitouch's per-pointer coordinates
	// and edge events.  Keep world-view touch pointers on that path; the
	// Multitouch UWP compatibility mirror supplies legacy GUI click events.
	if (!m_originalMenuActive && minecraft != nullptr &&
		m_originalClientHost.isOriginalWorldTickEnabled() &&
		!IsMousePointer(pointer) &&
		minecraft->screen == nullptr && minecraft->useTouchscreen())
	{
		const int slot = TouchSlotForPointer(pointer->PointerId);
		if (slot >= 0)
			Multitouch::feed(MouseAction::ACTION_MOVE, MouseAction::DATA_UP,
				static_cast<short>(point.X), static_cast<short>(point.Y),
				static_cast<char>(slot));
		return;
	}

	// Menus and pause screens use normal screen-space pointer input so the
	// original Screen::updateEvents path receives touch as a left click.
	if (m_originalMenuActive ||
		(minecraft != nullptr && minecraft->screen != nullptr) ||
		(m_originalClientHost.isOriginalWorldTickEnabled() &&
			IsMousePointer(pointer)))
	{
		const float x = static_cast<float>(point.X);
		const float y = static_cast<float>(point.Y);
		const short dx = m_originalPointerX >= 0.0f
			? static_cast<short>(x - m_originalPointerX) : 0;
		const short dy = m_originalPointerY >= 0.0f
			? static_cast<short>(y - m_originalPointerY) : 0;
		Mouse::feed(MouseAction::ACTION_MOVE, MouseAction::DATA_UP,
			static_cast<short>(x), static_cast<short>(y), dx, dy);
		m_originalPointerX = x;
		m_originalPointerY = y;
	}
}

void MainPage::OnPointerPressed(Object^, PointerRoutedEventArgs^ e)
{
	Focus(Windows::UI::Xaml::FocusState::Pointer);
	const auto pointer = e->GetCurrentPoint(AnglePanel);
	const auto point = pointer->Position;
	m_ui.setPointer(static_cast<float>(point.X), static_cast<float>(point.Y), true);
	Minecraft* minecraft = m_originalClientHost.minecraft();
	if (m_originalMenuActive)
	{
		m_originalPointerX = static_cast<float>(point.X);
		m_originalPointerY = static_cast<float>(point.Y);
		Mouse::feed(MouseAction::ACTION_LEFT, MouseAction::DATA_DOWN,
			static_cast<short>(point.X), static_cast<short>(point.Y));
		return;
	}

	if (m_originalClientHost.isOriginalWorldTickEnabled() &&
		!IsMousePointer(pointer) && minecraft != nullptr &&
		minecraft->screen == nullptr && minecraft->useTouchscreen())
	{
		const int slot = TouchSlotForPointer(pointer->PointerId);
		if (slot >= 0)
		{
			AnglePanel->CapturePointer(e->Pointer);
			Multitouch::feed(MouseAction::ACTION_LEFT, MouseAction::DATA_DOWN,
				static_cast<short>(point.X), static_cast<short>(point.Y),
				static_cast<char>(slot));
		}
		return;
	}
	if (!m_originalClientHost.isOriginalWorldTickEnabled() ||
		(!IsMousePointer(pointer) && (minecraft == nullptr || minecraft->screen == nullptr)))
		return;

	// Capture the pointer only while the original client is in its world view.
	// When PauseScreen is active, its normal screen-coordinate mouse handling
	// must remain available and the cursor is visible again.
	if (minecraft != nullptr && minecraft->mouseGrabbed &&
		minecraft->screen == nullptr)
	{
		AnglePanel->CapturePointer(e->Pointer);
	}
	const auto properties = pointer->Properties;
	if (!IsMousePointer(pointer))
		Mouse::feed(MouseAction::ACTION_LEFT, MouseAction::DATA_DOWN,
			static_cast<short>(point.X), static_cast<short>(point.Y));
	else if (properties->IsLeftButtonPressed)
		Mouse::feed(MouseAction::ACTION_LEFT, MouseAction::DATA_DOWN,
			static_cast<short>(point.X), static_cast<short>(point.Y));
	if (IsMousePointer(pointer) && properties->IsRightButtonPressed)
		Mouse::feed(MouseAction::ACTION_RIGHT, MouseAction::DATA_DOWN,
			static_cast<short>(point.X), static_cast<short>(point.Y));
}

void MainPage::OnKeyDown(Windows::UI::Core::CoreWindow^,
	Windows::UI::Core::KeyEventArgs^ e)
{
	using Windows::System::VirtualKey;
	if (m_keyMappingPageVisible)
	{
		const int keyCode = ToOriginalKeyboardKey(e->VirtualKey);
		if (m_keyMappingPage != nullptr && m_keyMappingPage->IsCapturing())
			m_keyMappingPage->HandleKeyDown(keyCode);
		else if (e->VirtualKey == VirtualKey::Escape)
			HideKeyMappingPage();
		e->Handled = true;
		return;
	}
	if (m_worldEditPageVisible)
	{
		if (e->VirtualKey == VirtualKey::Escape)
			HideWorldEditPage();
		e->Handled = true;
		return;
	}
	if (m_worldImportPageVisible)
	{
		if (m_worldImportPage != nullptr)
		{
			if (e->VirtualKey == VirtualKey::Enter)
				m_worldImportPage->HandleEnterKey();
			else if (e->VirtualKey == VirtualKey::Escape)
				m_worldImportPage->HandleEscapeKey();
		}
		e->Handled = true;
		return;
	}
	if (m_skinPageVisible)
	{
		if (e->VirtualKey == VirtualKey::Escape && !m_skinOperationPending)
			HideSkinPage();
		e->Handled = true;
		return;
	}
	if (!FeedOriginalKeyboard(e->VirtualKey, true))
		return;
	e->Handled = true;
	// The existing game bridge maps arrows to W/A/S/D for movement.  The
	// original menu also expects the Win32 up/down key codes (38/40) for its
	// tab navigation, so queue those codes in menu mode as well.
	if (m_originalMenuActive &&
		(e->VirtualKey == VirtualKey::Up || e->VirtualKey == VirtualKey::Down))
		Keyboard::feed(e->VirtualKey == VirtualKey::Up ? 38 : 40,
			KeyboardAction::KEYDOWN);

	switch (e->VirtualKey)
	{
	case VirtualKey::F6:
		m_worldRenderer.setRenderBackend(UwpRenderBackend::OriginalChunk);
		m_ui.setStatusText(L"Renderer backend: OriginalChunk");
		return;
	case VirtualKey::F7:
		m_worldRenderer.setRenderBackend(UwpRenderBackend::UwpFallback);
		m_ui.setStatusText(L"Renderer backend: UwpFallback");
		return;
	case VirtualKey::W:
	case VirtualKey::Up:
		m_keyW = true;
		break;
	case VirtualKey::A:
	case VirtualKey::Left:
		m_keyA = true;
		break;
	case VirtualKey::S:
	case VirtualKey::Down:
		m_keyS = true;
		break;
	case VirtualKey::D:
	case VirtualKey::Right:
		m_keyD = true;
		break;
	case VirtualKey::Space:
		m_keySpace = true;
		break;
	default:
		break;
	}
	UpdateMoveInput();
}

void MainPage::OnKeyUp(Windows::UI::Core::CoreWindow^,
	Windows::UI::Core::KeyEventArgs^ e)
{
	using Windows::System::VirtualKey;
	if (m_keyMappingPageVisible)
	{
		e->Handled = true;
		return;
	}
	if (m_worldEditPageVisible)
	{
		e->Handled = true;
		return;
	}
	if (m_worldImportPageVisible)
	{
		e->Handled = true;
		return;
	}
	if (m_skinPageVisible)
	{
		e->Handled = true;
		return;
	}
	if (!FeedOriginalKeyboard(e->VirtualKey, false))
		return;
	e->Handled = true;
	if (m_originalMenuActive &&
		(e->VirtualKey == VirtualKey::Up || e->VirtualKey == VirtualKey::Down))
		Keyboard::feed(e->VirtualKey == VirtualKey::Up ? 38 : 40,
			KeyboardAction::KEYUP);

	switch (e->VirtualKey)
	{
	case VirtualKey::W:
	case VirtualKey::Up:
		m_keyW = false;
		break;
	case VirtualKey::A:
	case VirtualKey::Left:
		m_keyA = false;
		break;
	case VirtualKey::S:
	case VirtualKey::Down:
		m_keyS = false;
		break;
	case VirtualKey::D:
	case VirtualKey::Right:
		m_keyD = false;
		break;
	case VirtualKey::Space:
		m_keySpace = false;
		break;
	default:
		break;
	}
	UpdateMoveInput();
}

void MainPage::OnSystemBackRequested(
	Platform::Object^,
	Windows::UI::Core::BackRequestedEventArgs^ e)
{
	if (e == nullptr)
		return;

	// Keep the system Back button identical to the existing Escape routing.
	// XAML overlays own the input while visible, so the native client must not
	// receive a second Back/Escape event underneath them.
	if (m_keyMappingPageVisible)
	{
		if (m_keyMappingPage != nullptr && m_keyMappingPage->IsCapturing())
			m_keyMappingPage->HandleKeyDown(Keyboard::KEY_ESCAPE);
		else
			HideKeyMappingPage();
		e->Handled = true;
		return;
	}
	if (m_worldEditPageVisible)
	{
		HideWorldEditPage();
		e->Handled = true;
		return;
	}
	if (m_worldImportPageVisible)
	{
		if (m_worldImportPage != nullptr)
			m_worldImportPage->HandleEscapeKey();
		e->Handled = true;
		return;
	}
	if (m_skinPageVisible)
	{
		if (!m_skinOperationPending)
			HideSkinPage();
		e->Handled = true;
		return;
	}

	// Feed exactly the same native Escape event used by the keyboard path.
	// This preserves Minecraft::tickInput()'s existing key.pause mapping,
	// including opening and closing PauseScreen with the normal pause behavior.
	if (m_originalClientHost.isOriginalClientLoopEnabled() ||
		m_originalClientHost.isOriginalWorldTickEnabled())
	{
		Keyboard::feed(static_cast<unsigned char>(Keyboard::KEY_ESCAPE),
			KeyboardAction::KEYDOWN);
		e->Handled = true;
	}
}

void MainPage::OnCharacterReceived(Windows::UI::Core::CoreWindow^,
	Windows::UI::Core::CharacterReceivedEventArgs^ e)
{
	if (e == nullptr)
		return;
	if (m_keyMappingPageVisible)
	{
		e->Handled = true;
		return;
	}
	if (m_worldEditPageVisible)
	{
		e->Handled = true;
		return;
	}

	// The original Screen/TextBox path consumes Keyboard's text queue from
	// Minecraft::tickInput() -> Screen::updateEvents().  KeyDown alone only
	// carries a virtual key and therefore cannot populate names or seeds.
	// CharacterReceived is the UWP equivalent of Win32 WM_CHAR; keep the
	// original byte-oriented TextBox contract and ignore control/non-ASCII
	// code points that the 0.6.1 client cannot represent in std::string.
	const unsigned int code = e->KeyCode;
	if (code >= 32u && code <= 0x7fu)
		Keyboard::feedText(static_cast<char>(code));
}

void MainPage::UpdateMoveInput()
{
	if (m_ui.screen() != UwpUiRenderer::Screen::Game)
	{
		m_core.setMoveInput(0.0f, 0.0f);
		m_core.setJumpInput(false);
		return;
	}
	if (m_originalClientHost.isOriginalWorldTickEnabled())
	{
		// Keyboard::feed remains the sole input source.  Minecraft::tickInput()
		// consumes that queue; do not also feed the fallback movement adapter.
		return;
	}
	// This remains a temporary vertical-slice bridge.  Its source is now the
	// original Keyboard state table, rather than a second UWP input state.
	Minecraft* minecraft = m_originalClientHost.minecraft();
	const Options* options = minecraft != nullptr ? &minecraft->options : nullptr;
	const int rightKey = options != nullptr ? options->keyRight.key : Keyboard::KEY_D;
	const int leftKey = options != nullptr ? options->keyLeft.key : Keyboard::KEY_A;
	const int backKey = options != nullptr ? options->keyDown.key : Keyboard::KEY_S;
	const int forwardKey = options != nullptr ? options->keyUp.key : Keyboard::KEY_W;
	const int jumpKey = options != nullptr ? options->keyJump.key : Keyboard::KEY_SPACE;
	const float x = (Keyboard::isKeyDown(rightKey) ? 1.0f : 0.0f) -
		(Keyboard::isKeyDown(leftKey) ? 1.0f : 0.0f);
	const float z = (Keyboard::isKeyDown(backKey) ? 1.0f : 0.0f) -
		(Keyboard::isKeyDown(forwardKey) ? 1.0f : 0.0f);
	m_core.setMoveInput(x, z);
	m_core.setJumpInput(Keyboard::isKeyDown(jumpKey));
}

void MainPage::AttachKeyboard()
{
	if (Window::Current == nullptr ||
		Window::Current->CoreWindow == nullptr)
		return;
	auto window = Window::Current->CoreWindow;
	if (m_keyDownToken.Value == 0)
	{
		m_keyDownToken = window->KeyDown += ref new TypedEventHandler<
			Windows::UI::Core::CoreWindow^, Windows::UI::Core::KeyEventArgs^>(
			this, &MainPage::OnKeyDown);
		m_keyUpToken = window->KeyUp += ref new TypedEventHandler<
			Windows::UI::Core::CoreWindow^, Windows::UI::Core::KeyEventArgs^>(
			this, &MainPage::OnKeyUp);
		m_characterReceivedToken = window->CharacterReceived += ref new TypedEventHandler<
			Windows::UI::Core::CoreWindow^,
			Windows::UI::Core::CharacterReceivedEventArgs^>(
			this, &MainPage::OnCharacterReceived);
	}
	if (m_systemBackToken.Value == 0)
	{
		m_systemNavigationManager =
			Windows::UI::Core::SystemNavigationManager::GetForCurrentView();
		if (m_systemNavigationManager != nullptr)
			m_systemBackToken = m_systemNavigationManager->BackRequested +=
				ref new EventHandler<
					Windows::UI::Core::BackRequestedEventArgs^>(
						this, &MainPage::OnSystemBackRequested);
	}
	if (m_mouseMovedToken.Value == 0)
	{
		auto mouse = Windows::Devices::Input::MouseDevice::GetForCurrentView();
		if (mouse != nullptr)
			m_mouseMovedToken = mouse->MouseMoved += ref new TypedEventHandler<
				Windows::Devices::Input::MouseDevice^,
				Windows::Devices::Input::MouseEventArgs^>(
				this, &MainPage::OnRelativeMouseMoved);
	}
}

void MainPage::DetachKeyboard()
{
	if (Window::Current != nullptr && Window::Current->CoreWindow != nullptr)
	{
		auto window = Window::Current->CoreWindow;
		if (m_keyDownToken.Value != 0)
			window->KeyDown -= m_keyDownToken;
		if (m_keyUpToken.Value != 0)
			window->KeyUp -= m_keyUpToken;
		if (m_characterReceivedToken.Value != 0)
			window->CharacterReceived -= m_characterReceivedToken;
	}
	if (m_systemNavigationManager != nullptr && m_systemBackToken.Value != 0)
		m_systemNavigationManager->BackRequested -= m_systemBackToken;
	if (m_mouseMovedToken.Value != 0)
	{
		auto mouse = Windows::Devices::Input::MouseDevice::GetForCurrentView();
		if (mouse != nullptr)
			mouse->MouseMoved -= m_mouseMovedToken;
	}
	m_keyDownToken.Value = 0;
	m_keyUpToken.Value = 0;
	m_systemBackToken.Value = 0;
	m_characterReceivedToken.Value = 0;
	m_mouseMovedToken.Value = 0;
	m_systemNavigationManager = nullptr;
	m_keyW = m_keyA = m_keyS = m_keyD = m_keySpace = false;
}

void MainPage::OnPointerReleased(Object^, PointerRoutedEventArgs^ e)
{
	const auto pointer = e->GetCurrentPoint(AnglePanel);
	const auto point = pointer->Position;
	m_ui.setPointer(static_cast<float>(point.X), static_cast<float>(point.Y), false);
	Minecraft* minecraft = m_originalClientHost.minecraft();
	if (m_originalMenuActive)
	{
		Mouse::feed(MouseAction::ACTION_LEFT, MouseAction::DATA_UP,
			static_cast<short>(point.X), static_cast<short>(point.Y));
		if (!IsMousePointer(pointer))
			ReleaseTouchSlot(pointer->PointerId);
		return;
	}

	if (m_originalClientHost.isOriginalWorldTickEnabled() &&
		!IsMousePointer(pointer) && minecraft != nullptr &&
		minecraft->screen == nullptr && minecraft->useTouchscreen())
	{
		// Do not allocate a new slot while processing a release.  The native
		// client may already have reset Multitouch while changing screens.
		const int slot = ExistingTouchSlotForPointer(pointer->PointerId);
		if (slot >= 0)
		{
			Multitouch::feed(MouseAction::ACTION_LEFT, MouseAction::DATA_UP,
				static_cast<short>(point.X), static_cast<short>(point.Y),
				static_cast<char>(slot));
		}
		ReleaseTouchSlot(pointer->PointerId);
		// Clear our mapping before releasing capture.  UWP may raise
		// PointerCaptureLost synchronously from ReleasePointerCapture; the
		// cancel path must then see an already-closed slot and stay idempotent.
		AnglePanel->ReleasePointerCapture(e->Pointer);
		return;
	}
	if (m_originalClientHost.isOriginalWorldTickEnabled() &&
		((minecraft != nullptr && minecraft->screen != nullptr) ||
			IsMousePointer(pointer)))
	{
		const auto updateKind = pointer->Properties->PointerUpdateKind;
		if (!IsMousePointer(pointer))
			Mouse::feed(MouseAction::ACTION_LEFT, MouseAction::DATA_UP,
				static_cast<short>(point.X), static_cast<short>(point.Y));
		else if (updateKind == Windows::UI::Input::PointerUpdateKind::LeftButtonReleased)
			Mouse::feed(MouseAction::ACTION_LEFT, MouseAction::DATA_UP,
				static_cast<short>(point.X), static_cast<short>(point.Y));
		else if (updateKind == Windows::UI::Input::PointerUpdateKind::RightButtonReleased)
			Mouse::feed(MouseAction::ACTION_RIGHT, MouseAction::DATA_UP,
				static_cast<short>(point.X), static_cast<short>(point.Y));
		if (!IsMousePointer(pointer))
			// A world touch can open a Screen before its finger is released.
			// Reclaim the MainPage mapping here as the release now belongs to
			// the screen path rather than the world-touch path above.
			ReleaseTouchSlot(pointer->PointerId);
		AnglePanel->ReleasePointerCapture(e->Pointer);
	}
}

void MainPage::ReleasePointerInput(PointerRoutedEventArgs^ e)
{
	if (e == nullptr)
		return;

	const auto pointer = e->GetCurrentPoint(AnglePanel);
	if (pointer == nullptr)
		return;
	const auto point = pointer->Position;
	// UWP documents that PointerCanceled may carry an invalid position.  The
	// position is only needed to construct the synthetic release event, so use
	// the last known Mouse position in that case instead of casting NaN.
	const bool validPosition = point.X == point.X && point.Y == point.Y;
	const short inputX = validPosition ? static_cast<short>(point.X) : Mouse::getX();
	const short inputY = validPosition ? static_cast<short>(point.Y) : Mouse::getY();
	Minecraft* minecraft = m_originalClientHost.minecraft();

	// PointerCanceled and PointerCaptureLost are the UWP equivalents of an
	// input cancel.  They do not guarantee a later PointerReleased, so close
	// the native touch slot and the legacy left-button mirror here.
	if (!IsMousePointer(pointer))
	{
		const int slot = ExistingTouchSlotForPointer(pointer->PointerId);
		if (slot >= 0)
		{
			if (m_originalClientHost.isOriginalWorldTickEnabled() &&
				minecraft != nullptr && minecraft->screen == nullptr &&
				minecraft->useTouchscreen())
			{
				Multitouch::feed(MouseAction::ACTION_LEFT, MouseAction::DATA_UP,
					inputX, inputY,
					static_cast<char>(slot));
			}
			ReleaseTouchSlot(pointer->PointerId);
		}

		// A touch that was canceled while a Screen was active was mirrored
		// through Mouse directly rather than assigned a Multitouch slot.
		if (m_originalMenuActive ||
			(m_originalClientHost.isOriginalWorldTickEnabled() &&
				minecraft != nullptr && minecraft->screen != nullptr))
		{
			Mouse::feed(MouseAction::ACTION_LEFT, MouseAction::DATA_UP,
				inputX, inputY);
		}
		return;
	}

	// A captured mouse can also disappear without a matching button-up when
	// focus changes.  Release both buttons defensively; an extra up event is
	// harmless, while a stuck down state blocks later GUI/world input.
	if (m_originalMenuActive ||
		(m_originalClientHost.isOriginalWorldTickEnabled() && minecraft != nullptr))
	{
		Mouse::feed(MouseAction::ACTION_LEFT, MouseAction::DATA_UP,
			inputX, inputY);
		Mouse::feed(MouseAction::ACTION_RIGHT, MouseAction::DATA_UP,
			inputX, inputY);
	}
}

void MainPage::OnPointerCanceled(Object^, PointerRoutedEventArgs^ e)
{
	ReleasePointerInput(e);
}

void MainPage::OnPointerCaptureLost(Object^, PointerRoutedEventArgs^ e)
{
	ReleasePointerInput(e);
}

void MainPage::OnPointerWheelChanged(Object^, PointerRoutedEventArgs^ e)
{
	if (e == nullptr)
		return;

	const auto pointer = e->GetCurrentPoint(AnglePanel);
	if (!IsMousePointer(pointer) || pointer->Properties == nullptr)
		return;

	const int rawDelta = pointer->Properties->MouseWheelDelta;
	if (rawDelta == 0)
		return;

	// Match Win32's one signed unit per 120-point wheel notch.  The original
	// Screen path dispatches this to world lists/options panes; in-world
	// Minecraft::tickInput() consumes the same event for hotbar selection.
	int delta = rawDelta / 120;
	if (delta == 0)
		delta = rawDelta > 0 ? 1 : -1;
	const auto position = pointer->Position;
	Mouse::feed(MouseAction::ACTION_WHEEL, MouseAction::DATA_DOWN,
		static_cast<short>(position.X), static_cast<short>(position.Y),
		0, static_cast<short>(delta));
	e->Handled = true;
}

bool MainPage::AttachOriginalWorldForTick()
{
	if (!m_core.isActive() || !m_core.nativeWorldGenerated())
		return false;
	if (!m_originalClientHost.attachWorld(
		m_core.nativeLevel(), m_core.nativeLocalPlayer()))
		return false;
	LevelRenderer* originalLevelRenderer = m_originalClientHost.levelRenderer();
	if (originalLevelRenderer == nullptr)
	{
		m_originalClientHost.detachWorld();
		return false;
	}
	// The UWP frame callback remains the presentation owner, while the
	// original LevelRenderer now owns native Chunk scheduling/submission.
	m_worldRenderer.setOriginalLevelRenderer(
		originalLevelRenderer, m_core.nativeLocalPlayer());
	// Keep a local UWP world offline until the user explicitly chooses
	// "Open to LAN" from PauseScreen. Automatically hosting here makes
	// LocalPlayer::tick() send movement packets while the player is still the
	// sole local authority; a delayed server MovePlayerPacket can then feed the
	// normal three-tick interpolation path back into the same player and feel
	// like a pull in the opposite direction.
	m_originalPointerX = -1.0f;
	m_originalPointerY = -1.0f;
	m_originalClientHost.setWorldTickEnabled(true);
	return true;
}

void MainPage::LoadMenuTexture(Platform::String^ uriString, UwpUiRenderer::TextureSlot slot)
{
	create_task(StorageFile::GetFileFromApplicationUriAsync(ref new Uri(uriString)))
		.then([](StorageFile^ file)
		{
			return file->OpenAsync(FileAccessMode::Read);
		})
		.then([](IRandomAccessStream^ stream)
		{
			return BitmapDecoder::CreateAsync(stream);
		})
		.then([this, slot](BitmapDecoder^ decoder)
		{
			const unsigned int width = decoder->PixelWidth;
			const unsigned int height = decoder->PixelHeight;
			auto transform = ref new BitmapTransform();
			return create_task(decoder->GetPixelDataAsync(
				BitmapPixelFormat::Rgba8,
				BitmapAlphaMode::Straight,
				transform,
				ExifOrientationMode::IgnoreExifOrientation,
				ColorManagementMode::DoNotColorManage))
				.then([this, slot, width, height](PixelDataProvider^ provider)
				{
					auto pixels = provider->DetachPixelData();
					m_ui.setTexturePixels(slot, width, height, pixels->Data, pixels->Length);
				}, task_continuation_context::use_current());
		})
		.then([this](task<void> result)
		{
			try
			{
				result.get();
			}
			catch (Exception^)
			{
				m_ui.setStatusText(L"Failed to load MCPE menu texture");
			}
		}, task_continuation_context::use_current());
}

void MainPage::LoadTerrainTexture()
{
	create_task(StorageFile::GetFileFromApplicationUriAsync(
		ref new Uri(L"ms-appx:///data/images/terrain.png")))
		.then([](StorageFile^ file)
		{
			return file->OpenAsync(FileAccessMode::Read);
		})
		.then([](IRandomAccessStream^ stream)
		{
			return BitmapDecoder::CreateAsync(stream);
		})
		.then([this](BitmapDecoder^ decoder)
		{
			const unsigned int width = decoder->PixelWidth;
			const unsigned int height = decoder->PixelHeight;
			auto transform = ref new BitmapTransform();
			return create_task(decoder->GetPixelDataAsync(
				BitmapPixelFormat::Rgba8,
				BitmapAlphaMode::Straight,
				transform,
				ExifOrientationMode::IgnoreExifOrientation,
				ColorManagementMode::DoNotColorManage))
				.then([this, width, height](PixelDataProvider^ provider)
				{
					auto pixels = provider->DetachPixelData();
					if (m_platform)
					{
						m_platform->cacheTexturePixels("terrain.png", true,
							width, height, pixels->Data, pixels->Length);
					}
					m_worldRenderer.setTerrainTexturePixels(
						width, height, pixels->Data, pixels->Length);
					RunPlatformSmokeTest();
					RecordOriginalClientTexturePreload("terrain.png", true);
				}, task_continuation_context::use_current());
		})
		.then([this](task<void> result)
		{
			try
			{
				result.get();
			}
			catch (...)
			{
				if (m_platform)
					m_platform->setTexturePreloadFailure("terrain.png", true, "UWP terrain preload failed");
				RunPlatformSmokeTest();
				RecordOriginalClientTexturePreload("terrain.png", false);
				m_ui.setStatusText(L"Failed to load MCPE terrain texture");
			}
		}, task_continuation_context::use_current());
}

void MainPage::BeginOriginalClientTexturePreload()
{
	// AppPlatform_uwp deliberately exposes a synchronous texture contract to
	// the original Textures class. Every image which the original client may
	// bind during GameRenderer/Gui/ItemInHand/LevelRenderer therefore has to be
	// decoded into that cache before Minecraft::init runs. Previously only the
	// five init-time images were cached; a later cache miss left the preceding
	// terrain texture bound, which made the hotbar, hand and clouds display the
	// terrain atlas instead of their own resources.
	static const char* const kOriginalClientTextures[] =
	{
		"misc/grasscolor.png",
		"misc/foliagecolor.png",
		"particles.png",
		"font/default8.png",
		"gui/gui.png",
        "gui/inventory_slot.png",
		"gui/gui_blocks.png",
		"gui/items.png",
		"gui/icons.png",
		"gui/cursor.png",
		"gui/touchgui.png",
		"gui/background.png",
		"gui/title.png",
		"gui/default_world.png",
		"gui/spritesheet.png",
		"gui/logo/raknet_high_72.png",
		"gui/itemframe.png",
		"gui/badge/minecon140.png",
		"item/arrows.png",
		"item/sign.png",
		"environment/clouds.png",
		"terrain/sun.png",
		"terrain/moon.png",
		"mob/char.png",
		// EntityRenderDispatcher resolves the original mob renderers lazily.
		// AppPlatform_uwp intentionally exposes a synchronous cache-only
		// texture API, so every mob skin that can be requested by those
		// renderers must be decoded before the client loop starts.  Without
		// these entries a failed load leaves the previous terrain texture bound
		// and the mob is drawn with a block-atlas appearance.
		"mob/chicken.png",
		"mob/cow.png",
		"mob/creeper.png",
		"mob/pig.png",
		"mob/pigzombie.png",
		"mob/sheep.png",
		"mob/sheep_fur.png",
		"mob/skeleton.png",
		"mob/spider.png",
		"mob/zombie.png"
	};
	const unsigned int textureCount = static_cast<unsigned int>(
		sizeof(kOriginalClientTextures) / sizeof(kOriginalClientTextures[0]));
	m_clientTexturePreloadCompleted = 0;
	m_clientTexturePreloadSucceeded = 0;
	// terrain.png is loaded by LoadTerrainTexture below, while the array above
	// is handled by this function.
	const bool hasStoredSkin = UwpStoragePaths::localSkinExistsBlocking();
	m_clientTexturePreloadExpected = textureCount + 1 + (hasStoredSkin ? 1 : 0);
	m_clientTexturePreloadFailures.clear();
	std::wostringstream pending;
	pending << L"Client texture cache: pending (0/"
		<< m_clientTexturePreloadExpected << L")";
	m_ui.setStatusDetailText(pending.str());

	for (unsigned int i = 0; i < textureCount; ++i)
		LoadOriginalClientStartupTexture(kOriginalClientTextures[i]);
	if (hasStoredSkin)
		LoadOriginalClientStartupTexture("skins/local.png");
}

void MainPage::LoadOriginalClientStartupTexture(const std::string& filename)
{
	if (filename == "skins/local.png" && UwpStoragePaths::localSkinExistsBlocking())
	{
		create_task(StorageFile::GetFileFromPathAsync(
			UwpStoragePaths::localSkinPath()))
			.then([](StorageFile^ file)
			{
				return file->OpenAsync(FileAccessMode::Read);
			})
			.then([](IRandomAccessStream^ stream)
			{
				return BitmapDecoder::CreateAsync(stream);
			})
			.then([this, filename](BitmapDecoder^ decoder)
			{
				const unsigned int width = decoder->PixelWidth;
				const unsigned int height = decoder->PixelHeight;
				auto transform = ref new BitmapTransform();
				return create_task(decoder->GetPixelDataAsync(
					BitmapPixelFormat::Rgba8,
					BitmapAlphaMode::Straight,
					transform,
					ExifOrientationMode::IgnoreExifOrientation,
					ColorManagementMode::DoNotColorManage))
					.then([this, filename, width, height](PixelDataProvider^ provider)
					{
						auto pixels = provider->DetachPixelData();
						if (m_platform)
							m_platform->cacheTexturePixels(filename, true,
								width, height, pixels->Data, pixels->Length);
						RecordOriginalClientTexturePreload(filename, m_platform != nullptr);
					}, task_continuation_context::use_current());
			})
			.then([this, filename](task<void> result)
			{
				try
				{
					result.get();
				}
				catch (...)
				{
					if (m_platform)
						m_platform->setTexturePreloadFailure(filename, true,
							"UWP local skin preload failed");
					RecordOriginalClientTexturePreload(filename, false);
				}
			}, task_continuation_context::use_current());
		return;
	}

	std::wstring uri = L"ms-appx:///data/images/";
	for (const unsigned char ch : filename)
		uri.push_back(static_cast<wchar_t>(ch));

	create_task(StorageFile::GetFileFromApplicationUriAsync(
		ref new Uri(ref new String(uri.c_str()))))
		.then([](StorageFile^ file)
		{
			return file->OpenAsync(FileAccessMode::Read);
		})
		.then([](IRandomAccessStream^ stream)
		{
			return BitmapDecoder::CreateAsync(stream);
		})
		.then([this, filename](BitmapDecoder^ decoder)
		{
			const unsigned int width = decoder->PixelWidth;
			const unsigned int height = decoder->PixelHeight;
			auto transform = ref new BitmapTransform();
			return create_task(decoder->GetPixelDataAsync(
				BitmapPixelFormat::Rgba8,
				BitmapAlphaMode::Straight,
				transform,
				ExifOrientationMode::IgnoreExifOrientation,
				ColorManagementMode::DoNotColorManage))
				.then([this, filename, width, height](PixelDataProvider^ provider)
				{
					auto pixels = provider->DetachPixelData();
					if (m_platform)
					{
						m_platform->cacheTexturePixels(filename, true,
							width, height, pixels->Data, pixels->Length);
					}
					RecordOriginalClientTexturePreload(filename, m_platform != nullptr);
				}, task_continuation_context::use_current());
		})
		.then([this, filename](task<void> result)
		{
			try
			{
				result.get();
			}
			catch (...)
			{
				if (m_platform)
					m_platform->setTexturePreloadFailure(filename, true, "UWP startup texture preload failed");
				RecordOriginalClientTexturePreload(filename, false);
			}
		}, task_continuation_context::use_current());
}

void MainPage::RecordOriginalClientTexturePreload(const std::string& filename, bool succeeded)
{
	++m_clientTexturePreloadCompleted;
	if (succeeded)
	{
		++m_clientTexturePreloadSucceeded;
	}
	else
	{
		std::wstring failedName;
		for (const unsigned char ch : filename)
			failedName.push_back(static_cast<wchar_t>(ch));
		m_clientTexturePreloadFailures.push_back(failedName);
	}

	if (m_clientTexturePreloadExpected == 0 ||
		m_clientTexturePreloadCompleted < m_clientTexturePreloadExpected)
		return;

	std::wostringstream status;
	status << L"Client texture cache: " << m_clientTexturePreloadSucceeded
		<< L"/" << m_clientTexturePreloadExpected << L" ready";
	if (!m_clientTexturePreloadFailures.empty())
		status << L"; failed " << m_clientTexturePreloadFailures.front();
	m_ui.setStatusDetailText(status.str());
	SetStartupDiagnostic(L"Original startup texture preload", status.str());
}

void MainPage::LoadMenuFont()
{
	create_task(StorageFile::GetFileFromApplicationUriAsync(
		ref new Uri(L"ms-appx:///data/fonts/minecraft.ttf")))
		.then([this](StorageFile^ file)
		{
			const bool loaded = m_ui.setFontFilePath(file->Path->Data());
			if (!loaded)
				m_ui.setStatusText(L"MCPE menu font initialization failed");
		}, task_continuation_context::use_current())
		.then([this](task<void> result)
		{
			try
			{
				result.get();
			}
			catch (Exception^)
			{
				m_ui.setStatusText(L"MCPE menu font file unavailable");
			}
		}, task_continuation_context::use_current());
}

void MainPage::ShutdownAngle()
{
	DetachKeyboard();
	m_core.setMoveInput(0.0f, 0.0f);
	m_originalClientHost.setWorldTickEnabled(false);
	m_originalClientHost.detachWorld();
	m_worldRenderer.setLevel(nullptr);
	m_core.stop();
	if (m_renderToken.Value != 0)
	{
		CompositionTarget::Rendering -= m_renderToken;
		m_renderToken.Value = 0;
	}
	m_ui.setOriginalFont(nullptr);
	m_worldRenderer.shutdown();
	// Detach the original client before its AppContext's EGL
	// handles are destroyed.  It never owns presentation, but it must not retain
	// stale display/context/surface values across a page shutdown.
	m_originalClientHost.clear();
	m_angle.shutdown();
	m_ui.shutdown();
}
