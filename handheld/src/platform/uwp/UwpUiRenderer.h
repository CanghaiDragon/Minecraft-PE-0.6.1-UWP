#pragma once

#ifndef GL_GLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES 1
#endif
#include <GLES2/gl2.h>
#include <array>
#include <cstdint>
#include <string>
#include <vector>

class Font;

	// A small GLES2-only 2D layer retained for the in-world UWP diagnostic/HUD
	// seam.  The original Minecraft client owns the main menu and SelectWorld
	// screens; this class no longer presents those legacy screens.
class UwpUiRenderer final
{
public:
	enum class TextureSlot
	{
		Background,
		Title,
		Buttons,
		Font,
		DefaultWorld,
		MojangLogo
	};

	enum class Screen
	{
		MainMenu,
		WorldSelect,
		Game
	};

	enum class FontProfile
	{
		Mcpe061,
		Win10Like
	};

	enum HitTarget
	{
		TargetNone = -1,
		TargetSingleplayer = 0,
		TargetMultiplayer = 1,
		TargetOptions = 2,
		TargetQuit = 3,
		TargetWorldFirst = 100,
		TargetWorldDelete = 1000,
		TargetWorldCreate = 1001,
		TargetWorldBack = 1002,
		TargetWorldImport = 1003,
		// Retained for source compatibility with older UWP callers.  The game
		// screen no longer emits this target; original Minecraft PauseScreen
		// owns world exit through Esc.
		TargetGameBack = 1100
	};

	UwpUiRenderer() = default;
	~UwpUiRenderer();

	bool initialize();
	void render(float width, float height);
	// Draw only the packaged dirt texture during the short interval before the
	// original client has a Screen to render. This is a transition surface, not
	// a second menu or world renderer.
	void renderLoadingBackground(float width, float height);
	// Draw the platform startup surface while the original client is still
	// loading its packaged textures and constructing the first Screen.
	void renderStartupLoading(float width, float height);
	// Draw the original-style dirt save transition before a synchronous world
	// save/close.  MainPage remains the only EGL presentation owner.
	void renderSavingWorld(float width, float height);
	void setScreen(Screen screen);
	// When enabled, the game screen leaves the full-viewport background and
	// diagnostic terrain to UwpWorldRenderer while retaining only the temporary
	// diagnostic HUD.  World exit belongs to original Minecraft PauseScreen.
	void setRealWorldRendering(bool enabled) { m_realWorldRendering = enabled; }
	bool realWorldRendering() const { return m_realWorldRendering; }
	Screen screen() const { return m_screen; }
	void setFontProfile(FontProfile profile);
	FontProfile fontProfile() const { return m_fontProfile; }
	void setPointer(float x, float y, bool pressed);
	int hitTest(float width, float height) const;
	void setSelectedWorld(int index);
	void setWorldNames(const std::vector<std::wstring>& names);
	void setStatusText(const std::wstring& text);
	void setStatusDetailText(const std::wstring& text);
	// Borrows the original MCPE bitmap font owned by the world renderer for the
	// in-world HUD seam.  Menu and SelectWorld text are now drawn by Minecraft.
	void setOriginalFont(Font* font) { m_originalFont = font; }
	void setGameTelemetry(const std::wstring& worldName, std::uint64_t tick,
		float playerX, float playerZ, float dayTime);
	// Builds a GLES font atlas from the packaged Minecraft TTF.  The call is
	// made after ANGLE has initialized so the generated texture belongs to the
	// same EGL context as the rest of the menu.
	bool setFontFilePath(const std::wstring& path);
	void setTexturePixels(TextureSlot slot, unsigned int width, unsigned int height,
		const unsigned char* pixels, unsigned int byteCount);
	void shutdown();

	bool isInitialized() const { return m_initialized; }

private:
	struct Vertex
	{
		GLfloat x;
		GLfloat y;
		GLfloat r;
		GLfloat g;
		GLfloat b;
		GLfloat a;
		GLfloat u;
		GLfloat v;
	};

	struct TtfGlyph
	{
		float sourceX = 0.0f;
		float sourceY = 0.0f;
		float sourceWidth = 0.0f;
		float sourceHeight = 0.0f;
		float bearingX = 0.0f;
		float bearingY = 0.0f;
		float advance = 0.0f;
		bool drawable = false;
	};

