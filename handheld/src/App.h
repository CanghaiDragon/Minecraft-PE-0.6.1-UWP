#ifndef APP_H__
#define APP_H__

#ifdef __APPLE__
#define NO_EGL
#endif
#ifdef LINUX
#define NO_EGL
#endif
#if defined(STANDALONE_SERVER) && !defined(MCPE_UWP_CLIENT_LIFECYCLE_ONLY)
#define NO_EGL
#endif
#ifdef WIN32_WGL
#define NO_EGL
#endif

#include "AppPlatform.h"
#ifndef NO_EGL 
    #include <EGL/egl.h>
#endif
#include "platform/log.h"

typedef struct AppContext {
#ifndef NO_EGL
	EGLDisplay display;
	EGLContext context;
	EGLSurface surface;
#endif
#ifdef WIN32_WGL
	// Desktop WGL owns its native context in the Win32 platform layer.
	void* graphicsContext;
	void (*swapGraphicsBuffers)(void* graphicsContext);
#endif
	AppPlatform* platform;
	bool doRender;
} AppContext;


class App
{
public:
    App()
	:	_finished(false),
		_inited(false)
	{
		_context.platform = 0;
	}
	virtual ~App() {}

	void init(AppContext& c) {
        _context = c;
		init();
		_inited = true;
    }
	bool isInited() { return _inited; }

	virtual AppPlatform* platform() { return _context.platform; }

	// Platform hosts that cannot enter the full virtual init() path yet can
	// still attach the real platform contract and mark the AppContext hand-off
	// complete.  This does not call init(), swap buffers, or any client loop.
	void bindPlatformContext(AppPlatform* platform) {
		_context.platform = platform;
		_inited = platform != 0;
	}

	// Lifecycle-only hosts can attach the real display/context/surface without
	// invoking the virtual init() chain.  doRender remains controlled by the
	// caller, so UWP's CompositionTarget remains the sole presenter.
	void bindContextWithoutInit(const AppContext& context) {
		_context = context;
		_inited = _context.platform != 0;
	}

	void onGraphicsReset(AppContext& c) {
		_context = c;
		onGraphicsReset();
	}

    virtual void audioEngineOn () {}
    virtual void audioEngineOff() {}
    
	virtual void destroy() {}

    virtual void loadState(void* state, int stateSize) {}
    virtual bool saveState(void** state, int* stateSize) { return false; }
    
	void swapBuffers() {
#ifndef NO_EGL
		if (_context.doRender)
			eglSwapBuffers(_context.display, _context.surface);
#elif defined(WIN32_WGL)
		if (_context.doRender && _context.swapGraphicsBuffers)
			_context.swapGraphicsBuffers(_context.graphicsContext);
#endif
	}

	virtual void draw() {}
	virtual void update() {};// = 0;
	virtual void setSize(int width, int height) {}
	
	virtual void quit() { _finished = true; }
	virtual bool wantToQuit() { return _finished; }
	virtual bool handleBack(bool isDown) { return false; }

protected:
	virtual void init() {}
	//virtual void onGraphicsLost() = 0;
	virtual void onGraphicsReset() = 0;

private:
	bool _inited;
	bool _finished;
    AppContext _context;
};

#endif//APP_H__
