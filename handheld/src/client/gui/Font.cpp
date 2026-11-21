#include "Font.h"

//#include "SharedConstants.h"
#include "../Options.h"
#include "../renderer/Textures.h"
#include "../renderer/Tesselator.h"
#include "../../util/Mth.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <map>

namespace
{
	unsigned int nextCodepoint(const std::string& text, size_t& position)
	{
		const unsigned char first = static_cast<unsigned char>(text[position++]);
		if (first < 0x80)
			return first;

		int continuationCount = 0;
		unsigned int codepoint = 0;
		if (first >= 0xc2 && first <= 0xdf)
		{
			continuationCount = 1;
			codepoint = first & 0x1f;
		}
		else if (first >= 0xe0 && first <= 0xef)
		{
			continuationCount = 2;
			codepoint = first & 0x0f;
		}
		else if (first >= 0xf0 && first <= 0xf4)
		{
			continuationCount = 3;
			codepoint = first & 0x07;
		}
		else
			return 0xfffd;

		const size_t start = position;
		for (int i = 0; i < continuationCount; ++i)
		{
			if (position >= text.length())
			{
				position = start;
				return 0xfffd;
			}
			const unsigned char next = static_cast<unsigned char>(text[position]);
			if ((next & 0xc0) != 0x80)
			{
				position = start;
				return 0xfffd;
			}
			++position;
			codepoint = (codepoint << 6) | (next & 0x3f);
		}

		if ((continuationCount == 2 && codepoint < 0x800) ||
			(continuationCount == 3 && codepoint < 0x10000) ||
			(codepoint >= 0xd800 && codepoint <= 0xdfff) || codepoint > 0x10ffff)
		{
			position = start;
			return 0xfffd;
		}
		return codepoint;
	}

	size_t colorSequenceLength(const std::string& text, size_t position)
	{
		size_t markerLength = 0;
		if (position + 1 < text.length() &&
			static_cast<unsigned char>(text[position]) == 0xa7)
			markerLength = 1;
		else if (position + 2 < text.length() &&
			static_cast<unsigned char>(text[position]) == 0xc2 &&
			static_cast<unsigned char>(text[position + 1]) == 0xa7)
			markerLength = 2;

		if (!markerLength)
			return 0;

		const unsigned char code = static_cast<unsigned char>(text[position + markerLength]);
		return std::isxdigit(code) ? markerLength + 1 : 0;
	}

	bool isAsciiPunctuation(unsigned int codepoint)
	{
		return (codepoint >= 0x21 && codepoint <= 0x2f) ||
			(codepoint >= 0x3a && codepoint <= 0x40) ||
			(codepoint >= 0x5b && codepoint <= 0x60) ||
			(codepoint >= 0x7b && codepoint <= 0x7e);
	}

	bool isContextSeparator(unsigned int codepoint)
	{
		return codepoint == ' ' || codepoint == '\t' || isAsciiPunctuation(codepoint);
	}

	bool isCjkText(unsigned int codepoint)
	{
		return (codepoint >= 0x2e80 && codepoint <= 0x2eff) ||
			(codepoint >= 0x2f00 && codepoint <= 0x2fdf) ||
			(codepoint >= 0x3000 && codepoint <= 0x30ff) ||
			(codepoint >= 0x3100 && codepoint <= 0x312f) ||
			(codepoint >= 0x31a0 && codepoint <= 0x31bf) ||
			(codepoint >= 0x3400 && codepoint <= 0x4dbf) ||
			(codepoint >= 0x4e00 && codepoint <= 0x9fff) ||
			(codepoint >= 0xac00 && codepoint <= 0xd7af) ||
			(codepoint >= 0xf900 && codepoint <= 0xfaff);
	}

	bool usesMojangFont(unsigned int codepoint)
	{
		if (isAsciiPunctuation(codepoint))
			return true; // Context decides which font an ASCII mark uses.
		if (codepoint <= 0x20 || (codepoint >= 0x80 && codepoint <= 0xff))
		{
			if (codepoint == 0x00a1 || codepoint == 0x00a7 ||
				codepoint == 0x00ab || (codepoint >= 0x00b6 && codepoint <= 0x00b7) ||
				codepoint == 0x00bb || codepoint == 0x00bf)
				return false;
			return true;
		}
		if ((codepoint >= '0' && codepoint <= '9') ||
			(codepoint >= 'A' && codepoint <= 'Z') ||
			(codepoint >= 'a' && codepoint <= 'z'))
			return true;
		return false;
	}

