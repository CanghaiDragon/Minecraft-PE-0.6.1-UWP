#include "pch.h"
#include "UwpUiRenderer.h"

#include <dwrite.h>
#include <wrl/client.h>
#include <algorithm>
#include <cstdio>
#include <cmath>
#include <cstring>
#include <vector>

#pragma comment(lib, "dwrite.lib")

namespace
{
struct FontProfileConfig
{
	float tracking;
	float advanceScale;
	float minimumAdvance;
};

FontProfileConfig getFontProfileConfig(UwpUiRenderer::FontProfile profile)
{
	if (profile == UwpUiRenderer::FontProfile::Mcpe061)
		return { 0.0f, 1.0f, 4.0f };

	// Keep the optional profile in base UI pixels.  It is not part of the
	// original MCPE profile and is intentionally kept separate for comparison.
	return { 1.0f, 1.0f, 4.0f };
}

const char* kVertexShader =
	"attribute vec2 a_position;\n"
	"attribute vec4 a_color;\n"
	"attribute vec2 a_texCoord;\n"
	"uniform vec2 u_viewport;\n"
	"varying vec4 v_color;\n"
	"varying vec2 v_texCoord;\n"
	"void main() {\n"
	"  vec2 ndc = vec2((a_position.x / u_viewport.x) * 2.0 - 1.0,\n"
	"                 1.0 - (a_position.y / u_viewport.y) * 2.0);\n"
	"  gl_Position = vec4(ndc, 0.0, 1.0);\n"
	"  v_color = a_color;\n"
	"  v_texCoord = a_texCoord;\n"
	"}\n";

const char* kFragmentShader =
	"precision mediump float;\n"
	"uniform sampler2D u_texture;\n"
	"uniform float u_useTexture;\n"
	"varying vec4 v_color;\n"
	"varying vec2 v_texCoord;\n"
	"void main() {\n"
	"  vec4 color = v_color;\n"
	"  if (u_useTexture > 0.5) color *= texture2D(u_texture, v_texCoord);\n"
	"  gl_FragColor = color;\n"
	"}\n";

GLuint compileShader(GLenum type, const char* source)
{
	GLuint shader = glCreateShader(type);
	glShaderSource(shader, 1, &source, nullptr);
	glCompileShader(shader);

	GLint success = GL_FALSE;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
	if (!success)
	{
		char log[1024] = {};
		glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
		std::fprintf(stderr, "UWP UI shader compile failed: %s\n", log);
		glDeleteShader(shader);
		return 0;
	}
	return shader;
}

GLuint createProgram()
{
	GLuint vertex = compileShader(GL_VERTEX_SHADER, kVertexShader);
	GLuint fragment = compileShader(GL_FRAGMENT_SHADER, kFragmentShader);
	if (!vertex || !fragment)
	{
		if (vertex) glDeleteShader(vertex);
		if (fragment) glDeleteShader(fragment);
		return 0;
	}

	GLuint program = glCreateProgram();
	glAttachShader(program, vertex);
	glAttachShader(program, fragment);
	glBindAttribLocation(program, 0, "a_position");
	glBindAttribLocation(program, 1, "a_color");
	glBindAttribLocation(program, 2, "a_texCoord");
	glLinkProgram(program);
	glDeleteShader(vertex);
	glDeleteShader(fragment);

	GLint success = GL_FALSE;
	glGetProgramiv(program, GL_LINK_STATUS, &success);
	if (!success)
	{
		char log[1024] = {};
		glGetProgramInfoLog(program, sizeof(log), nullptr, log);
		std::fprintf(stderr, "UWP UI shader link failed: %s\n", log);
		glDeleteProgram(program);
		return 0;
	}
	return program;
}
}

UwpUiRenderer::~UwpUiRenderer()
{
	shutdown();
}

bool UwpUiRenderer::initialize()
{
	if (m_initialized)
		return true;

	m_program = createProgram();
	if (!m_program)
		return false;

	m_position = glGetAttribLocation(m_program, "a_position");
	m_color = glGetAttribLocation(m_program, "a_color");
	m_texCoord = glGetAttribLocation(m_program, "a_texCoord");
	m_viewport = glGetUniformLocation(m_program, "u_viewport");
	m_texture = glGetUniformLocation(m_program, "u_texture");
	m_useTexture = glGetUniformLocation(m_program, "u_useTexture");
	m_initialized = m_position >= 0 && m_color >= 0 && m_texCoord >= 0 &&
		m_viewport >= 0 && m_texture >= 0 && m_useTexture >= 0;
	if (!m_initialized)
		shutdown();
	return m_initialized;
}

void UwpUiRenderer::drawRect(float x, float y, float width, float height,
	float r, float g, float b, float a)
{
	Vertex vertices[4] = {
		{ x,         y,          r, g, b, a, 0.0f, 0.0f },
		{ x + width, y,          r, g, b, a, 0.0f, 0.0f },
		{ x,         y + height, r, g, b, a, 0.0f, 0.0f },
		{ x + width, y + height, r, g, b, a, 0.0f, 0.0f }
	};
	glVertexAttribPointer(m_position, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), &vertices[0].x);
	glVertexAttribPointer(m_color, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), &vertices[0].r);
	glVertexAttribPointer(m_texCoord, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), &vertices[0].u);
	glEnableVertexAttribArray(m_position);
	glEnableVertexAttribArray(m_color);
	glEnableVertexAttribArray(m_texCoord);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	glDisableVertexAttribArray(m_position);
	glDisableVertexAttribArray(m_color);
	glDisableVertexAttribArray(m_texCoord);
}

void UwpUiRenderer::drawGradient(float x, float y, float width, float height,
	float topR, float topG, float topB,
	float bottomR, float bottomG, float bottomB)
{
	Vertex vertices[4] = {
		{ x,         y,          topR,    topG,    topB,    1.0f, 0.0f, 0.0f },
		{ x + width, y,          topR,    topG,    topB,    1.0f, 0.0f, 0.0f },
		{ x,         y + height, bottomR, bottomG, bottomB, 1.0f, 0.0f, 0.0f },
		{ x + width, y + height, bottomR, bottomG, bottomB, 1.0f, 0.0f, 0.0f }
	};
	glVertexAttribPointer(m_position, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), &vertices[0].x);
	glVertexAttribPointer(m_color, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), &vertices[0].r);
	glVertexAttribPointer(m_texCoord, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), &vertices[0].u);
	glEnableVertexAttribArray(m_position);
	glEnableVertexAttribArray(m_color);
	glEnableVertexAttribArray(m_texCoord);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	glDisableVertexAttribArray(m_position);
	glDisableVertexAttribArray(m_color);
	glDisableVertexAttribArray(m_texCoord);
}

