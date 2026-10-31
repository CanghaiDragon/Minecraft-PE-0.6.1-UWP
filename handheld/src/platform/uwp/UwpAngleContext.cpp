#include "pch.h"
#include "UwpAngleContext.h"

#define GL_GLEXT_PROTOTYPES 1
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <angle_windowsstore.h>
#include <d3d11.h>
#include <dxgi.h>
#include <sstream>
#include <vector>

// The legacy MCPE EGL headers bundled in handheld/lib/include predate the
// ANGLE Windows Store extension.  Keep the extension value local to this
// bridge so the UWP project can include the original core headers without
// changing their include order.
#ifndef EGL_ANGLE_SURFACE_RENDER_TO_BACK_BUFFER
#define EGL_ANGLE_SURFACE_RENDER_TO_BACK_BUFFER 0x320C
#endif
#ifndef EGL_PLATFORM_ANGLE_ANGLE
#define EGL_PLATFORM_ANGLE_ANGLE 0x3202
#endif
#ifndef EGL_PLATFORM_ANGLE_TYPE_ANGLE
#define EGL_PLATFORM_ANGLE_TYPE_ANGLE 0x3203
#endif
#ifndef EGL_PLATFORM_ANGLE_TYPE_D3D9_ANGLE
#define EGL_PLATFORM_ANGLE_TYPE_D3D9_ANGLE 0x3207
#endif
#ifndef EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE
#define EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE 0x3208
#endif
#ifndef EGL_ANGLE_DISPLAY_ALLOW_RENDER_TO_BACK_BUFFER
#define EGL_ANGLE_DISPLAY_ALLOW_RENDER_TO_BACK_BUFFER 0x320B
#endif
#ifndef EGL_PLATFORM_ANGLE_ENABLE_AUTOMATIC_TRIM_ANGLE
#define EGL_PLATFORM_ANGLE_ENABLE_AUTOMATIC_TRIM_ANGLE 0x320F
#endif
#ifndef EGL_D3D11_ONLY_DISPLAY_ANGLE
#define EGL_D3D11_ONLY_DISPLAY_ANGLE ((EGLNativeDisplayType)-3)
#endif

using namespace Platform;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::UI::Xaml::Controls;

namespace
{
	std::wstring narrowToWide(const char* value);

	#if defined(_M_ARM)
	std::wstring gEglDebugMessages;

	void EGLAPIENTRY angleEglDebugCallback(
		EGLenum error, const char* command, EGLint messageType,
		void*, void*, const char* message)
	{
		std::wostringstream text;
		text << L"EGL_KHR_debug type=0x" << std::hex << std::uppercase
			<< static_cast<unsigned int>(messageType) << L" error=0x"
			<< static_cast<unsigned int>(error) << std::dec
			<< L" command=" << narrowToWide(command)
			<< L" message=" << narrowToWide(message) << L"; ";
		gEglDebugMessages += text.str();
	}
	#endif

	std::wstring narrowToWide(const char* value)
	{
		if (value == nullptr)
			return L"<null>";
		std::wstring result;
		while (*value != '\0')
			result.push_back(static_cast<unsigned char>(*value++));
		return result;
	}

	std::wstring displayDetailsForFailure(const std::wstring& details,
		const wchar_t* operation, EGLint error)
	{
		std::wostringstream result;
		result << details << L" | " << operation << L"=0x" << std::hex
			<< std::uppercase << static_cast<unsigned int>(error) << std::dec;
		return result.str();
	}

#if defined(_M_ARM)
	std::wstring hresultToWide(HRESULT result)
	{
		std::wostringstream text;
		text << L"0x" << std::hex << std::uppercase
			<< static_cast<unsigned long>(result) << std::dec;
		return text.str();
	}

	std::wstring runDxgiAdapterProbe()
	{
		std::wostringstream result;
		IDXGIFactory1* factory = nullptr;
		const HRESULT factoryResult = CreateDXGIFactory1(
			__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&factory));
		result << L"DXGI CreateDXGIFactory1 HRESULT="
			<< hresultToWide(factoryResult);
		if (FAILED(factoryResult) || factory == nullptr)
			return result.str();