	unsigned int mojangGlyphIndex(unsigned int codepoint)
	{
		// The Mojang atlas keeps its larger, separated copyright glyph at the
		// legacy 0xFF slot. Route valid UTF-8 U+00A9 text to that same cell.
		return codepoint == 0x00a9 ? 0xff : codepoint;
	}

	bool asciiPunctuationUsesUnifont(const std::string& text, size_t end,
		unsigned int previousTextCodepoint)
	{
		if (isCjkText(previousTextCodepoint))
			return true;

		// Ignore whitespace and adjacent ASCII punctuation when inspecting the
		// following text, so marks separated by spaces in localized UI strings
		// still inherit the surrounding CJK font.
		while (end < text.length())
		{
			size_t nextPosition = end;
			const unsigned int next = nextCodepoint(text, nextPosition);
			if (!isContextSeparator(next))
				return isCjkText(next);
			end = nextPosition;
		}
		return false;
	}

	std::string unifontPageName(unsigned int codepoint)
	{
		char page[8] = {};
		sprintf_s(page, sizeof(page), "%02X", codepoint >> 8);
		return std::string("font/unifont/") + page + ".png";
	}

	void setFontVertexColor(Tesselator& tesselator, int color, bool darken,
		bool anaglyph)
	{
		int alpha = (color >> 24) & 0xff;
		if (alpha == 0)
			alpha = 0xff;
		int r = (color >> 16) & 0xff;
		int g = (color >> 8) & 0xff;
		int b = color & 0xff;
		if (anaglyph)
		{
			const int cr = (r * 30 + g * 59 + b * 11) / 100;
			const int cg = (r * 30 + g * 70) / 100;
			const int cb = (r * 30 + b * 70) / 100;
			r = cr;
			g = cg;
			b = cb;
		}
		if (darken)
		{
			r /= 4;
			g /= 4;
			b /= 4;
		}
		tesselator.color(r, g, b, alpha);
	}

	int formatCodeColor(char code, bool darken, bool anaglyph, int alpha)
	{
		static const char hex[] = "0123456789abcdef";
		const char* found = std::strchr(hex, static_cast<char>(std::tolower(
			static_cast<unsigned char>(code))));
		const int value = found ? static_cast<int>(found - hex) : 15;
		const int br = ((value >> 3) & 1) * 0x55;
		int r = ((value >> 2) & 1) * 0xaa + br;
		int g = ((value >> 1) & 1) * 0xaa + br;
		int b = (value & 1) * 0xaa + br;
		if (value == 6)
			r += 0x55;
		if (anaglyph)
		{
			const int cr = (r * 30 + g * 59 + b * 11) / 100;
			const int cg = (r * 30 + g * 70) / 100;
			const int cb = (r * 30 + b * 70) / 100;
			r = cr;
			g = cg;
			b = cb;
		}
		if (darken)
		{
			r /= 4;
			g /= 4;
			b /= 4;
		}
		return (alpha << 24) | (r << 16) | (g << 8) | b;
	}

	struct GlyphBounds
	{
		int left;
		int top;
		int right;
		int bottom;
	};

	GlyphBounds findGlyphBounds(const TextureData* image, unsigned int slot)
	{
		GlyphBounds bounds = { 0, 0, 16, 16 };
		if (!image || !image->data || image->w < 256 || image->h < 256)
			return bounds;

		int left = 16;
		int top = 16;
		int right = -1;
		int bottom = -1;
		const int cellX = static_cast<int>(slot & 15) * 16;
		const int cellY = static_cast<int>(slot >> 4) * 16;
		for (int py = 0; py < 16; ++py)
		{
			for (int px = 0; px < 16; ++px)
			{
				const size_t pixel = (static_cast<size_t>(cellY + py) * image->w + cellX + px) * 4;
				if (image->data[pixel + 3] == 0)
					continue;
				left = (std::min)(left, px);
				top = (std::min)(top, py);
				right = (std::max)(right, px);
				bottom = (std::max)(bottom, py);
			}
		}

		if (right >= left && bottom >= top)
		{
			bounds.left = (std::max)(0, left - 1);
			bounds.top = (std::max)(0, top - 1);
			bounds.right = (std::min)(16, right + 2);
			bounds.bottom = (std::min)(16, bottom + 2);
		}
		else
			bounds.left = bounds.top = bounds.right = bounds.bottom = 0;
		return bounds;
	}

}