void UwpUiRenderer::drawTexturedRect(GLuint texture, unsigned int textureWidth, unsigned int textureHeight,
	float x, float y, float width, float height,
	float sx, float sy, float sw, float sh,
	float r, float g, float b, float a)
{
	if (!texture || textureWidth == 0 || textureHeight == 0)
		return;

	const GLfloat u0 = sx / static_cast<GLfloat>(textureWidth);
	const GLfloat u1 = (sx + sw) / static_cast<GLfloat>(textureWidth);
	const GLfloat v0 = sy / static_cast<GLfloat>(textureHeight);
	const GLfloat v1 = (sy + sh) / static_cast<GLfloat>(textureHeight);
	Vertex vertices[4] = {
		{ x,         y,          r, g, b, a, u0, v0 },
		{ x + width, y,          r, g, b, a, u1, v0 },
		{ x,         y + height, r, g, b, a, u0, v1 },
		{ x + width, y + height, r, g, b, a, u1, v1 }
	};
	glBindTexture(GL_TEXTURE_2D, texture);
	glUniform1f(m_useTexture, 1.0f);
	glVertexAttribPointer(m_position, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), &vertices[0].x);
	glVertexAttribPointer(m_color, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), &vertices[0].r);
	glVertexAttribPointer(m_texCoord, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), &vertices[0].u);
	glEnableVertexAttribArray(m_position);
	glEnableVertexAttribArray(m_color);
	glEnableVertexAttribArray(m_texCoord);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	glDisableVertexAttribArray(m_position);
	glDisableVertexAttribArray(m_color);
	glDisableVertexAttribArray(m_texCoord);
}

void UwpUiRenderer::drawText(const std::wstring& text, float x, float y, float scale,
	float r, float g, float b, float a)
{
	// The packaged minecraft.ttf is useful as a fallback while the asynchronous
	// menu texture load is still in flight, but MCPE's original GUI uses the
	// pixel font atlas (font/default8.png).  Prefer that atlas for the menu
	// screens whenever it is available; this keeps button labels on the same
	// rendering path as the original client without affecting in-game overlays.
	if (m_ttfReady && !useBitmapFontForUi())
	{
		drawTtfText(text, x, y, scale, r, g, b, a);
		return;
	}

	if (!m_fontTexture || m_fontWidth < 16 || m_fontHeight < 16 || scale <= 0.0f)
		return;

	const float cellWidth = static_cast<float>(m_fontWidth) / 16.0f;
	const float cellHeight = static_cast<float>(m_fontHeight) / 16.0f;
	// Use the original 8x8 atlas cell vertically; horizontal geometry is
	// reduced to the glyph's visible columns below.
	const float glyphSourceSize = 7.99f / 8.0f;
	const float glyphSize = 7.99f * scale;
	const FontProfileConfig profile = getFontProfileConfig(m_fontProfile);
	float cursor = x;
	for (std::wstring::size_type index = 0; index < text.size(); ++index)
	{
		const wchar_t character = text[index];
		const unsigned int code = static_cast<unsigned int>(character) & 0xffu;
		const float rawAdvance = m_fontAdvance[code] != 0
			? static_cast<float>(m_fontAdvance[code])
			: (character == L' ' ? 4.0f : 8.0f);
		const float advance = rawAdvance < profile.minimumAdvance
			? profile.minimumAdvance
			: rawAdvance;

		if (character != L' ')
		{
			const float sourceX = static_cast<float>(code % 16u) * cellWidth;
			const float sourceY = static_cast<float>(code / 16u) * cellHeight;
			// Font::buildChar uses an 8x8 cell, but the texture quad is mostly
			// transparent for narrow glyphs such as 'i' and 'l'.  Draw only the
			// visible columns here.  This keeps one source texel at one logical
			// pixel and prevents the transparent part of a narrow quad from
			// overlapping the following glyph under ANGLE.
			const float visibleWidth = m_fontVisibleWidth[code] != 0
				? static_cast<float>(m_fontVisibleWidth[code])
				: cellWidth;
			const float sourceVisibleWidth = visibleWidth < (cellWidth - 0.01f)
				? visibleWidth
				: (cellWidth - 0.01f);
			drawTexturedRect(m_fontTexture, m_fontWidth, m_fontHeight,
				cursor, y, visibleWidth * scale, glyphSize,
				sourceX, sourceY, sourceVisibleWidth,
				cellHeight * glyphSourceSize, r, g, b, a);
		}
		cursor += advance * profile.advanceScale * scale;
		if (index + 1 < text.size())
			cursor += profile.tracking * m_uiScale;
	}
}

void UwpUiRenderer::drawTtfText(const std::wstring& text, float x, float y, float scale,
	float r, float g, float b, float a)
{
	if (!m_ttfTexture || scale <= 0.0f)
		return;

	const FontProfileConfig profile = getFontProfileConfig(m_fontProfile);
	const float glyphScale = scale * (8.0f / m_ttfBaseSize);
	float cursor = x;
	const float baseline = y + m_ttfAscent * glyphScale;
	for (std::wstring::size_type index = 0; index < text.size(); ++index)
	{
		const unsigned int code = static_cast<unsigned int>(text[index]) & 0xffu;
		const TtfGlyph& glyph = m_ttfGlyphs[code];
		if (glyph.drawable)
		{
			drawTexturedRect(m_ttfTexture, m_ttfWidth, m_ttfHeight,
				cursor + glyph.bearingX * glyphScale,
				baseline + glyph.bearingY * glyphScale,
				glyph.sourceWidth * glyphScale,
				glyph.sourceHeight * glyphScale,
				glyph.sourceX, glyph.sourceY,
				glyph.sourceWidth, glyph.sourceHeight,
				r, g, b, a);
		}
		cursor += glyph.advance * glyphScale;
		if (index + 1 < text.size())
			cursor += profile.tracking * m_uiScale;
	}
}

void UwpUiRenderer::drawCenteredText(const std::wstring& text, float centerX, float y, float scale,
	float r, float g, float b, float a)
{
	const float textWidth = measureText(text, scale);
	const float x = centerX - textWidth * 0.5f;
	// Match Font::drawShadow from the Win32 renderer: draw a darkened copy
	// one logical pixel down/right before drawing the normal glyphs.
	drawText(text, x + m_uiScale, y + m_uiScale, scale,
		r * 0.25f, g * 0.25f, b * 0.25f, a);
	drawText(text, x, y, scale, r, g, b, a);
}

float UwpUiRenderer::measureText(const std::wstring& text, float scale) const
{
	if (m_ttfReady && !useBitmapFontForUi())
		return measureTtfText(text, scale);

	const FontProfileConfig profile = getFontProfileConfig(m_fontProfile);
	float width = 0.0f;
	for (std::wstring::size_type index = 0; index < text.size(); ++index)
	{
		const wchar_t character = text[index];
		const unsigned int code = static_cast<unsigned int>(character) & 0xffu;
		const float rawAdvance = m_fontAdvance[code] != 0
			? static_cast<float>(m_fontAdvance[code])
			: (character == L' ' ? 4.0f : 8.0f);
		const float advance = rawAdvance < profile.minimumAdvance
			? profile.minimumAdvance
			: rawAdvance;
		width += advance * profile.advanceScale * scale;
		if (index + 1 < text.size())
			width += profile.tracking * m_uiScale;
	}
	return width;
}

bool UwpUiRenderer::useBitmapFontForUi() const
{
	const bool menuScreen = m_screen == Screen::MainMenu ||
		m_screen == Screen::WorldSelect;
	return menuScreen && m_fontTexture != 0 &&
		m_fontWidth >= 16 && m_fontHeight >= 16;
}

float UwpUiRenderer::measureTtfText(const std::wstring& text, float scale) const
{
	const FontProfileConfig profile = getFontProfileConfig(m_fontProfile);
	float width = 0.0f;
	for (std::wstring::size_type index = 0; index < text.size(); ++index)
	{
		const unsigned int code = static_cast<unsigned int>(text[index]) & 0xffu;
		width += m_ttfGlyphs[code].advance * scale * (8.0f / m_ttfBaseSize);
		if (index + 1 < text.size())
			width += profile.tracking * m_uiScale;
	}
	return width;
}