		IDXGIAdapter1* adapter = nullptr;
		const HRESULT adapterResult = factory->EnumAdapters1(0, &adapter);
		result << L" | EnumAdapters1(0) HRESULT="
			<< hresultToWide(adapterResult);
		if (FAILED(adapterResult) || adapter == nullptr)
		{
			factory->Release();
			return result.str();
		}

		DXGI_ADAPTER_DESC1 description = {};
		const HRESULT descriptionResult = adapter->GetDesc1(&description);
		result << L" | GetDesc1 HRESULT=" << hresultToWide(descriptionResult);
		if (SUCCEEDED(descriptionResult))
		{
			result << L" | Description=" << description.Description
				<< L" VendorId=0x" << std::hex << std::uppercase << description.VendorId
				<< L" DeviceId=0x" << description.DeviceId
				<< L" Flags=0x" << description.Flags << std::dec;
		}

		const D3D_FEATURE_LEVEL levels[] =
		{
			D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1,
			D3D_FEATURE_LEVEL_10_0, D3D_FEATURE_LEVEL_9_3,
			D3D_FEATURE_LEVEL_9_2, D3D_FEATURE_LEVEL_9_1
		};
		ID3D11Device* device = nullptr;
		ID3D11DeviceContext* context = nullptr;
		D3D_FEATURE_LEVEL actualLevel = static_cast<D3D_FEATURE_LEVEL>(0);
		const HRESULT deviceResult = D3D11CreateDevice(
			adapter, D3D_DRIVER_TYPE_UNKNOWN, nullptr,
			D3D11_CREATE_DEVICE_BGRA_SUPPORT, levels, _countof(levels),
			D3D11_SDK_VERSION, &device, &actualLevel, &context);
		result << L" | Adapter+D3D_DRIVER_TYPE_UNKNOWN BGRA=on HRESULT="
			<< hresultToWide(deviceResult);
		if (SUCCEEDED(deviceResult))
		{
			result << L" ActualFeatureLevel="
				<< (actualLevel == D3D_FEATURE_LEVEL_11_0 ? L"11_0" :
					actualLevel == D3D_FEATURE_LEVEL_10_1 ? L"10_1" :
					actualLevel == D3D_FEATURE_LEVEL_10_0 ? L"10_0" :
					actualLevel == D3D_FEATURE_LEVEL_9_3 ? L"9_3" :
					actualLevel == D3D_FEATURE_LEVEL_9_2 ? L"9_2" :
					actualLevel == D3D_FEATURE_LEVEL_9_1 ? L"9_1" : L"unknown");
		}
		if (context != nullptr)
			context->Release();
		if (device != nullptr)
			device->Release();
		adapter->Release();
		factory->Release();
		return result.str();
	}
#endif

	bool getConfigAttribute(EGLDisplay display, EGLConfig config, EGLint attribute, EGLint& value)
	{
		value = 0;
		return eglGetConfigAttrib(display, config, attribute, &value) == EGL_TRUE;
	}