Font::Font( Options* options, const std::string& name, Textures* textures )
:	options(options),
	fontTexture(0),
	fontName(name),
	index(0),
	count(0),
	_textures(textures),
	_x(0), _y(0),
	_cols(16), _rows(16),
	_charOffset(0),
	lineHeight(DefaultLineHeight)
{
	init(options);
}


//Font::Font( Options* options, const std::string& name, Textures* textures, int imgW, int imgH, int x, int y, int cols, int rows, unsigned char charOffset )
//:	options(options),
//	fontTexture(0),
//	fontName(name),
//	index(0),
//	count(0),
//	_textures(textures),
//	_x(x), _y(y),
//	_cols(cols), _rows(rows),
//	_charOffset(charOffset)
//{
//	init(options);
//}

void Font::onGraphicsReset()
{
	brightenedUnifontPages.clear();
	init(options);
}

void Font::init( Options* options )
{
	TextureId fontTexture = _textures->loadTexture(fontName);
	const TextureData* tex = _textures->getTemporaryTextureData(fontTexture);

	if (!tex)
		return;

	unsigned char* rawPixels = tex->data;

	const int numChars = _rows * _cols;
	for (int i = 0; i < numChars; i++) {
		int xt = i % _cols;
		int yt = i / _cols;

		int x = 7;
		for (; x >= 0; x--) {
			int xPixel = _x + xt * 8 + x;
			bool emptyColumn = true;
			for (int y = 0; y < 8 && emptyColumn; y++) {
				int yPixel = _y + (yt * 8 + y) * tex->w;
				unsigned char pixelalpha = rawPixels[(xPixel + yPixel) << 2];
				if (pixelalpha > 0) emptyColumn = false;
			}
			if (!emptyColumn) {
				break;
			}
		}

		if (i == ' ') x = 4 - 2;
		charWidths[i] = x + 2;
		fcharWidths[i] = (float) charWidths[i];
	}

#ifdef USE_VBO
	return; // this <1
#endif

#ifndef USE_VBO
	listPos = glGenLists(256 + 32);

	Tesselator& t = Tesselator::instance;
	for (int i = 0; i < 256; i++) {
		glNewList(listPos + i, GL_COMPILE);
		// @attn @huge @note: This is some dangerous code right here / Aron, added ^1
		t.begin();
		buildChar(i);
		t.end(false, -1);

		glTranslatef2((GLfloat)charWidths[i], 0.0f, 0.0f);
		glEndList();
	}

	for (int i = 0; i < 32; i++) {
		int br = ((i >> 3) & 1) * 0x55;
		int r = ((i >> 2) & 1) * 0xaa + br;
		int g = ((i >> 1) & 1) * 0xaa + br;
		int b = ((i >> 0) & 1) * 0xaa + br;
		if (i == 6) {
			r += 0x55;
		}
		bool darken = i >= 16;

		if (options->anaglyph3d) {
			int cr = (r * 30 + g * 59 + b * 11) / 100;
			int cg = (r * 30 + g * 70) / (100);
			int cb = (r * 30 + b * 70) / (100);

			r = cr;
			g = cg;
			b = cb;
		}

		// color = r << 16 | g << 8 | b;
		if (darken) {
			r /= 4;
			g /= 4;
			b /= 4;
		}

		glNewList(listPos + 256 + i, GL_COMPILE);
		glColor3f(r / 255.0f, g / 255.0f, b / 255.0f);
		glEndList();
	}
#endif
}

void Font::drawShadow( const std::string& str, float x, float y, int color )
{
	draw(str, x + 1, y + 1, color, true);
	draw(str, x, y, color);
}
void Font::drawShadow( const char* str, float x, float y, int color )
{
	draw(str, x + 1, y + 1, color, true);
	draw(str, x, y, color);
}

void Font::draw( const std::string& str, float x, float y, int color )
{
	draw(str, x, y, color, false);
}

void Font::draw( const char* str, float x, float y, int color )
{
	draw(str, x, y, color, false);
}

void Font::draw( const char* str, float x, float y, int color, bool darken )
{
#ifdef USE_VBO
	drawSlow(str, x, y, color, darken);
#else
	// The legacy display-list path is used when VBOs are unavailable (ARM32
	// WGL). Keep the C-string overload equivalent to the std::string overload;
	// the old conditional body silently discarded all C-string text.
	if (str)
		draw(std::string(str), x, y, color, darken);
#endif
}