void UwpUiRenderer::drawTiledBackground(float width, float height)
{
	if (!m_backgroundTexture)
		return;
	// MCPE's GUI is authored against a 854x480 surface.  The texture itself
	// is 16x16, so one source texel must grow with the GUI scale instead of
	// remaining one physical pixel on a high-DPI UWP window.
	const GLfloat uiScale = height / 480.0f;
	// The original 0.6.x menu uses a visibly enlarged dirt tile on the
	// desktop-sized surface.  Keep that background-only scale separate from
	// the logical UI scale so the title and controls are unaffected.
	const GLfloat backgroundTileScale = 4.0f;
	const GLfloat u = width / (static_cast<GLfloat>(m_backgroundWidth) * uiScale * backgroundTileScale);
	const GLfloat v = height / (static_cast<GLfloat>(m_backgroundHeight) * uiScale * backgroundTileScale);
	const GLfloat u0 = 0.0f;
	const GLfloat u1 = u;
	const GLfloat v0 = 0.0f;
	const GLfloat v1 = v;
	Vertex vertices[4] = {
		// Match Screen::renderDirtBackground()'s original 0x404040 tint.
		// Keeping this value here makes the UWP transition surfaces identical
		// to the original ProgressScreen instead of leaving Saving world
		// visibly brighter than the loading screen.
		{ 0.0f, 0.0f, width > 0 ? 0.25f : 0.0f, 0.25f, 0.25f, 1.0f, u0, v0 },
		{ width, 0.0f, 0.25f, 0.25f, 0.25f, 1.0f, u1, v0 },
		{ 0.0f, height, 0.25f, 0.25f, 0.25f, 1.0f, u0, v1 },
		{ width, height, 0.25f, 0.25f, 0.25f, 1.0f, u1, v1 }
	};
	glBindTexture(GL_TEXTURE_2D, m_backgroundTexture);
	glUniform1f(m_useTexture, 1.0f);
	glVertexAttribPointer(m_position, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), &vertices[0].x);
	glVertexAttribPointer(m_color, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), &vertices[0].r);
	glVertexAttribPointer(m_texCoord, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), &vertices[0].u);
	glEnableVertexAttribArray(m_position);
	glEnableVertexAttribArray(m_color);
	glEnableVertexAttribArray(m_texCoord);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	glDisableVertexAttribArray(m_position);
	glDisableVertexAttribArray(m_color);
	glDisableVertexAttribArray(m_texCoord);
}

void UwpUiRenderer::drawButton(float x, float y, float width, float height, bool selected)
{
	if (selected)
	{
		drawRect(x - 3.0f, y - 3.0f, width + 6.0f, height + 6.0f,
			0.95f, 0.80f, 0.35f, 1.0f);
	}
	if (m_buttonsTexture)
	{
		const float yImage = selected ? 40.0f : 20.0f;
		drawTexturedRect(m_buttonsTexture, m_buttonsWidth, m_buttonsHeight,
			x, y, width * 0.5f, height, 0.0f, 46.0f + yImage, 100.0f, 20.0f);
		drawTexturedRect(m_buttonsTexture, m_buttonsWidth, m_buttonsHeight,
			x + width * 0.5f, y, width * 0.5f, height, 100.0f, 46.0f + yImage, 100.0f, 20.0f);
		return;
	}
	drawGradient(x, y, width, height,
		selected ? 0.34f : 0.20f,
		selected ? 0.42f : 0.27f,
		selected ? 0.56f : 0.36f,
		selected ? 0.18f : 0.12f,
		selected ? 0.25f : 0.17f,
		selected ? 0.38f : 0.23f);
	drawRect(x + 2.0f, y + 2.0f, width - 4.0f, 1.0f,
		1.0f, 1.0f, 1.0f, selected ? 0.38f : 0.16f);
}

void UwpUiRenderer::drawMainMenu(float width, float height)
{
	const float uiScale = height / 480.0f;
	if (m_titleTexture)
	{
		const float titleScale = 2.0f;
		const float titleWidth = static_cast<float>(m_titleWidth) * uiScale * titleScale;
		const float titleHeight = static_cast<float>(m_titleHeight) * uiScale * titleScale;
		drawTexturedRect(m_titleTexture, m_titleWidth, m_titleHeight,
			(width - titleWidth) * 0.5f, 4.0f * uiScale, titleWidth, titleHeight,
			0.0f, 0.0f, static_cast<float>(m_titleWidth), static_cast<float>(m_titleHeight));
	}

	const float buttonWidth = width * 0.40f;
	const float buttonHeight = height * (34.0f / 480.0f);
	const float buttonX = (width - buttonWidth) * 0.5f;
	const float firstY = height * (214.0f / 480.0f);
	const float gap = height * (10.0f / 480.0f);
	// Keep the baseline version's original MCPE GUI scale.  Glyph-specific
	// visible widths below handle narrow letters; do not compensate by scaling
	// the whole font, which changes the menu proportions.
	const float fontScale = uiScale * 1.5f;
	static const wchar_t* labels[] = { L"Singleplayer", L"Multiplayer", L"Options", L"Quit" };
	const int hovered = hitTest(width, height);
	for (int i = 0; i < 4; ++i)
	{
		const float y = firstY + i * (buttonHeight + gap);
		const bool selected = hovered == i;
		drawButton(buttonX, y, buttonWidth, buttonHeight, selected);
		drawCenteredText(labels[i], width * 0.5f,
			y + (buttonHeight - 8.0f * fontScale) * 0.5f,
			fontScale,
			selected ? 1.0f : 0.8784f,
			selected ? 1.0f : 0.8784f,
			selected ? 0.6275f : 0.8784f);
	}

	if (!m_statusText.empty())
	{
		drawCenteredText(m_statusText, width * 0.5f,
			m_statusDetailText.empty() ? height - 34.0f * uiScale : height - 54.0f * uiScale,
			uiScale * 1.6f, 1.0f, 0.84f, 0.74f, 1.0f);
	}
	if (!m_statusDetailText.empty())
	{
		drawCenteredText(m_statusDetailText, width * 0.5f, height - 34.0f * uiScale,
			uiScale * 1.2f, 0.92f, 0.82f, 0.68f, 1.0f);
	}
}