#if defined(_M_ARM)
	std::wstring runD3D11DeviceProbe()
	{
		const D3D_FEATURE_LEVEL requestedWith11_1[] =
		{
			D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0,
			D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0,
			D3D_FEATURE_LEVEL_9_3, D3D_FEATURE_LEVEL_9_2,
			D3D_FEATURE_LEVEL_9_1
		};
		const D3D_FEATURE_LEVEL requestedWithout11_1[] =
		{
			D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1,
			D3D_FEATURE_LEVEL_10_0, D3D_FEATURE_LEVEL_9_3,
			D3D_FEATURE_LEVEL_9_2, D3D_FEATURE_LEVEL_9_1
		};

		auto featureLevelName = [](D3D_FEATURE_LEVEL level) -> const wchar_t*
		{
			switch (level)
			{
			case D3D_FEATURE_LEVEL_11_1: return L"11_1";
			case D3D_FEATURE_LEVEL_11_0: return L"11_0";
			case D3D_FEATURE_LEVEL_10_1: return L"10_1";
			case D3D_FEATURE_LEVEL_10_0: return L"10_0";
			case D3D_FEATURE_LEVEL_9_3: return L"9_3";
			case D3D_FEATURE_LEVEL_9_2: return L"9_2";
			case D3D_FEATURE_LEVEL_9_1: return L"9_1";
			default: return L"unknown";
			}
		};

		auto hresultText = [](HRESULT result) -> std::wstring
		{
			std::wostringstream text;
			text << L"0x" << std::hex << std::uppercase
				<< static_cast<unsigned long>(result) << std::dec;
			return text.str();
		};

		auto tryDriver = [&](D3D_DRIVER_TYPE driver, const wchar_t* name,
			const D3D_FEATURE_LEVEL* levels, UINT levelCount,
			bool& invalid11_1) -> std::wstring
		{
			ID3D11Device* device = nullptr;
			ID3D11DeviceContext* context = nullptr;
			D3D_FEATURE_LEVEL actualLevel = static_cast<D3D_FEATURE_LEVEL>(0);
			const UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
			const HRESULT result = D3D11CreateDevice(
				nullptr, driver, nullptr, flags, levels, levelCount,
				D3D11_SDK_VERSION, &device, &actualLevel, &context);
			if (result == E_INVALIDARG && levelCount == _countof(requestedWith11_1))
				invalid11_1 = true;
			std::wostringstream text;
			text << name << L" BGRA=on requested=" << levelCount
				<< L" HRESULT=" << hresultText(result);
			if (SUCCEEDED(result))
				text << L" actual=" << featureLevelName(actualLevel);
			if (context != nullptr)
				context->Release();
			if (device != nullptr)
				device->Release();
			return text.str();
		};

		bool invalid11_1 = false;
		std::wstring hardware = tryDriver(
			D3D_DRIVER_TYPE_HARDWARE, L"D3D11 hardware",
			requestedWith11_1, _countof(requestedWith11_1), invalid11_1);
		std::wostringstream result;
		result << hardware;
		bool hardwareSucceeded = hardware.find(L" HRESULT=0x0") != std::wstring::npos;
		if (invalid11_1)
		{
			bool ignored = false;
			const std::wstring retry = tryDriver(
				D3D_DRIVER_TYPE_HARDWARE, L"D3D11 hardware",
				requestedWithout11_1, _countof(requestedWithout11_1), ignored);
			result << L" | retry-without-11_1: " << retry;
			hardwareSucceeded = retry.find(L" HRESULT=0x0") != std::wstring::npos;
		}

		// WARP is only a control test after hardware creation failed.
		if (!hardwareSucceeded)
		{
			bool ignored = false;
			result << L" | " << tryDriver(
				D3D_DRIVER_TYPE_WARP, L"D3D11 WARP",
				requestedWithout11_1, _countof(requestedWithout11_1), ignored);
		}
		return result.str();
	}
#endif
}

UwpAngleContext::~UwpAngleContext()
{
	shutdown();
}