	void drawRect(float x, float y, float width, float height,
		float r, float g, float b, float a);
	void drawGradient(float x, float y, float width, float height,
		float topR, float topG, float topB,
		float bottomR, float bottomG, float bottomB);
	void drawTexturedRect(GLuint texture, unsigned int textureWidth, unsigned int textureHeight,
		float x, float y, float width, float height,
		float sx, float sy, float sw, float sh,
		float r = 1.0f, float g = 1.0f, float b = 1.0f, float a = 1.0f);
	void drawText(const std::wstring& text, float x, float y, float scale,
		float r = 1.0f, float g = 1.0f, float b = 1.0f, float a = 1.0f);
	bool useBitmapFontForUi() const;
	void drawTtfText(const std::wstring& text, float x, float y, float scale,
		float r, float g, float b, float a);
	void drawCenteredText(const std::wstring& text, float centerX, float y, float scale,
		float r = 1.0f, float g = 1.0f, float b = 1.0f, float a = 1.0f);
	float measureText(const std::wstring& text, float scale) const;
	float measureTtfText(const std::wstring& text, float scale) const;
	void drawTiledBackground(float width, float height);
	void drawButton(float x, float y, float width, float height, bool selected);
	void drawMainMenu(float width, float height);
	void drawWorldSelect(float width, float height);
	void drawGame(float width, float height);
	void drawOriginalFontStatus(float width, float height);
	void drawOriginalFontTick(float width, float height);
	void uploadTexture(GLuint& texture, unsigned int& textureWidth, unsigned int& textureHeight,
		bool& repeat, unsigned int width, unsigned int height,
		const unsigned char* pixels, unsigned int byteCount, bool shouldRepeat);
	bool buildTtfAtlas(const std::wstring& path);

	GLuint m_program = 0;
	GLint m_position = -1;
	GLint m_color = -1;
	GLint m_texCoord = -1;
	GLint m_viewport = -1;
	GLint m_texture = -1;
	GLint m_useTexture = -1;
	GLuint m_backgroundTexture = 0;
	GLuint m_titleTexture = 0;
	GLuint m_buttonsTexture = 0;
	GLuint m_fontTexture = 0;
	GLuint m_ttfTexture = 0;
	GLuint m_defaultWorldTexture = 0;
	GLuint m_mojangLogoTexture = 0;
	unsigned int m_backgroundWidth = 0;
	unsigned int m_backgroundHeight = 0;
	unsigned int m_titleWidth = 0;
	unsigned int m_titleHeight = 0;
	unsigned int m_buttonsWidth = 0;
	unsigned int m_buttonsHeight = 0;
	unsigned int m_fontWidth = 0;
	unsigned int m_fontHeight = 0;
	unsigned int m_ttfWidth = 0;
	unsigned int m_ttfHeight = 0;
	unsigned int m_defaultWorldWidth = 0;
	unsigned int m_defaultWorldHeight = 0;
	unsigned int m_mojangLogoWidth = 0;
	unsigned int m_mojangLogoHeight = 0;
	bool m_backgroundRepeat = false;
	bool m_ttfRepeat = false;
	bool m_mojangLogoRepeat = false;
	bool m_initialized = false;
	Screen m_screen = Screen::Game;
	std::vector<std::wstring> m_worldNames;
	int m_selectedWorld = -1;
	std::wstring m_statusText;
	std::wstring m_statusDetailText;
	std::array<unsigned char, 256> m_fontAdvance{};
	// Width of the non-transparent part of each glyph in the 8x8 atlas cell.
	// Keeping this separate from advance preserves MCPE's original spacing
	// while preventing a narrow glyph quad from extending into the next glyph.
	std::array<unsigned char, 256> m_fontVisibleWidth{};
	std::array<TtfGlyph, 256> m_ttfGlyphs{};
	bool m_ttfReady = false;
	bool m_realWorldRendering = false;
	Font* m_originalFont = nullptr;
	bool m_originalFontVboInitialized = false;
	float m_ttfAscent = 0.0f;
	// Rasterize above the logical 8-pixel MCPE cell, then downsample through
	// the GLES texture filter so thin TTF strokes remain intact.
	float m_ttfBaseSize = 32.0f;
	FontProfile m_fontProfile = FontProfile::Mcpe061;
	float m_uiScale = 1.0f;
	float m_pointerX = -1.0f;
	float m_pointerY = -1.0f;
	bool m_pointerPressed = false;
	std::wstring m_gameWorldName;
	std::uint64_t m_gameTick = 0;
	float m_gamePlayerX = 0.0f;
	float m_gamePlayerZ = 0.0f;
	float m_gameDayTime = 0.0f;
};