void UwpUiRenderer::drawWorldSelect(float width, float height)
{
	const float uiScale = height / 480.0f;
	const float fontScale = uiScale * 1.8f;
	drawCenteredText(L"Select world", width * 0.5f, 8.0f * uiScale, uiScale * 2.25f);

	if (m_worldNames.empty())
	{
		drawCenteredText(L"No saved worlds", width * 0.5f, 220.0f * uiScale,
			uiScale * 2.0f, 0.85f, 0.85f, 0.85f, 1.0f);
	}
	else
	{
		const float cardWidth = 150.0f * uiScale;
		const float cardHeight = 102.0f * uiScale;
		const float cardGap = 12.0f * uiScale;
		const float totalWidth = cardWidth * static_cast<float>(m_worldNames.size())
			+ cardGap * static_cast<float>(m_worldNames.size() - 1);
		const float firstX = (width - totalWidth) * 0.5f;
		const float cardY = 72.0f * uiScale;
		const int hovered = hitTest(width, height);

		for (unsigned int i = 0; i < m_worldNames.size(); ++i)
		{
			const float x = firstX + i * (cardWidth + cardGap);
			const bool selected = static_cast<int>(i) == m_selectedWorld ||
				hovered == TargetWorldFirst + static_cast<int>(i);
			if (selected)
			{
				drawRect(x - 4.0f * uiScale, cardY - 4.0f * uiScale,
					cardWidth + 8.0f * uiScale, cardHeight + 8.0f * uiScale,
					0.95f, 0.80f, 0.35f, 1.0f);
			}
			drawRect(x, cardY, cardWidth, cardHeight,
				0.12f, 0.16f, 0.22f, 0.94f);
			if (m_defaultWorldTexture)
			{
				const float previewWidth = 64.0f * uiScale;
				const float previewHeight = 48.0f * uiScale;
				drawTexturedRect(m_defaultWorldTexture, m_defaultWorldWidth, m_defaultWorldHeight,
					x + (cardWidth - previewWidth) * 0.5f, cardY + 8.0f * uiScale,
					previewWidth, previewHeight, 0.0f, 0.0f,
					static_cast<float>(m_defaultWorldWidth), static_cast<float>(m_defaultWorldHeight));
			}

			std::wstring name = m_worldNames[i];
			if (name.length() > 18)
				name.resize(18);
			drawCenteredText(name, x + cardWidth * 0.5f,
				cardY + 66.0f * uiScale, fontScale,
				selected ? 1.0f : 0.8784f,
				selected ? 1.0f : 0.8784f,
				selected ? 0.6275f : 0.8784f);
		}
	}

	const float buttonWidth = 84.0f * uiScale;
	const float buttonHeight = 26.0f * uiScale;
	const float buttonGap = 8.0f * uiScale;
	const float buttonY = height - 28.0f * uiScale;
	const float buttonStart = width * 0.5f - (buttonWidth * 2.0f + buttonGap * 1.5f);
	const int hovered = hitTest(width, height);
	const float fontY = buttonY + (buttonHeight - 8.0f * fontScale) * 0.5f;

	drawButton(buttonStart, buttonY, buttonWidth, buttonHeight, hovered == TargetWorldDelete);
	drawCenteredText(L"Edit", buttonStart + buttonWidth * 0.5f, fontY, fontScale,
		hovered == TargetWorldDelete ? 1.0f : 0.8784f,
		hovered == TargetWorldDelete ? 1.0f : 0.8784f,
		hovered == TargetWorldDelete ? 0.6275f : 0.8784f);
	drawButton(buttonStart + buttonWidth + buttonGap, buttonY, buttonWidth, buttonHeight,
		hovered == TargetWorldImport);
	drawCenteredText(L"Import", buttonStart + buttonWidth * 1.5f + buttonGap, fontY, fontScale,
		hovered == TargetWorldImport ? 1.0f : 0.8784f,
		hovered == TargetWorldImport ? 1.0f : 0.8784f,
		hovered == TargetWorldImport ? 0.6275f : 0.8784f);
	drawButton(buttonStart + (buttonWidth + buttonGap) * 2.0f, buttonY, buttonWidth, buttonHeight,
		hovered == TargetWorldCreate);
	drawCenteredText(L"Create new", buttonStart + buttonWidth * 2.5f + buttonGap * 2.0f, fontY, fontScale,
		hovered == TargetWorldCreate ? 1.0f : 0.8784f,
		hovered == TargetWorldCreate ? 1.0f : 0.8784f,
		hovered == TargetWorldCreate ? 0.6275f : 0.8784f);
	drawButton(buttonStart + (buttonWidth + buttonGap) * 3.0f, buttonY,
		buttonWidth, buttonHeight, hovered == TargetWorldBack);
	drawCenteredText(L"Back", buttonStart + buttonWidth * 3.5f + buttonGap * 3.0f, fontY, fontScale,
		hovered == TargetWorldBack ? 1.0f : 0.8784f,
		hovered == TargetWorldBack ? 1.0f : 0.8784f,
		hovered == TargetWorldBack ? 0.6275f : 0.8784f);

	if (!m_statusText.empty())
	{
		drawCenteredText(m_statusText, width * 0.5f,
			m_statusDetailText.empty() ? height - 58.0f * uiScale : height - 78.0f * uiScale,
			uiScale * 1.25f, 1.0f, 0.84f, 0.74f, 1.0f);
	}
	if (!m_statusDetailText.empty())
	{
		drawCenteredText(m_statusDetailText, width * 0.5f, height - 58.0f * uiScale,
			uiScale * 1.05f, 0.92f, 0.82f, 0.68f, 1.0f);
	}
}

void UwpUiRenderer::drawGame(float width, float height)
{
	const float uiScale = height / 480.0f;
	const float horizon = height * 0.56f;

	if (!m_realWorldRendering)
	{
		// Keep the diagnostic scene as a loading fallback until the native
		// Level has produced the first terrain mesh.
		drawGradient(0.0f, 0.0f, width, horizon,
			0.28f, 0.55f, 0.82f,
			0.66f, 0.82f, 0.94f);
		drawRect(0.0f, horizon, width, height - horizon,
			0.27f, 0.18f, 0.11f, 1.0f);
		drawRect(0.0f, horizon, width, 12.0f * uiScale,
			0.30f, 0.52f, 0.20f, 1.0f);

		const float block = 32.0f * uiScale;
		const float playerX = width * 0.5f + m_gamePlayerX * uiScale;
		const float playerZ = horizon - 22.0f * uiScale + m_gamePlayerZ * uiScale;
		for (int i = -12; i <= 12; ++i)
		{
			const float x = width * 0.5f + static_cast<float>(i) * block -
				std::fmod(m_gamePlayerX * uiScale, block);
			drawRect(x, horizon - 4.0f * uiScale, block - 2.0f * uiScale,
				4.0f * uiScale, 0.36f, 0.62f, 0.24f, 1.0f);
		}
		for (int row = 0; row < 4; ++row)
		{
			const float y = horizon + static_cast<float>(row) * block;
			for (int i = -12; i <= 12; ++i)
			{
				const float x = width * 0.5f + static_cast<float>(i) * block -
					std::fmod(m_gamePlayerX * uiScale, block);
				drawRect(x, y, block - 2.0f * uiScale, block - 2.0f * uiScale,
					row % 2 == 0 ? 0.25f : 0.22f,
					row % 2 == 0 ? 0.16f : 0.14f,
					row % 2 == 0 ? 0.10f : 0.09f, 1.0f);
			}
		}

		drawRect(playerX - 10.0f * uiScale, playerZ - 26.0f * uiScale,
			20.0f * uiScale, 26.0f * uiScale, 0.15f, 0.34f, 0.72f, 1.0f);
		drawRect(playerX - 8.0f * uiScale, playerZ - 40.0f * uiScale,
			16.0f * uiScale, 14.0f * uiScale, 0.86f, 0.63f, 0.40f, 1.0f);
	}

#if !defined(MCPE_UWP_RELEASE)
	const float fontScale = uiScale * 1.35f;
	const std::wstring tickText = L"Tick " + std::to_wstring(m_gameTick);
	drawText(L"MCPE 0.6.1 CORE", 12.0f * uiScale, 10.0f * uiScale,
		fontScale, 1.0f, 1.0f, 1.0f, 1.0f);
	drawText(m_gameWorldName, 12.0f * uiScale, 30.0f * uiScale,
		fontScale, 1.0f, 0.90f, 0.68f, 1.0f);
	if (m_originalFont == nullptr)
		drawText(tickText, 12.0f * uiScale, 50.0f * uiScale,
			fontScale, 0.92f, 0.96f, 1.0f, 1.0f);
#endif

	// World exit is now owned by the original Minecraft PauseScreen (Esc).
	// Do not draw the former UWP-only lower-left Back button here; keeping it
	// would create a second pause/exit owner over the original client frame.
#if !defined(MCPE_UWP_RELEASE)
	if (!m_statusText.empty())
		drawCenteredText(m_statusText, width * 0.5f,
			m_statusDetailText.empty() ? height - 58.0f * uiScale : height - 78.0f * uiScale,
			uiScale * 1.15f, 1.0f, 0.84f, 0.74f, 1.0f);
	if (!m_statusDetailText.empty() && m_originalFont == nullptr)
		drawCenteredText(m_statusDetailText, width * 0.5f, height - 58.0f * uiScale,
			uiScale * 1.0f, 0.92f, 0.82f, 0.68f, 1.0f);
#endif
}

