#include "pch.h"
#include "UwpAngleContext.h"

#define GL_GLEXT_PROTOTYPES 1
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <angle_windowsstore.h>
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

using namespace Platform;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::UI::Xaml::Controls;

namespace
{
	std::wstring narrowToWide(const char* value)
	{
		if (value == nullptr)
			return L"<null>";
		std::wstring result;
		while (*value != '\0')
			result.push_back(static_cast<unsigned char>(*value++));
		return result;
	}

	bool getConfigAttribute(EGLDisplay display, EGLConfig config, EGLint attribute, EGLint& value)
	{
		value = 0;
		return eglGetConfigAttrib(display, config, attribute, &value) == EGL_TRUE;
	}
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

	EGLint major = 0;
	EGLint minor = 0;
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
			eglTerminate(m_display);
			m_display = EGL_NO_DISPLAY;
		}
	}

	if (m_display == EGL_NO_DISPLAY && getPlatformDisplay != nullptr)
	{
		m_lastStage = L"eglGetPlatformDisplayEXT(D3D11)";
		m_display = getPlatformDisplay(
			EGL_PLATFORM_ANGLE_ANGLE,
			reinterpret_cast<void*>(EGL_DEFAULT_DISPLAY),
			d3d11DisplayAttributes);
		if (m_display != EGL_NO_DISPLAY)
		{
			m_lastStage = L"eglInitialize(D3D11)";
			if (!eglInitialize(m_display, &major, &minor))
			{
				m_lastError = eglGetError();
				eglTerminate(m_display);
				m_display = EGL_NO_DISPLAY;
			}
		}

		if (m_display == EGL_NO_DISPLAY)
		{
			m_lastStage = L"eglGetPlatformDisplayEXT(D3D9)";
			m_display = getPlatformDisplay(
				EGL_PLATFORM_ANGLE_ANGLE,
				reinterpret_cast<void*>(EGL_DEFAULT_DISPLAY),
				d3d9DisplayAttributes);
			if (m_display != EGL_NO_DISPLAY)
			{
				m_lastStage = L"eglInitialize(D3D9)";
				if (!eglInitialize(m_display, &major, &minor))
				{
					m_lastError = eglGetError();
					eglTerminate(m_display);
					m_display = EGL_NO_DISPLAY;
				}
			}
		}
	}
	if (m_display == EGL_NO_DISPLAY)
	{
		m_lastError = eglGetError();
		return false;
	}

	// Capture immutable display facts before selecting a config.  These values
	// distinguish an ANGLE/driver problem from a bad config supplied to the
	// context call and remain visible in the XAML diagnostic overlay.
	EGLint availableConfigs = 0;
	eglGetConfigs(m_display, nullptr, 0, &availableConfigs);
	std::wostringstream displayInfo;
	displayInfo << L"EGL " << major << L'.' << minor
		<< L" | EGLint=" << sizeof(EGLint)
		<< L" | vendor=" << narrowToWide(eglQueryString(m_display, EGL_VENDOR))
		<< L" | version=" << narrowToWide(eglQueryString(m_display, EGL_VERSION))
		<< L" | APIs=" << narrowToWide(eglQueryString(m_display, EGL_CLIENT_APIS))
		<< L" | configs=" << availableConfigs;
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
		shutdown();
		return false;
	}
	EGLint returnedConfigCount = configCount;
	std::vector<EGLConfig> configs(static_cast<size_t>(configCount));
	if (!eglChooseConfig(m_display, configAttributes, configs.data(), configCount, &returnedConfigCount) ||
		returnedConfigCount == 0)
	{
		m_lastError = eglGetError();
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