void Font::draw( const std::string& str, float x, float y, int color, bool darken )
{
#ifdef USE_VBO
	drawSlow(str, x, y, color, darken);
	return;
#endif

	if (str.empty()) return;

	if (darken) {
		int oldAlpha = color & 0xff000000;
		color = (color & 0xfcfcfc) >> 2;
		color += oldAlpha;
	}

	// The UWP GLES2 compatibility layer represents the original fixed-function
	// GL_TEXTURE_2D enable bit as the shader's u_useTexture uniform.  World HUD
	// callers already establish this state, but original menu screens enter
	// through a fresh GUI pass after an untextured clear/fill. Binding the atlas
	// alone is not sufficient in GLES2, so restore the textured draw contract.
#if defined(MCPE_UWP)
	glEnable2(GL_TEXTURE_2D);
	// GUI fills and gradients are allowed to disable alpha testing.  The
	// bitmap font atlas uses zero-alpha pixels for its background, so every
	// original font draw must re-establish the same transparent-text contract
	// as the Win32 fixed-function path before emitting glyph quads.
	glEnable2(GL_ALPHA_TEST);
	glAlphaFunc2(GL_GREATER, 0.1f);
	glEnable2(GL_BLEND);
	glBlendFunc2(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
#endif
	_textures->loadAndBindTexture(fontName);
	float r = ((color >> 16) & 0xff) / 255.0f;
	float g = ((color >> 8) & 0xff) / 255.0f;
	float b = ((color) & 0xff) / 255.0f;
	float a = ((color >> 24) & 0xff) / 255.0f;
	if (a == 0) a = 1;
	glColor4f2(r, g, b, a);

	static const std::string hex("0123456789abcdef");

	index = 0;
	glPushMatrix2();
	glTranslatef2((GLfloat)x, (GLfloat)y, 0.0f);
	for (unsigned int i = 0; i < str.length(); i++) {
		while (str.length() > i + 1 && str[i] == (char)0xA7) {
			int cc = hex.find((char)tolower(str[i + 1]));
			if (cc < 0 || cc > 15) cc = 15;
			lists[index++] = listPos + 256 + cc + (darken ? 16 : 0);

			if (index == 1024) {
				count = index;
				index = 0;
#ifndef USE_VBO
				glCallLists(count, GL_UNSIGNED_INT, lists);
#endif
				count = 1024;
			}

			i += 2;
		}

		if (i < str.length()) {
			//int ch = SharedConstants.acceptableLetters.indexOf(str.charAt(i));
			char ch = str[i];
			if (ch >= 0) {
				//ib.put(listPos + ch + 32);
				lists[index++] = listPos + ch;
			}
		}

		if (index == 1024) {
			count = index;
			index = 0;
#ifndef USE_VBO
			glCallLists(count, GL_UNSIGNED_INT, lists);
#endif
			count = 1024;
		}
	}
	count = index;
	index = 0;
#ifndef USE_VBO
	glCallLists(count, GL_UNSIGNED_INT, lists);
#endif
	glPopMatrix2();
}

int Font::width( const std::string& str )
{
#if defined(MCPE_UWP)
	int maxLen = 0;
	int len = 0;
	bool hasPreviousGlyph = false;
	bool previousUnicode = false;
	unsigned int previousTextCodepoint = 0;
	for (size_t position = 0; position < str.length();)
	{
		const size_t colorLength = colorSequenceLength(str, position);
		if (colorLength)
		{
			position += colorLength;
			continue;
		}
		const unsigned int codepoint = nextCodepoint(str, position);
		if (codepoint == '\n')
		{
			maxLen = (std::max)(maxLen, len);
			len = 0;
			hasPreviousGlyph = false;
			previousTextCodepoint = 0;
		}
		else if (codepoint != '\r')
		{
			const bool currentUnicode = !usesMojangFont(codepoint) ||
				(isAsciiPunctuation(codepoint) &&
				 asciiPunctuationUsesUnifont(str, position, previousTextCodepoint));
			const bool enlargedUnifont = options &&
				(options->guiScale == 1 || options->guiScale == 2);
			const int glyphWidth = currentUnicode
				? (enlargedUnifont ? 10 : 8) : codepointWidth(codepoint);
			len += glyphWidth -
				(hasPreviousGlyph && currentUnicode != previousUnicode ? 1 : 0);
			hasPreviousGlyph = true;
			previousUnicode = currentUnicode;
			if (!isContextSeparator(codepoint))
				previousTextCodepoint = codepoint;
		}
	}
	return (std::max)(maxLen, len);
#else
	int maxLen = 0;
	int len = 0;

	for (unsigned int i = 0; i < str.length(); i++) {
		if (str[i] == (char)0xA7) {
			i++;
		} else {
			//int ch = SharedConstants.acceptableLetters.indexOf(str.charAt(i));
			//if (ch >= 0) {
			//    len += charWidths[ch + 32];
			//}
			if (str[i] == '\n') {
				if (len > maxLen) maxLen = len;
				len = 0;
			}
			else {
				int charWidth = charWidths[ (unsigned char) str[i] ];
				len += charWidth;
			}
		}
	}
	return maxLen>len? maxLen : len;
#endif
}

int Font::codepointWidth(unsigned int codepoint) const
{
	if (usesMojangFont(codepoint) && codepoint <= 0xff)
		return charWidths[mojangGlyphIndex(codepoint)];
	// The 16x16 CJK source is rendered as one 8x8 Mojang-font cell.
	return options && (options->guiScale == 1 || options->guiScale == 2) ? 10 : 8;
}

int Font::lineHeightFor(const std::string& str) const
{
	for (size_t position = 0; position < str.length();)
	{
		const size_t colorLength = colorSequenceLength(str, position);
		if (colorLength)
		{
			position += colorLength;
			continue;
		}
		if (nextCodepoint(str, position) > 0xff)
			return lineHeight;
	}
	return lineHeight;
}

int Font::height( const std::string& str ) {
	int h = 0;
	bool hasLine = false;
	int textLineHeight = lineHeightFor(str);
	for (size_t position = 0; position < str.length();) {
		const size_t colorLength = colorSequenceLength(str, position);
		if (colorLength)
		{
			position += colorLength;
			continue;
		}
		const unsigned int codepoint = nextCodepoint(str, position);
		if (codepoint == '\n') {
			hasLine = true;
		}
		else {
			if (hasLine) h += textLineHeight;
			hasLine = false;
		}
	}
	return h;
}

std::string Font::sanitize( const std::string& str )
{
	std::string sanitized(str.length() + 1, 0);
	int j = 0;

	for (size_t i = 0; i < str.length();) {
		const size_t colorLength = colorSequenceLength(str, i);
		if (colorLength) {
			i += colorLength;
			continue;
		}
		const size_t start = i;
		nextCodepoint(str, i);
		for (size_t byte = start; byte < i; ++byte)
			sanitized[j++] = str[byte];
	}
	return sanitized.erase(j);
}

void Font::drawWordWrap( const std::string& str, float x, float y, float w, int col )
{
#if defined(MCPE_UWP)
	const int textLineHeight = lineHeight;
	std::string line;
	auto emitLine = [&]()
	{
		if (!line.empty())
			drawShadow(line, x, y, col);
		line.clear();
		y += textLineHeight;
	};

	for (size_t position = 0; position < str.length();)
	{
		const size_t start = position;
		const size_t colorLength = colorSequenceLength(str, position);
		unsigned int codepoint = 0;
		if (colorLength)
		{
			position += colorLength;
		}
		else
			codepoint = nextCodepoint(str, position);

		if (!colorLength && codepoint == '\n')
		{
			emitLine();
			continue;
		}
		if (!colorLength && (codepoint == '\r' || codepoint == '\t'))
			continue;

		const std::string token = str.substr(start, position - start);
		if (!line.empty() && width(line + token) > w)
		{
			const size_t breakAt = line.find_last_of(" \t");
			if (breakAt != std::string::npos && breakAt > 0)
			{
				const std::string remainder = line.substr(breakAt + 1);
				line.erase(breakAt);
				emitLine();
				line = remainder;
			}
			else
				emitLine();
		}
		line += token;
	}
	if (!line.empty())
		drawShadow(line, x, y, col);
#else
	char* cstr = new char[str.length() + 1];
	strncpy(cstr, str.c_str(), str.length());
	cstr[str.length()] = 0;

	const char* lims = " \n\t\r";
	char* ptok = strtok(cstr, lims);

	std::vector<std::string> words;
	while (ptok != NULL) {
		words.push_back( ptok );
		ptok = strtok(NULL, lims);
	}

	delete[] cstr;

	int pos = 0;
	while (pos < (int)words.size()) {
		std::string line = words[pos++] + " ";
		while (pos < (int)words.size() && width(line + words[pos]) < w) {
			line += words[pos++] + " ";
		}
		drawShadow(line, x, y, col);
		y += lineHeight;
	}
#endif
}

void Font::drawSlow( const std::string& str, float x, float y, int color, bool darken /*= false*/ ) {
	drawSlow(str.c_str(), x, y, color, darken);
}
void Font::drawSlow( const char* str, float x, float y, int color, bool darken /*= false*/ )
{
	if (!str) return;

	if (darken) {
		int oldAlpha = color & 0xff000000;
		color = (color & 0xfcfcfc) >> 2;
		color += oldAlpha;
	}

	// The UWP GLES2 compatibility layer represents the original fixed-function
	// GL_TEXTURE_2D enable bit as the shader's u_useTexture uniform. Restore it
	// before emitting glyph quads; world HUD callers do this explicitly, while
	// original menu screens can arrive after an untextured GUI pass.
#if defined(MCPE_UWP)
	glEnable2(GL_TEXTURE_2D);
	glEnable2(GL_ALPHA_TEST);
	glAlphaFunc2(GL_GREATER, 0.1f);
	glEnable2(GL_BLEND);
	glBlendFunc2(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	const std::string text(str);
	Tesselator& unicodeTesselator = Tesselator::instance;
	const int unicodeAlpha = ((color >> 24) & 0xff) ? ((color >> 24) & 0xff) : 0xff;
	const bool anaglyph = options && options->anaglyph3d;
	int currentColor = (unicodeAlpha << 24) | (color & 0x00ffffff);
	TextureId activeTexture = Textures::InvalidId;
	bool batchOpen = false;
	float unicodeXOffset = 0;
	float unicodeYOffset = 0;
	bool hasPreviousGlyph = false;
	bool previousUnicode = false;
	unsigned int previousTextCodepoint = 0;
	const float lineAdvance = static_cast<float>(lineHeightFor(text));
	unicodeTesselator.addOffset(x, y, 0);

	auto selectTexture = [&](TextureId texture, bool isUnifontPage) {
		if (texture == Textures::InvalidId)
			return false;
		if (!batchOpen || activeTexture != texture)
		{
			if (batchOpen)
				unicodeTesselator.draw();
			_textures->bind(texture);
			const bool softenUnifont = isUnifontPage && options &&
				(options->guiScale == 1 || options->guiScale == 2);
			const GLint fontFilter = softenUnifont ? GL_LINEAR : GL_NEAREST;
			glTexParameteri2(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, fontFilter);
			glTexParameteri2(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, fontFilter);
			unicodeTesselator.begin();
			setFontVertexColor(unicodeTesselator, currentColor, darken, anaglyph);
			activeTexture = texture;
			batchOpen = true;
		}
		return true;
	};

	auto drawFallback = [&]() {
		const TextureId texture = _textures->loadTexture(fontName);
		if (selectTexture(texture, false))
		{
			buildChar('?', unicodeXOffset, unicodeYOffset + 0.5f);
			unicodeXOffset += fcharWidths[(unsigned char)'?'];
		}
	};

	for (size_t position = 0; position < text.length();)
	{
		const size_t colorLength = colorSequenceLength(text, position);
		if (colorLength)
		{
			currentColor = formatCodeColor(text[position + colorLength - 1], false, false, unicodeAlpha);
			position += colorLength;
			if (batchOpen)
				setFontVertexColor(unicodeTesselator, currentColor, darken, anaglyph);
			continue;
		}

		const unsigned int codepoint = nextCodepoint(text, position);
		if (codepoint == '\n')
		{
			unicodeXOffset = 0;
			unicodeYOffset += lineAdvance;
			hasPreviousGlyph = false;
			previousTextCodepoint = 0;
			continue;
		}
		if (codepoint == '\r' || codepoint == '\t')
			continue;

		const bool currentUnicode = !usesMojangFont(codepoint) ||
			(isAsciiPunctuation(codepoint) &&
			 asciiPunctuationUsesUnifont(text, position, previousTextCodepoint));
		if (hasPreviousGlyph && currentUnicode != previousUnicode)
			unicodeXOffset -= 1.0f;
		if (!currentUnicode && usesMojangFont(codepoint) && codepoint <= 0xff)
		{
			const TextureId texture = _textures->loadTexture(fontName);
			if (!selectTexture(texture, false))
			{
				drawFallback();
				continue;
			}
			const unsigned int glyphIndex = mojangGlyphIndex(codepoint);
			buildChar(static_cast<unsigned char>(glyphIndex), unicodeXOffset, unicodeYOffset + 0.5f);
			unicodeXOffset += fcharWidths[glyphIndex];
			hasPreviousGlyph = true;
			previousUnicode = currentUnicode;
			if (!isContextSeparator(codepoint))
				previousTextCodepoint = codepoint;
			continue;
		}

		TextureId texture = _textures->loadTexture(unifontPageName(codepoint));
		const bool softenUnifont = options &&
			(options->guiScale == 1 || options->guiScale == 2);
		if (softenUnifont && texture != Textures::InvalidId)
		{
			const unsigned int page = codepoint >> 8;
			if (brightenedUnifontPages.find(page) == brightenedUnifontPages.end() && batchOpen)
			{
				unicodeTesselator.draw();
				batchOpen = false;
				activeTexture = Textures::InvalidId;
			}
			texture = getBrightenedUnifontPage(texture, codepoint);
		}
		if (selectTexture(texture, true))
		{

			buildUnicodeChar(codepoint, unicodeXOffset, unicodeYOffset);
			unicodeXOffset += static_cast<float>(currentUnicode ? (softenUnifont ? 10 : 8) : codepointWidth(codepoint));
		}
		else
			drawFallback();
		hasPreviousGlyph = true;
		previousUnicode = currentUnicode;
		if (!isContextSeparator(codepoint))
			previousTextCodepoint = codepoint;
	}

	if (batchOpen)
		unicodeTesselator.draw();
	unicodeTesselator.addOffset(-x, -y, 0);
	return;
#endif
	_textures->loadAndBindTexture(fontName);

	Tesselator& t = Tesselator::instance;
	t.begin();
	int alpha = (0xff000000 & color) >> 24;
	if (!alpha) alpha = 0xff;
	t.color((color >> 16) & 0xff, (color >> 8) & 0xff, color & 0xff, alpha);
	
	t.addOffset((float)x, (float)y, 0);
	float xOffset = 0;
	float yOffset = 0;

	while (unsigned char ch = *(str++)) {
		if (ch == 0xA7 && *str) {
			static const char formatColors[] = "0123456789abcdef";
			const char formatCode = (char)tolower((unsigned char)*(str++));
			const char* colorCode = strchr(formatColors, formatCode);
			int code = colorCode ? (int)(colorCode - formatColors) : 15;
			int br = ((code >> 3) & 1) * 0x55;
			int r = ((code >> 2) & 1) * 0xaa + br;
			int g = ((code >> 1) & 1) * 0xaa + br;
			int b = (code & 1) * 0xaa + br;
			if (code == 6) r += 0x55;
			if (options->anaglyph3d) {
				int cr = (r * 30 + g * 59 + b * 11) / 100;
				int cg = (r * 30 + g * 70) / 100;
				int cb = (r * 30 + b * 70) / 100;
				r = cr;
				g = cg;
				b = cb;
			}
			if (darken) {
				r /= 4;
				g /= 4;
				b /= 4;
			}
			t.color(r, g, b, alpha);
		} else if (ch == '\n') {
			xOffset = 0;
			yOffset += lineHeight;
		} else {
			buildChar(ch, xOffset, yOffset);
			xOffset += fcharWidths[ch];
		}
	}
	t.draw();
	t.addOffset(-(float)x, -(float)y, 0);
}

void Font::buildChar( unsigned char i, float x /*= 0*/, float y /*=0*/ )
{
	Tesselator& t = Tesselator::instance;

	//i -= _charOffset;
	//int ix = (i % _cols) * 8 + _x;
	//int iy = (i / _cols) * 8 + _y;
	float ix = (float)((i & 15) * 8);
	float iy = (float)((i >> 4) * 8);
	float s = 7.99f;

	float uo = (0.0f) / 128.0f;
	float vo = (0.0f) / 128.0f;

	t.vertexUV(x, y + s, 0, ix / 128.0f + uo, (iy + s) / 128.0f + vo);
	t.vertexUV(x + s, y + s, 0, (ix + s) / 128.0f + uo, (iy + s) / 128.0f + vo);
	t.vertexUV(x + s, y, 0, (ix + s) / 128.0f + uo, iy / 128.0f + vo);
	t.vertexUV(x, y, 0, ix / 128.0f + uo, iy / 128.0f + vo);
}

void Font::buildUnicodeChar(unsigned int codepoint, float x, float y)
{
	Tesselator& t = Tesselator::instance;
	const unsigned int slot = codepoint & 0xff;
	const float ix = static_cast<float>((slot & 15) * 16);
	const float iy = static_cast<float>((slot >> 4) * 16);
	const TextureId texture = _textures->loadTexture(unifontPageName(codepoint));
	const TextureData* image = _textures->getTemporaryTextureData(texture);
	static std::map<unsigned int, GlyphBounds> boundsCache;
	std::map<unsigned int, GlyphBounds>::iterator found = boundsCache.find(codepoint);
	if (found == boundsCache.end())
		found = boundsCache.insert(std::make_pair(codepoint, findGlyphBounds(image, slot))).first;
	const GlyphBounds& bounds = found->second;
	const int sourceWidth = bounds.right - bounds.left;
	const int sourceHeight = bounds.bottom - bounds.top;
	if (sourceWidth <= 0 || sourceHeight <= 0)
		return;

	// Cropping removes only transparent margins. Never magnify a small glyph
	// to fill the cell: that made commas and periods much larger than their
	// Unifont bitmap and unlike neighboring CJK punctuation.
	const float cellSize = options && (options->guiScale == 1 || options->guiScale == 2)
		? 10.0f : 8.0f;
	const float scale = (std::min)(0.625f,
		cellSize / static_cast<float>((std::max)(sourceWidth, sourceHeight)));
	const float displayWidth = sourceWidth * scale;
	const float displayHeight = sourceHeight * scale;
	const bool lowerLeftPunctuation = codepoint == 0x002c || codepoint == 0x002e ||
		codepoint == 0x3001 || codepoint == 0x3002 ||
		codepoint == 0xfe10 || codepoint == 0xfe11 || codepoint == 0xfe12 ||
		codepoint == 0xff0c || codepoint == 0xff0e;
	const bool ellipsis = codepoint == 0x2026;
	const float left = lowerLeftPunctuation
		? x : x + (cellSize - displayWidth) * 0.5f;
	const float verticalOffset = cellSize > 8.0f ? -0.5f : 0.0f;
	const float top = (lowerLeftPunctuation || ellipsis)
		? y + cellSize - displayHeight + verticalOffset
		: y + (cellSize - displayHeight) * 0.5f + verticalOffset;
	const float u0 = (ix + bounds.left) / 256.0f;
	const float v0 = (iy + bounds.top) / 256.0f;
	const float u1 = (ix + bounds.right) / 256.0f;
	const float v1 = (iy + bounds.bottom) / 256.0f;
	t.vertexUV(left, top + displayHeight, 0, u0, v1);
	t.vertexUV(left + displayWidth, top + displayHeight, 0, u1, v1);
	t.vertexUV(left + displayWidth, top, 0, u1, v0);
	t.vertexUV(left, top, 0, u0, v0);
}

TextureId Font::getBrightenedUnifontPage(TextureId sourceTexture, unsigned int codepoint)
{
	const unsigned int page = codepoint >> 8;
	std::map<unsigned int, TextureId>::const_iterator cached = brightenedUnifontPages.find(page);
	if (cached != brightenedUnifontPages.end())
		return cached->second;

	const TextureData* source = _textures->getTemporaryTextureData(sourceTexture);
	if (source == NULL || source->data == NULL || source->format != TEXF_UNCOMPRESSED_8888 ||
		source->numBytes <= 0)
		return sourceTexture;

	TextureData brightened = *source;
	brightened.data = new unsigned char[brightened.numBytes];
	brightened.memoryHandledExternally = false;
	std::memcpy(brightened.data, source->data, brightened.numBytes);
	for (int i = 3; i < brightened.numBytes; i += 4)
	{
		// Expand glyph coverage aggressively on softened UI sizes. Fully
		// transparent atlas pixels stay untouched, while every antialiased edge
		// pixel becomes substantially more opaque before linear filtering.
		const int alpha = brightened.data[i];
		if (alpha != 0)
			brightened.data[i] = static_cast<unsigned char>(
				alpha + ((255 - alpha) * 7 + 4) / 8);
	}

	char pageName[8] = {};
	sprintf_s(pageName, sizeof(pageName), "%02X", page);
	const std::string resourceName = std::string("font/unifont_bright/") + pageName + ".png";
	const TextureId brightenedTexture = _textures->assignTexture(resourceName, brightened);
	brightenedUnifontPages[page] = brightenedTexture;
	return brightenedTexture;
}