int UwpUiRenderer::hitTest(float width, float height) const
{
	if (m_pointerX < 0.0f || m_pointerY < 0.0f)
		return -1;

	if (m_screen == Screen::WorldSelect)
	{
		const float uiScale = height / 480.0f;
		if (!m_worldNames.empty())
		{
			const float cardWidth = 150.0f * uiScale;
			const float cardGap = 12.0f * uiScale;
			const float totalWidth = cardWidth * static_cast<float>(m_worldNames.size())
				+ cardGap * static_cast<float>(m_worldNames.size() - 1);
			const float firstX = (width - totalWidth) * 0.5f;
			const float cardY = 72.0f * uiScale;
			const float cardHeight = 102.0f * uiScale;
			for (unsigned int i = 0; i < m_worldNames.size(); ++i)
			{
				const float x = firstX + i * (cardWidth + cardGap);
				if (m_pointerX >= x && m_pointerX <= x + cardWidth &&
					m_pointerY >= cardY && m_pointerY <= cardY + cardHeight)
					return TargetWorldFirst + static_cast<int>(i);
			}
		}

		const float buttonWidth = 84.0f * uiScale;
		const float buttonHeight = 26.0f * uiScale;
		const float buttonGap = 8.0f * uiScale;
		const float buttonY = height - 28.0f * uiScale;
		const float buttonStart = width * 0.5f - (buttonWidth * 2.0f + buttonGap * 1.5f);
		if (m_pointerY >= buttonY && m_pointerY <= buttonY + buttonHeight)
		{
			if (m_pointerX >= buttonStart && m_pointerX <= buttonStart + buttonWidth)
				return TargetWorldDelete;
			if (m_pointerX >= buttonStart + buttonWidth + buttonGap &&
				m_pointerX <= buttonStart + buttonWidth * 2.0f + buttonGap)
				return TargetWorldImport;
			if (m_pointerX >= buttonStart + (buttonWidth + buttonGap) * 2.0f &&
				m_pointerX <= buttonStart + buttonWidth * 3.0f + buttonGap * 2.0f)
				return TargetWorldCreate;
			if (m_pointerX >= buttonStart + (buttonWidth + buttonGap) * 3.0f &&
				m_pointerX <= buttonStart + (buttonWidth + buttonGap) * 3.0f + buttonWidth)
				return TargetWorldBack;
		}
		return TargetNone;
	}
	if (m_screen == Screen::Game)
	{
		// The original Minecraft client handles Esc through PauseScreen.  The
		// old UWP Game Back hit target is intentionally no longer emitted.
		return TargetNone;
	}

	const float panelWidth = width * 0.64f;
	const float panelX = (width - panelWidth) * 0.5f;
	const float buttonWidth = panelWidth * 0.62f;
	const float buttonX = (width - buttonWidth) * 0.5f;
	const float buttonHeight = height * (34.0f / 480.0f);
	const float firstY = height * (214.0f / 480.0f);
	const float gap = height * (10.0f / 480.0f);
	(void)panelX;
	for (int i = 0; i < 4; ++i)
	{
		const float y = firstY + i * (buttonHeight + gap);
		if (m_pointerX >= buttonX && m_pointerX <= buttonX + buttonWidth &&
			m_pointerY >= y && m_pointerY <= y + buttonHeight)
			return i;
	}
	return TargetNone;
}

