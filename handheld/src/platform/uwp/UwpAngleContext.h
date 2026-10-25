#pragma once

#include <EGL/egl.h>
#include <string>

namespace Windows { namespace UI { namespace Xaml { namespace Controls { ref class SwapChainPanel; } } } }

// The first UWP graphics layer is deliberately small.  It owns only the
// ANGLE EGL display/surface/context and leaves MCPE's renderer above it.
class UwpAngleContext final
{
public:
	UwpAngleContext() = default;
	~UwpAngleContext();

	bool initialize(Windows::UI::Xaml::Controls::SwapChainPanel^ panel);
	void beginFrame(float width, float height);
	void present();
	void render(float width, float height);
	void shutdown();

	bool isInitialized() const { return m_initialized; }
	EGLDisplay display() const { return m_display; }
	EGLContext context() const { return m_context; }
	EGLSurface surface() const { return m_surface; }
	EGLint lastError() const { return m_lastError; }
	const std::wstring& lastStage() const { return m_lastStage; }
	const std::wstring& lastDetails() const { return m_lastDetails; }

private:
	EGLDisplay m_display = EGL_NO_DISPLAY;
	EGLSurface m_surface = EGL_NO_SURFACE;
	EGLContext m_context = EGL_NO_CONTEXT;
	EGLint m_lastError = EGL_SUCCESS;
	std::wstring m_lastStage;
	std::wstring m_lastDetails;
	bool m_initialized = false;
};