bool UwpAngleContext::initialize(SwapChainPanel^ panel)
{
	m_lastDetails.clear();
	m_lastError = EGL_SUCCESS;
	if (m_initialized)
	{
		return true;
	}
	if (panel == nullptr)
	{
		m_lastStage = L"SwapChainPanel validation";
		m_lastError = EGL_BAD_NATIVE_WINDOW;
		return false;
	}

	// ANGLE.WindowsStore expects an IPropertySet as the native window.  The
	// property set points at the XAML SwapChainPanel used for presentation.
	auto surfaceProperties = ref new PropertySet();
	surfaceProperties->Insert(ref new String(EGLNativeWindowTypeProperty), panel);

	// The Windows Store ANGLE template can select the renderer through
	// eglGetPlatformDisplayEXT.  The legacy binary used by this port also
	// supports the simpler default-display path, which is preferred below.
	using GetPlatformDisplayProc = EGLDisplay(EGLAPIENTRYP)(
		EGLenum, void*, const EGLint*);
	auto getPlatformDisplay = reinterpret_cast<GetPlatformDisplayProc>(
		eglGetProcAddress("eglGetPlatformDisplayEXT"));
	const EGLint d3d11DisplayAttributes[] =
	{
		EGL_PLATFORM_ANGLE_TYPE_ANGLE, EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE,
		EGL_ANGLE_DISPLAY_ALLOW_RENDER_TO_BACK_BUFFER, EGL_TRUE,
		EGL_PLATFORM_ANGLE_ENABLE_AUTOMATIC_TRIM_ANGLE, EGL_TRUE,
		EGL_NONE
	};
	const EGLint d3d9DisplayAttributes[] =
	{
		EGL_PLATFORM_ANGLE_TYPE_ANGLE, EGL_PLATFORM_ANGLE_TYPE_D3D9_ANGLE,
		EGL_ANGLE_DISPLAY_ALLOW_RENDER_TO_BACK_BUFFER, EGL_TRUE,
		EGL_PLATFORM_ANGLE_ENABLE_AUTOMATIC_TRIM_ANGLE, EGL_TRUE,
		EGL_NONE
	};
	// Controlled compatibility probes.  Keep the original attribute lists above
	// as the comparison path; these probes test only the backend selector and
	// omit optional display attributes whose ownership is version-dependent.
	const EGLint d3d11MinimalAttributes[] =
	{
		EGL_PLATFORM_ANGLE_TYPE_ANGLE, EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE,
		EGL_NONE
	};
	const EGLint d3d9MinimalAttributes[] =
	{
		EGL_PLATFORM_ANGLE_TYPE_ANGLE, EGL_PLATFORM_ANGLE_TYPE_D3D9_ANGLE,
		EGL_NONE
	};

	EGLint major = 0;
	EGLint minor = 0;
	std::wostringstream backendAttempts;
#if defined(_M_ARM)
	gEglDebugMessages.clear();
	using EglDebugMessageControlProc = EGLint(EGLAPIENTRYP)(
		void (EGLAPIENTRY *)(EGLenum, const char*, EGLint, void*, void*, const char*),
		const void*);
	auto eglDebugMessageControl = reinterpret_cast<EglDebugMessageControlProc>(
		eglGetProcAddress("eglDebugMessageControlKHR"));
	if (eglDebugMessageControl != nullptr)
	{
		const EGLint debugResult = eglDebugMessageControl(angleEglDebugCallback, nullptr);
		backendAttempts << L"EGL_KHR_debug registration=0x" << std::hex
			<< std::uppercase << static_cast<unsigned int>(debugResult) << std::dec << L"; ";
	}
	else
	{
		backendAttempts << L"EGL_KHR_debug registration=unavailable; ";
	}
	backendAttempts << runDxgiAdapterProbe() << L"; ";
	backendAttempts << L"native D3D probe: " << runD3D11DeviceProbe() << L"; ";
#endif
	auto recordBackendFailure = [&backendAttempts](const wchar_t* backend,
		const wchar_t* operation, EGLint error)
	{
		backendAttempts << backend << L"." << operation << L"=0x"
			<< std::hex << std::uppercase << static_cast<unsigned int>(error)
			<< std::dec << L"; ";
	};
	auto tryPlatformBackend = [&](const wchar_t* label, const EGLint* attributes) -> bool
	{
		m_lastStage = std::wstring(L"eglGetPlatformDisplayEXT(") + label + L")";
		m_display = getPlatformDisplay(
			EGL_PLATFORM_ANGLE_ANGLE,
			reinterpret_cast<void*>(EGL_DEFAULT_DISPLAY), attributes);
		if (m_display == EGL_NO_DISPLAY)
		{
			m_lastError = eglGetError();
			recordBackendFailure(label, L"eglGetPlatformDisplayEXT", m_lastError);
			return false;
		}

		m_lastStage = std::wstring(L"eglInitialize(") + label + L")";
		if (!eglInitialize(m_display, &major, &minor))
		{
			m_lastError = eglGetError();
			recordBackendFailure(label, L"eglInitialize", m_lastError);
			eglTerminate(m_display);
			m_display = EGL_NO_DISPLAY;
			return false;
		}

		backendAttempts << label << L".eglGetPlatformDisplayEXT=OK; ";
		backendAttempts << label << L".eglInitialize=OK; ";
		return true;
	};
	// The legacy ANGLE binary shipped with this port is known to work with
	// the default display path (the same path used by UWPtest).  Prefer it so
	// that the surface/context ordering remains compatible with that binary;
	// use the explicit D3D backends only when the default display is unavailable.
	m_lastStage = L"eglGetDisplay(default)";
	m_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	if (m_display != EGL_NO_DISPLAY)
	{
		m_lastStage = L"eglInitialize(default)";
		if (!eglInitialize(m_display, &major, &minor))
		{
			m_lastError = eglGetError();
			recordBackendFailure(L"default", L"eglInitialize", m_lastError);
			eglTerminate(m_display);
			m_display = EGL_NO_DISPLAY;
		}
		else
		{
			backendAttempts << L"default.eglInitialize=OK; ";
		}
	}
	else
	{
		m_lastError = eglGetError();
		recordBackendFailure(L"default", L"eglGetDisplay", m_lastError);
	}

	if (m_display == EGL_NO_DISPLAY)
	{
		// ANGLE's older direct3d_display extension provides a second D3D11
		// entry point.  Keep it as an independent probe; do not replace the
		// platform-attribute paths below.
		m_lastStage = L"eglGetDisplay(D3D11_ONLY_DISPLAY_ANGLE)";
		m_display = eglGetDisplay(EGL_D3D11_ONLY_DISPLAY_ANGLE);
		if (m_display != EGL_NO_DISPLAY)
		{
			m_lastStage = L"eglInitialize(D3D11_ONLY_DISPLAY_ANGLE)";
			if (!eglInitialize(m_display, &major, &minor))
			{
				m_lastError = eglGetError();
				recordBackendFailure(L"D3D11_ONLY_DISPLAY_ANGLE", L"eglInitialize", m_lastError);
				eglTerminate(m_display);
				m_display = EGL_NO_DISPLAY;
			}
			else
			{
				backendAttempts << L"D3D11_ONLY_DISPLAY_ANGLE.eglGetDisplay=OK; ";
				backendAttempts << L"D3D11_ONLY_DISPLAY_ANGLE.eglInitialize=OK; ";
			}
		}
		else
		{
			m_lastError = eglGetError();
			recordBackendFailure(L"D3D11_ONLY_DISPLAY_ANGLE", L"eglGetDisplay", m_lastError);
		}
	}

	if (m_display == EGL_NO_DISPLAY && getPlatformDisplay != nullptr)
	{
		// Test the minimal D3D11 and D3D9 selector lists first.  If both fail,
		// retry the original lists so the previous implementation remains a
		// directly comparable control.
		if (!tryPlatformBackend(L"D3D11-minimal", d3d11MinimalAttributes) &&
			!tryPlatformBackend(L"D3D9-minimal", d3d9MinimalAttributes) &&
			!tryPlatformBackend(L"D3D11-original", d3d11DisplayAttributes))
		{
			tryPlatformBackend(L"D3D9-original", d3d9DisplayAttributes);
		}
	}
	if (m_display == EGL_NO_DISPLAY)
	{
		if (m_lastError == EGL_SUCCESS)
			m_lastError = eglGetError();
		m_lastDetails = L"Backend attempts: " + backendAttempts.str();
	#if defined(_M_ARM)
		m_lastDetails += L" ANGLE debug: " +
			(gEglDebugMessages.empty() ? L"<none>" : gEglDebugMessages);
	#endif
		return false;
	}

	// Capture immutable display facts before selecting a config.  These values
	// distinguish an ANGLE/driver problem from a bad config supplied to the
	// context call and remain visible in the XAML diagnostic overlay.
	EGLint availableConfigs = 0;
	eglGetConfigs(m_display, nullptr, 0, &availableConfigs);
	std::wostringstream displayInfo;
	const std::wstring extensions = narrowToWide(eglQueryString(m_display, EGL_EXTENSIONS));
	displayInfo << L"EGL " << major << L'.' << minor
		<< L" | EGLint=" << sizeof(EGLint)
		<< L" | vendor=" << narrowToWide(eglQueryString(m_display, EGL_VENDOR))
		<< L" | version=" << narrowToWide(eglQueryString(m_display, EGL_VERSION))
		<< L" | APIs=" << narrowToWide(eglQueryString(m_display, EGL_CLIENT_APIS))
		<< L" | EXT_platform_base="
		<< (extensions.find(L"EGL_EXT_platform_base") != std::wstring::npos ? L"yes" : L"no")
		<< L" | ANGLE_platform_angle="
		<< (extensions.find(L"EGL_ANGLE_platform_angle") != std::wstring::npos ? L"yes" : L"no")
		<< L" | ANGLE_platform_angle_d3d="
		<< (extensions.find(L"EGL_ANGLE_platform_angle_d3d") != std::wstring::npos ? L"yes" : L"no")
		<< L" | configs=" << availableConfigs
		<< L" | backend attempts: " << backendAttempts.str();
	#if defined(_M_ARM)
	displayInfo << L" | ANGLE debug: "
		<< (gEglDebugMessages.empty() ? L"<none>" : gEglDebugMessages);
	#endif
	m_lastDetails = displayInfo.str();

	const EGLint configAttributes[] =
	{
		EGL_RED_SIZE, 8,
		EGL_GREEN_SIZE, 8,
		EGL_BLUE_SIZE, 8,
		EGL_ALPHA_SIZE, 8,
		EGL_DEPTH_SIZE, 24,
		EGL_STENCIL_SIZE, 8,
		EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
		EGL_NONE
	};
	EGLint configCount = 0;
	m_lastStage = L"eglChooseConfig";
	if (!eglChooseConfig(m_display, configAttributes, nullptr, 0, &configCount) || configCount == 0)
	{
		m_lastError = eglGetError();
		m_lastDetails = displayDetailsForFailure(m_lastDetails, L"eglChooseConfig(count)", m_lastError);
		shutdown();
		return false;
	}
	EGLint returnedConfigCount = configCount;
	std::vector<EGLConfig> configs(static_cast<size_t>(configCount));
	if (!eglChooseConfig(m_display, configAttributes, configs.data(), configCount, &returnedConfigCount) ||
		returnedConfigCount == 0)
	{
		m_lastError = eglGetError();
		m_lastDetails = displayDetailsForFailure(m_lastDetails, L"eglChooseConfig(list)", m_lastError);
		shutdown();
		return false;
	}

	// Create the surface before the context.  Try every matching config rather
	// than trusting the first one: old ANGLE builds can expose a config that
	// satisfies eglChooseConfig but rejects GLES2 context creation.
	const EGLint contextAttributes[] =
	{
		EGL_CONTEXT_CLIENT_VERSION, 2,
		EGL_NONE
	};
	m_lastStage = L"eglBindAPI";
	if (!eglBindAPI(EGL_OPENGL_ES_API))
	{
		m_lastError = eglGetError();
		m_lastDetails = displayDetailsForFailure(m_lastDetails, L"eglBindAPI", m_lastError);
		shutdown();
		return false;
	}
	const EGLint surfaceAttributes[] =
	{
		EGL_ANGLE_SURFACE_RENDER_TO_BACK_BUFFER, EGL_TRUE,
		EGL_NONE
	};
	const std::wstring displayDetails = m_lastDetails;
	std::wostringstream attempts;
	EGLint lastContextError = EGL_BAD_CONFIG;
	bool contextCreated = false;
	EGLConfig selectedConfig = nullptr;
	for (EGLint index = 0; index < returnedConfigCount; ++index)
	{
		EGLConfig config = configs[static_cast<size_t>(index)];
		EGLint configId = 0;
		EGLint renderableType = 0;
		EGLint surfaceType = 0;
		EGLint depthSize = 0;
		EGLint stencilSize = 0;
		getConfigAttribute(m_display, config, EGL_CONFIG_ID, configId);
		getConfigAttribute(m_display, config, EGL_RENDERABLE_TYPE, renderableType);
		getConfigAttribute(m_display, config, EGL_SURFACE_TYPE, surfaceType);
		getConfigAttribute(m_display, config, EGL_DEPTH_SIZE, depthSize);
		getConfigAttribute(m_display, config, EGL_STENCIL_SIZE, stencilSize);

		EGLSurface candidateSurface = eglCreateWindowSurface(
			m_display,
			config,
			reinterpret_cast<EGLNativeWindowType>(surfaceProperties),
			surfaceAttributes);
		EGLint surfaceError = EGL_SUCCESS;
		if (candidateSurface == EGL_NO_SURFACE)
		{
			surfaceError = eglGetError();
			if (surfaceError == EGL_BAD_ATTRIBUTE)
			{
				candidateSurface = eglCreateWindowSurface(
					m_display,
					config,
					reinterpret_cast<EGLNativeWindowType>(surfaceProperties),
					nullptr);
				if (candidateSurface == EGL_NO_SURFACE)
					surfaceError = eglGetError();
			}
		}
		if (candidateSurface == EGL_NO_SURFACE)
		{
			if (index < 8)
				attempts << L" cfg" << configId << L":surface=0x" << std::hex
					<< static_cast<unsigned int>(surfaceError) << std::dec;
			continue;
		}

		EGLContext candidateContext = eglCreateContext(
			m_display, config, EGL_NO_CONTEXT, contextAttributes);
		EGLint contextError = EGL_SUCCESS;
		if (candidateContext == EGL_NO_CONTEXT)
		{
			contextError = eglGetError();
			// Keep the retry for diagnostics/legacy drivers, but do not hide a
			// failed config; continue to the next candidate when it fails.
			if (contextError == EGL_BAD_ATTRIBUTE)
			{
				const EGLint defaultContextAttributes[] = { EGL_NONE };
				candidateContext = eglCreateContext(
					m_display, config, EGL_NO_CONTEXT, defaultContextAttributes);
				if (candidateContext == EGL_NO_CONTEXT)
					contextError = eglGetError();
			}
		}
		if (candidateContext == EGL_NO_CONTEXT)
		{
			lastContextError = contextError;
			if (index < 8)
				attempts << L" cfg" << configId << L":context=0x" << std::hex
					<< static_cast<unsigned int>(contextError) << std::dec;
			eglDestroySurface(m_display, candidateSurface);
			continue;
		}

		m_surface = candidateSurface;
		m_context = candidateContext;
		selectedConfig = config;
		contextCreated = true;
		std::wostringstream configInfo;
		configInfo << displayDetails << L" | cfg=" << configId
			<< L" render=0x" << std::hex << renderableType
			<< L" surface=0x" << surfaceType << std::dec
			<< L" depth=" << depthSize << L" stencil=" << stencilSize
			<< L" candidates=" << returnedConfigCount;
		m_lastDetails = configInfo.str();
		break;
	}
	(void)selectedConfig;
	if (!contextCreated)
	{
		m_lastStage = L"eglCreateContext (all configs failed)";
		m_lastError = lastContextError;
		std::wostringstream failureInfo;
		failureInfo << displayDetails << L" | candidates=" << returnedConfigCount
			<< L" | attempts:" << attempts.str();
		m_lastDetails = failureInfo.str();
		shutdown();
		return false;
	}
	m_lastStage = L"eglMakeCurrent";
	if (!eglMakeCurrent(m_display, m_surface, m_surface, m_context))
	{
		m_lastError = eglGetError();
		m_lastDetails = displayDetailsForFailure(m_lastDetails, L"eglMakeCurrent", m_lastError);
		shutdown();
		return false;
	}

	m_initialized = true;
	m_lastError = EGL_SUCCESS;
	return true;
}

void UwpAngleContext::beginFrame(float width, float height)
{
	if (!m_initialized)
	{
		return;
	}

	glViewport(0, 0, static_cast<GLsizei>(width), static_cast<GLsizei>(height));
	glClearColor(0.05f, 0.18f, 0.32f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glDisable(GL_DEPTH_TEST);
}

void UwpAngleContext::present()
{
	if (!m_initialized)
	{
		return;
	}

	if (!eglSwapBuffers(m_display, m_surface))
	{
		m_lastError = eglGetError();
	}
}

void UwpAngleContext::render(float width, float height)
{
	beginFrame(width, height);
	present();
}

void UwpAngleContext::shutdown()
{
	if (m_display != EGL_NO_DISPLAY)
	{
		eglMakeCurrent(m_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
		if (m_context != EGL_NO_CONTEXT)
		{
			eglDestroyContext(m_display, m_context);
		}
		if (m_surface != EGL_NO_SURFACE)
		{
			eglDestroySurface(m_display, m_surface);
		}
		eglTerminate(m_display);
	}

	m_display = EGL_NO_DISPLAY;
	m_surface = EGL_NO_SURFACE;
	m_context = EGL_NO_CONTEXT;
	m_initialized = false;
}