void UwpUiRenderer::render(float width, float height)
{
	if (!m_initialized || width <= 0.0f || height <= 0.0f)
		return;
	m_uiScale = height / 480.0f;

	// The original client initialization and renderer use the same ANGLE
	// context.  Re-establish the small GLES2 state contract required by this
	// UI pass instead of relying on whichever legacy path ran immediately
	// before it.  In particular, a stale scissor/color mask or an array-buffer
	// binding makes the CPU-side UI vertices silently disappear.
	glViewport(0, 0, static_cast<GLsizei>(width), static_cast<GLsizei>(height));
	glDisable(GL_SCISSOR_TEST);
	glDisable(GL_STENCIL_TEST);
	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	glActiveTexture(GL_TEXTURE0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	glUseProgram(m_program);
	glUniform2f(m_viewport, width, height);
	glUniform1i(m_texture, 0);
	glUniform1f(m_useTexture, 0.0f);
	// The world pass may have a populated depth buffer.  The 2D HUD must be
	// composited on top of it regardless of terrain depth.
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	if (!(m_screen == Screen::Game && m_realWorldRendering) && m_backgroundTexture)
		drawTiledBackground(width, height);
	else if (!(m_screen == Screen::Game && m_realWorldRendering))
		drawGradient(0.0f, 0.0f, width, height,
			0.08f, 0.12f, 0.20f,
			0.30f, 0.19f, 0.14f);

	if (m_screen == Screen::MainMenu)
		drawMainMenu(width, height);
	else if (m_screen == Screen::WorldSelect)
		drawWorldSelect(width, height);
	else
		drawGame(width, height);

	glDisable(GL_BLEND);
	glUseProgram(0);
#if !defined(MCPE_UWP_RELEASE)
	if (m_screen == Screen::Game && m_originalFont != nullptr &&
		!m_statusDetailText.empty())
		drawOriginalFontStatus(width, height);
	if (m_screen == Screen::Game && m_originalFont != nullptr)
		drawOriginalFontTick(width, height);
#endif
}

void UwpUiRenderer::renderLoadingBackground(float width, float height)
{
	if (!m_initialized || width <= 0.0f || height <= 0.0f)
		return;

	m_uiScale = height / 480.0f;
	glViewport(0, 0, static_cast<GLsizei>(width), static_cast<GLsizei>(height));
	glDisable(GL_SCISSOR_TEST);
	glDisable(GL_STENCIL_TEST);
	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	glActiveTexture(GL_TEXTURE0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	glUseProgram(m_program);
	glUniform2f(m_viewport, width, height);
	glUniform1i(m_texture, 0);
	glUniform1f(m_useTexture, 0.0f);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	if (m_backgroundTexture)
		drawTiledBackground(width, height);
	else
		drawGradient(0.0f, 0.0f, width, height,
			0.10f, 0.07f, 0.05f,
			0.20f, 0.12f, 0.08f);
	glDisable(GL_BLEND);
	glUseProgram(0);
}

void UwpUiRenderer::renderStartupLoading(float width, float height)
{
	if (!m_initialized || width <= 0.0f || height <= 0.0f)
		return;

	m_uiScale = height / 480.0f;
	glViewport(0, 0, static_cast<GLsizei>(width), static_cast<GLsizei>(height));
	glDisable(GL_SCISSOR_TEST);
	glDisable(GL_STENCIL_TEST);
	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	glActiveTexture(GL_TEXTURE0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	glUseProgram(m_program);
	glUniform2f(m_viewport, width, height);
	glUniform1i(m_texture, 0);
	glUniform1f(m_useTexture, 0.0f);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	// The iOS Mojang asset has transparent margins around the logo.  A white
	// clear is intentional: it matches the source launch artwork and avoids
	// exposing the ANGLE clear colour while the original client is booting.
	drawRect(0.0f, 0.0f, width, height, 1.0f, 1.0f, 1.0f, 1.0f);
	if (m_mojangLogoTexture)
	{
		const float requestedLogoWidth = width * 0.68f;
		// The native cold-start splash uses the logo canvas at roughly 280
		// logical pixels wide.  The source PNG is a 900x600 canvas with large
		// transparent margins, so scaling the whole canvas rather than only the
		// visible wordmark is important for matching that splash geometry.
		const float coldSplashLogoWidth = 280.0f * m_uiScale;
		const float logoWidth = requestedLogoWidth < coldSplashLogoWidth
			? requestedLogoWidth : coldSplashLogoWidth;
		const float logoHeight = logoWidth *
			(static_cast<float>(m_mojangLogoHeight) /
				static_cast<float>(m_mojangLogoWidth));
		drawTexturedRect(m_mojangLogoTexture, m_mojangLogoWidth,
			m_mojangLogoHeight, (width - logoWidth) * 0.5f,
			height * 0.30f, logoWidth, logoHeight, 0.0f, 0.0f,
			static_cast<float>(m_mojangLogoWidth),
			static_cast<float>(m_mojangLogoHeight));
	}

	// Use the same MCPE bitmap/TTF selection as the original menu.  The font
	// texture is loaded asynchronously, so the first frame may contain only
	// the logo; subsequent frames draw the stable loading caption.
	drawCenteredText(L"Loading...", width * 0.5f, height * 0.64f,
		m_uiScale * 2.0f, 0.20f, 0.20f, 0.20f, 1.0f);
	glDisable(GL_BLEND);
	glUseProgram(0);
}

void UwpUiRenderer::renderSavingWorld(float width, float height)
{
	if (!m_initialized || width <= 0.0f || height <= 0.0f)
		return;

	m_uiScale = height / 480.0f;
	glViewport(0, 0, static_cast<GLsizei>(width), static_cast<GLsizei>(height));
	glDisable(GL_SCISSOR_TEST);
	glDisable(GL_STENCIL_TEST);
	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	glActiveTexture(GL_TEXTURE0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	glUseProgram(m_program);
	glUniform2f(m_viewport, width, height);
	glUniform1i(m_texture, 0);
	glUniform1f(m_useTexture, 0.0f);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	if (m_backgroundTexture)
		drawTiledBackground(width, height);
	else
		drawGradient(0.0f, 0.0f, width, height,
			0.10f, 0.07f, 0.05f, 0.20f, 0.12f, 0.08f);
	drawCenteredText(L"Saving world...", width * 0.5f,
		height * 0.48f, m_uiScale * 2.5f,
		1.0f, 1.0f, 1.0f, 1.0f);
	glDisable(GL_BLEND);
	glUseProgram(0);
}

void UwpUiRenderer::setScreen(Screen screen)
{
	// MainPage no longer routes either legacy UWP menu to this renderer.  Keep
	// the old enum values source-compatible for callers in older branches, but
	// refuse to make them visible so there is one menu/world-list owner.
	(void)screen;
	m_screen = Screen::Game;
}

void UwpUiRenderer::setFontProfile(FontProfile profile)
{
	m_fontProfile = profile;
}

void UwpUiRenderer::setSelectedWorld(int index)
{
	if (index >= 0 && index < static_cast<int>(m_worldNames.size()))
		m_selectedWorld = index;
}

void UwpUiRenderer::setWorldNames(const std::vector<std::wstring>& names)
{
	m_worldNames = names;
	m_selectedWorld = names.empty() ? -1 : 0;
}

void UwpUiRenderer::setStatusText(const std::wstring& text)
{
	m_statusText = text;
}

void UwpUiRenderer::setStatusDetailText(const std::wstring& text)
{
	m_statusDetailText = text;
}

void UwpUiRenderer::setGameTelemetry(const std::wstring& worldName, std::uint64_t tick,
	float playerX, float playerZ, float dayTime)
{
	m_gameWorldName = worldName;
	m_gameTick = tick;
	m_gamePlayerX = playerX;
	m_gamePlayerZ = playerZ;
	m_gameDayTime = dayTime;
}

void UwpUiRenderer::setPointer(float x, float y, bool pressed)
{
	m_pointerX = x;
	m_pointerY = y;
	m_pointerPressed = pressed;
}

bool UwpUiRenderer::setFontFilePath(const std::wstring& path)
{
	return buildTtfAtlas(path);
}

bool UwpUiRenderer::buildTtfAtlas(const std::wstring& path)
{
	using Microsoft::WRL::ComPtr;

	m_ttfReady = false;
	for (TtfGlyph& glyph : m_ttfGlyphs)
		glyph = TtfGlyph{};

	if (!m_initialized || path.empty())
		return false;

	ComPtr<IDWriteFactory> factory;
	HRESULT hr = DWriteCreateFactory(
		DWRITE_FACTORY_TYPE_SHARED,
		__uuidof(IDWriteFactory),
		reinterpret_cast<IUnknown**>(factory.GetAddressOf()));
	if (FAILED(hr))
		return false;

	ComPtr<IDWriteFontFile> fontFile;
	hr = factory->CreateFontFileReference(path.c_str(), nullptr, &fontFile);
	if (FAILED(hr))
		return false;

	BOOL supported = FALSE;
	DWRITE_FONT_FILE_TYPE fileType = DWRITE_FONT_FILE_TYPE_UNKNOWN;
	DWRITE_FONT_FACE_TYPE faceType = DWRITE_FONT_FACE_TYPE_UNKNOWN;
	UINT32 faceCount = 0;
	hr = fontFile->Analyze(&supported, &fileType, &faceType, &faceCount);
	if (FAILED(hr) || !supported || faceCount == 0)
		return false;

	IDWriteFontFile* fontFiles[] = { fontFile.Get() };
	ComPtr<IDWriteFontFace> fontFace;
	hr = factory->CreateFontFace(
		faceType, 1, fontFiles, 0, DWRITE_FONT_SIMULATIONS_NONE, &fontFace);
	if (FAILED(hr))
		return false;

	DWRITE_FONT_METRICS metrics{};
	fontFace->GetMetrics(&metrics);
	if (metrics.designUnitsPerEm == 0)
		return false;

	// The original MCPE GUI uses an 8-pixel logical glyph cell.  Rasterize
	// the TTF at that same base size, then let the existing uiScale enlarge it.
	const float fontSize = m_ttfBaseSize;
	m_ttfAscent = fontSize * static_cast<float>(metrics.ascent) /
		static_cast<float>(metrics.designUnitsPerEm);
	const LONG baseline = static_cast<LONG>(std::ceil(m_ttfAscent)) + 4;
	const unsigned int cellSize = 64;
	const unsigned int atlasWidth = cellSize * 16;
	const unsigned int atlasHeight = cellSize * 16;
	const unsigned int padding = 8;
	std::vector<unsigned char> pixels(atlasWidth * atlasHeight * 4u, 0u);

	for (unsigned int code = 0; code < 256u; ++code)
	{
		TtfGlyph glyph{};
		const UINT32 character = static_cast<UINT32>(code);
		UINT16 glyphIndex = 0;
		if (FAILED(fontFace->GetGlyphIndices(&character, 1, &glyphIndex)))
			continue;

		DWRITE_GLYPH_METRICS glyphMetrics{};
		if (FAILED(fontFace->GetDesignGlyphMetrics(&glyphIndex, 1, &glyphMetrics, FALSE)))
			continue;
		glyph.advance = fontSize * static_cast<float>(glyphMetrics.advanceWidth) /
			static_cast<float>(metrics.designUnitsPerEm);

		FLOAT glyphAdvance = glyph.advance;
		DWRITE_GLYPH_RUN run{};
		run.fontFace = fontFace.Get();
		run.fontEmSize = fontSize;
		run.glyphCount = 1;
		run.glyphIndices = &glyphIndex;
		run.glyphAdvances = &glyphAdvance;
		run.glyphOffsets = nullptr;
		run.isSideways = FALSE;
		run.bidiLevel = 0;

		ComPtr<IDWriteGlyphRunAnalysis> analysis;
		hr = factory->CreateGlyphRunAnalysis(
			&run, 1.0f, nullptr,
			DWRITE_RENDERING_MODE_ALIASED,
			DWRITE_MEASURING_MODE_NATURAL,
			0.0f, static_cast<FLOAT>(baseline), &analysis);
		if (FAILED(hr))
		{
			m_ttfGlyphs[code] = glyph;
			continue;
		}

		RECT bounds{};
		const DWRITE_TEXTURE_TYPE alphaTextureType = static_cast<DWRITE_TEXTURE_TYPE>(0);
		hr = analysis->GetAlphaTextureBounds(alphaTextureType, &bounds);
		if (FAILED(hr) || bounds.right <= bounds.left || bounds.bottom <= bounds.top)
		{
			// Spaces and other blank glyphs still retain their measured advance.
			m_ttfGlyphs[code] = glyph;
			continue;
		}

		const unsigned int bitmapWidth = static_cast<unsigned int>(bounds.right - bounds.left);
		const unsigned int bitmapHeight = static_cast<unsigned int>(bounds.bottom - bounds.top);
		const unsigned int cellX = (code % 16u) * cellSize;
		const unsigned int cellY = (code / 16u) * cellSize;
		if (bitmapWidth + padding * 2u > cellSize || bitmapHeight + padding * 2u > cellSize)
		{
			m_ttfGlyphs[code] = glyph;
			continue;
		}

		std::vector<unsigned char> alpha(bitmapWidth * bitmapHeight);
		hr = analysis->CreateAlphaTexture(
			alphaTextureType, &bounds,
			alpha.data(), static_cast<UINT32>(alpha.size()));
		if (FAILED(hr))
		{
			m_ttfGlyphs[code] = glyph;
			continue;
		}

		const unsigned int destinationX = cellX + padding;
		const unsigned int destinationY = cellY + padding;
		for (unsigned int row = 0; row < bitmapHeight; ++row)
		{
			for (unsigned int column = 0; column < bitmapWidth; ++column)
			{
				const unsigned int destination =
					((destinationY + row) * atlasWidth + destinationX + column) * 4u;
				pixels[destination + 0u] = 255u;
				pixels[destination + 1u] = 255u;
				pixels[destination + 2u] = 255u;
				pixels[destination + 3u] = alpha[row * bitmapWidth + column];
			}
		}

		glyph.sourceX = static_cast<float>(destinationX);
		glyph.sourceY = static_cast<float>(destinationY);
		glyph.sourceWidth = static_cast<float>(bitmapWidth);
		glyph.sourceHeight = static_cast<float>(bitmapHeight);
		glyph.bearingX = static_cast<float>(bounds.left);
		glyph.bearingY = static_cast<float>(bounds.top - baseline);
		glyph.drawable = true;
		m_ttfGlyphs[code] = glyph;
	}

	if (m_ttfTexture)
		glDeleteTextures(1, &m_ttfTexture);
	m_ttfTexture = 0;
	m_ttfWidth = 0;
	m_ttfHeight = 0;
	uploadTexture(m_ttfTexture, m_ttfWidth, m_ttfHeight,
		m_ttfRepeat, atlasWidth, atlasHeight,
		pixels.data(), static_cast<unsigned int>(pixels.size()), false);
	if (m_ttfTexture)
	{
		// The TTF atlas is rendered above logical resolution and reduced when
		// sampled.  Linear filtering keeps thin strokes from disappearing.
		glBindTexture(GL_TEXTURE_2D, m_ttfTexture);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}
	m_ttfReady = m_ttfTexture != 0;
	return m_ttfReady;
}

void UwpUiRenderer::uploadTexture(GLuint& texture, unsigned int& textureWidth, unsigned int& textureHeight,
	bool& repeat, unsigned int width, unsigned int height,
	const unsigned char* pixels, unsigned int byteCount, bool shouldRepeat)
{
	if (!m_initialized || !pixels || byteCount < width * height * 4u || width == 0 || height == 0)
		return;
	if (!texture)
		glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	// MCPE's GUI assets are pixel-art textures.  Linear filtering makes the
	// 16x16 dirt tile look like a fine, blurry grain when it is enlarged and
	// also softens the title/button atlas.  Keep source pixels crisp, matching
	// the original renderer's nearest-neighbour sampling.
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, shouldRepeat ? GL_REPEAT : GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, shouldRepeat ? GL_REPEAT : GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
	textureWidth = width;
	textureHeight = height;
	repeat = shouldRepeat;
}

void UwpUiRenderer::setTexturePixels(TextureSlot slot, unsigned int width, unsigned int height,
	const unsigned char* pixels, unsigned int byteCount)
{
	switch (slot)
	{
	case TextureSlot::Background:
		uploadTexture(m_backgroundTexture, m_backgroundWidth, m_backgroundHeight,
			m_backgroundRepeat, width, height, pixels, byteCount, true);
		break;
	case TextureSlot::Title:
		uploadTexture(m_titleTexture, m_titleWidth, m_titleHeight,
			m_backgroundRepeat, width, height, pixels, byteCount, false);
		break;
	case TextureSlot::Buttons:
		uploadTexture(m_buttonsTexture, m_buttonsWidth, m_buttonsHeight,
			m_backgroundRepeat, width, height, pixels, byteCount, false);
		break;
	case TextureSlot::Font:
		uploadTexture(m_fontTexture, m_fontWidth, m_fontHeight,
			m_backgroundRepeat, width, height, pixels, byteCount, false);
		if (width >= 16 && height >= 16 && byteCount >= width * height * 4u)
		{
			const unsigned int cellWidth = width / 16u;
			const unsigned int cellHeight = height / 16u;
			for (unsigned int code = 0; code < 256u; ++code)
			{
				const unsigned int originX = (code % 16u) * cellWidth;
				const unsigned int originY = (code / 16u) * cellHeight;
				int rightmost = -1;
				for (unsigned int y = 0; y < cellHeight; ++y)
				{
					for (unsigned int x = 0; x < cellWidth; ++x)
					{
						const unsigned int pixel = ((originY + y) * width + originX + x) * 4u;
						if (pixels[pixel + 3u] > 0u)
							rightmost = static_cast<int>(x);
					}
				}
				m_fontVisibleWidth[code] = rightmost >= 0
					? static_cast<unsigned char>(rightmost + 1)
					: 0;
				// This is the same metric extraction used by the original
				// Font::init: spaces are four logical pixels, all other glyphs
				// use the rightmost visible pixel plus two.
				const int advance = code == 32u ? 4 : rightmost + 2;
				// The original Font implementation keeps rightmost + 2 verbatim;
				// do not clamp a full-width glyph back to the cell width.
				m_fontAdvance[code] = static_cast<unsigned char>(advance);
			}
		}
		break;
	case TextureSlot::DefaultWorld:
		uploadTexture(m_defaultWorldTexture, m_defaultWorldWidth, m_defaultWorldHeight,
			m_backgroundRepeat, width, height, pixels, byteCount, false);
		break;
	case TextureSlot::MojangLogo:
		uploadTexture(m_mojangLogoTexture, m_mojangLogoWidth, m_mojangLogoHeight,
			m_mojangLogoRepeat, width, height, pixels, byteCount, false);
		break;
	}
}

void UwpUiRenderer::shutdown()
{
	if (m_program)
		glDeleteProgram(m_program);
	if (m_backgroundTexture) glDeleteTextures(1, &m_backgroundTexture);
	if (m_titleTexture) glDeleteTextures(1, &m_titleTexture);
	if (m_buttonsTexture) glDeleteTextures(1, &m_buttonsTexture);
	if (m_fontTexture) glDeleteTextures(1, &m_fontTexture);
	if (m_ttfTexture) glDeleteTextures(1, &m_ttfTexture);
	if (m_defaultWorldTexture) glDeleteTextures(1, &m_defaultWorldTexture);
	if (m_mojangLogoTexture) glDeleteTextures(1, &m_mojangLogoTexture);
	m_program = 0;
	m_backgroundTexture = 0;
	m_titleTexture = 0;
	m_buttonsTexture = 0;
	m_fontTexture = 0;
	m_ttfTexture = 0;
	m_defaultWorldTexture = 0;
	m_mojangLogoTexture = 0;
	m_position = -1;
	m_color = -1;
	m_texCoord = -1;
	m_viewport = -1;
	m_texture = -1;
	m_useTexture = -1;
	m_backgroundWidth = 0;
	m_backgroundHeight = 0;
	m_titleWidth = 0;
	m_titleHeight = 0;
	m_buttonsWidth = 0;
	m_buttonsHeight = 0;
	m_fontWidth = 0;
	m_fontHeight = 0;
	m_ttfWidth = 0;
	m_ttfHeight = 0;
	m_defaultWorldWidth = 0;
	m_defaultWorldHeight = 0;
	m_mojangLogoWidth = 0;
	m_mojangLogoHeight = 0;
	m_initialized = false;
	m_screen = Screen::Game;
	m_worldNames.clear();
	m_selectedWorld = -1;
	m_statusText.clear();
	m_statusDetailText.clear();
	m_fontAdvance.fill(0);
	m_fontVisibleWidth.fill(0);
	for (TtfGlyph& glyph : m_ttfGlyphs)
		glyph = TtfGlyph{};
	m_ttfReady = false;
	m_ttfAscent = 0.0f;
	m_fontProfile = FontProfile::Mcpe061;
	m_uiScale = 1.0f;
	m_mojangLogoRepeat = false;
	m_originalFont = nullptr;
	m_originalFontVboInitialized = false;
	m_pointerPressed = false;
}

// Keep the original renderer compatibility headers local to this one Stage 6
// seam.  The rest of this file intentionally uses the UWP UI shader directly.
#include "../../client/gui/Font.h"
#include "../../client/renderer/Tesselator.h"
#include "../../client/renderer/gles.h"

namespace
{
std::string toOriginalFontAscii(const std::wstring& text)
{
	std::string result;
	result.reserve(text.size());
	for (wchar_t ch : text)
		result.push_back(ch >= 0 && ch <= 0x7f ? static_cast<char>(ch) : '?');
	return result;
}
}

void UwpUiRenderer::drawOriginalFontStatus(float width, float height)
{
	if (!m_originalFont || width <= 0.0f || height <= 0.0f ||
		g_shader_program == 0)
		return;

	const std::string text = toOriginalFontAscii(m_statusDetailText);
	if (text.empty())
		return;

	if (!m_originalFontVboInitialized)
	{
		Tesselator::instance.init();
		m_originalFontVboInitialized = true;
	}

	const float scale = m_uiScale > 0.0f ? m_uiScale : 1.0f;
	const float logicalWidth = width / scale;
	const float logicalHeight = height / scale;
	const float textWidth = static_cast<float>(m_originalFont->width(text));
	float x = (logicalWidth - textWidth) * 0.5f;
	if (x < 4.0f)
		x = 4.0f;
	const float y = logicalHeight - 58.0f;

	shaderBind();
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc2(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glActiveTexture(GL_TEXTURE0);
	glEnable(GL_TEXTURE_2D);
	glEnable(GL_ALPHA_TEST);
	glAlphaFunc2(GL_GREATER, 0.1f);

	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	glOrthof(0.0f, logicalWidth, logicalHeight, 0.0f, -1.0f, 1.0f);
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();
	glScalef2(scale, scale, 1.0f);

	// This replaces only the game status-detail line.  Buttons, menu text,
	// terrain, and the rest of the HUD remain on their existing UWP paths.
	m_originalFont->draw(text, x, y, 0xffebd1ad);

	glBindBuffer2(GL_ARRAY_BUFFER, 0);
	glPopMatrix();
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glDisable(GL_ALPHA_TEST);
	glDisable(GL_TEXTURE_2D);
	glDisable(GL_BLEND);
	glEnable(GL_DEPTH_TEST);
	shaderUnbind();
}

void UwpUiRenderer::drawOriginalFontTick(float width, float height)
{
	if (!m_originalFont || width <= 0.0f || height <= 0.0f ||
		g_shader_program == 0)
		return;

	const std::string text = toOriginalFontAscii(
		L"Tick " + std::to_wstring(m_gameTick));
	if (text.empty())
		return;

	if (!m_originalFontVboInitialized)
	{
		Tesselator::instance.init();
		m_originalFontVboInitialized = true;
	}

	const float scale = m_uiScale > 0.0f ? m_uiScale : 1.0f;
	const float logicalWidth = width / scale;
	const float logicalHeight = height / scale;

	shaderBind();
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc2(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glActiveTexture(GL_TEXTURE0);
	glEnable(GL_TEXTURE_2D);
	glEnable(GL_ALPHA_TEST);
	glAlphaFunc2(GL_GREATER, 0.1f);

	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	glOrthof(0.0f, logicalWidth, logicalHeight, 0.0f, -1.0f, 1.0f);
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();
	glScalef2(scale, scale, 1.0f);

	// The world name and title remain on UwpUiRenderer.  This seam replaces
	// only the live tick counter with the original bitmap Font path.
	m_originalFont->draw(text, 12.0f, 50.0f, 0xffebd1ad);

	glBindBuffer2(GL_ARRAY_BUFFER, 0);
	glPopMatrix();
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glDisable(GL_ALPHA_TEST);
	glDisable(GL_TEXTURE_2D);
	glDisable(GL_BLEND);
	glEnable(GL_DEPTH_TEST);
	shaderUnbind();
}
