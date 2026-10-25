#include "pch.h"
#include "UwpWorldRenderer.h"

#include "../../world/level/Level.h"
#include "../../world/level/chunk/LevelChunk.h"
#include "../../world/level/tile/Tile.h"
#include "../../AppPlatform.h"
#include "../../client/Options.h"
#include "../../client/renderer/Textures.h"
#include "../../client/player/LocalPlayer.h"

// The frame bridge queries LevelRenderer's native dirty queue before the
// renderer-specific GLES compatibility headers are included below.  Keep the
// class definition visible here without pulling its legacy GL macro redirects
// into the UWP fallback renderer's direct GLES2 path.
#define MCPE_LEVELRENDERER_NO_GLES_INCLUDE
#include "../../client/renderer/LevelRenderer.h"
#undef MCPE_LEVELRENDERER_NO_GLES_INCLUDE

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <utility>

namespace
{
const char* kVertexShader =
	"attribute vec3 a_position;\n"
	"attribute vec4 a_color;\n"
	"attribute vec2 a_texcoord;\n"
	"uniform mat4 u_mvp;\n"
	"varying vec4 v_color;\n"
	"varying vec2 v_texcoord;\n"
	"void main() {\n"
	"  gl_Position = u_mvp * vec4(a_position, 1.0);\n"
	"  v_color = a_color;\n"
	"  v_texcoord = a_texcoord;\n"
	"}\n";

const char* kFragmentShader =
	"precision mediump float;\n"
	"varying vec4 v_color;\n"
	"varying vec2 v_texcoord;\n"
	"uniform sampler2D u_texture;\n"
	"uniform float u_useTexture;\n"
	"void main() {\n"
	"  vec4 color = v_color;\n"
	"  if (u_useTexture > 0.5) color *= texture2D(u_texture, v_texcoord);\n"
	"  if (color.a < 0.1) discard;\n"
	"  gl_FragColor = color;\n"
	"}\n";

struct Mat4
{
	float v[16]{};
};

Mat4 multiply(const Mat4& a, const Mat4& b)
{
	Mat4 result;
	for (int column = 0; column < 4; ++column)
	{
		for (int row = 0; row < 4; ++row)
		{
			result.v[column * 4 + row] =
				a.v[0 * 4 + row] * b.v[column * 4 + 0] +
				a.v[1 * 4 + row] * b.v[column * 4 + 1] +
				a.v[2 * 4 + row] * b.v[column * 4 + 2] +
				a.v[3 * 4 + row] * b.v[column * 4 + 3];
		}
	}
	return result;
}

Mat4 perspective(float fovy, float aspect, float zNear, float zFar)
{
	Mat4 result;
	const float f = 1.0f / std::tan(fovy * 0.5f);
	result.v[0] = f / aspect;
	result.v[5] = f;
	result.v[10] = (zFar + zNear) / (zNear - zFar);
	result.v[11] = -1.0f;
	result.v[14] = (2.0f * zFar * zNear) / (zNear - zFar);
	return result;
}

float dot3(float ax, float ay, float az, float bx, float by, float bz)
{
	return ax * bx + ay * by + az * bz;
}

template <typename T>
T clampValue(T value, T low, T high)
{
	return value < low ? low : (value > high ? high : value);
}

template <typename T>
T maxValue(T left, T right)
{
	return left > right ? left : right;
}

template <typename T>
T minValue(T left, T right)
{
	return left < right ? left : right;
}

void cross3(float ax, float ay, float az, float bx, float by, float bz,
	float& x, float& y, float& z)
{
	x = ay * bz - az * by;
	y = az * bx - ax * bz;
	z = ax * by - ay * bx;
}

void normalize3(float& x, float& y, float& z)
{
	const float length = std::sqrt(maxValue(0.000001f, dot3(x, y, z, x, y, z)));
	x /= length;
	y /= length;
	z /= length;
}

Mat4 lookAt(float eyeX, float eyeY, float eyeZ,
	float targetX, float targetY, float targetZ)
{
	float fx = targetX - eyeX;
	float fy = targetY - eyeY;
	float fz = targetZ - eyeZ;
	normalize3(fx, fy, fz);

	float sx = 0.0f;
	float sy = 0.0f;
	float sz = 0.0f;
	cross3(fx, fy, fz, 0.0f, 1.0f, 0.0f, sx, sy, sz);
	normalize3(sx, sy, sz);
	float ux = 0.0f;
	float uy = 0.0f;
	float uz = 0.0f;
	cross3(sx, sy, sz, fx, fy, fz, ux, uy, uz);

	Mat4 result;
	result.v[0] = sx;
	result.v[4] = sy;
	result.v[8] = sz;
	result.v[1] = ux;
	result.v[5] = uy;
	result.v[9] = uz;
	result.v[2] = -fx;
	result.v[6] = -fy;
	result.v[10] = -fz;
	result.v[12] = -dot3(sx, sy, sz, eyeX, eyeY, eyeZ);
	result.v[13] = -dot3(ux, uy, uz, eyeX, eyeY, eyeZ);
	result.v[14] = dot3(fx, fy, fz, eyeX, eyeY, eyeZ);
	result.v[15] = 1.0f;
	return result;
}

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
		std::fprintf(stderr, "UWP world shader compile failed: %s\n", log);
		glDeleteShader(shader);
		return 0;
	}
	return shader;
}
}

UwpWorldRenderer::~UwpWorldRenderer()
{
	shutdown();
}

bool UwpWorldRenderer::createProgram()
{
	GLuint vertex = compileShader(GL_VERTEX_SHADER, kVertexShader);
	GLuint fragment = compileShader(GL_FRAGMENT_SHADER, kFragmentShader);
	if (!vertex || !fragment)
	{
		if (vertex) glDeleteShader(vertex);
		if (fragment) glDeleteShader(fragment);
		return false;
	}

	m_program = glCreateProgram();
	glAttachShader(m_program, vertex);
	glAttachShader(m_program, fragment);
	glBindAttribLocation(m_program, 0, "a_position");
	glBindAttribLocation(m_program, 1, "a_color");
	glBindAttribLocation(m_program, 2, "a_texcoord");
	glLinkProgram(m_program);
	glDeleteShader(vertex);
	glDeleteShader(fragment);

	GLint success = GL_FALSE;
	glGetProgramiv(m_program, GL_LINK_STATUS, &success);
	if (!success)
	{
		char log[1024] = {};
		glGetProgramInfoLog(m_program, sizeof(log), nullptr, log);
		std::fprintf(stderr, "UWP world shader link failed: %s\n", log);
		destroyProgram();
		return false;
	}

	m_position = glGetAttribLocation(m_program, "a_position");
	m_color = glGetAttribLocation(m_program, "a_color");
	m_texCoord = glGetAttribLocation(m_program, "a_texcoord");
	m_mvp = glGetUniformLocation(m_program, "u_mvp");
	m_texture = glGetUniformLocation(m_program, "u_texture");
	m_useTexture = glGetUniformLocation(m_program, "u_useTexture");
	return m_position >= 0 && m_color >= 0 && m_texCoord >= 0 &&
		m_mvp >= 0 && m_texture >= 0 && m_useTexture >= 0;
}

void UwpWorldRenderer::destroyProgram()
{
	if (m_program != 0)
	{
		glDeleteProgram(m_program);
		m_program = 0;
	}
	m_position = -1;
	m_color = -1;
	m_texCoord = -1;
	m_mvp = -1;
	m_texture = -1;
	m_useTexture = -1;
}

bool UwpWorldRenderer::initialize()
{
	if (m_initialized)
		return true;
	m_initialized = createProgram();
	if (!m_initialized)
		destroyProgram();
	return m_initialized;
}

void UwpWorldRenderer::shutdown()
{
	if (m_level != nullptr)
		m_level->removeListener(this);
	destroyOriginalChunks();
	destroyProgram();
	m_originalFont.reset();
	// Textures owns the original GL name.  Release it before the fallback
	// texture and while the ANGLE context is still expected to be current.
	m_originalTextures.reset();
	m_originalTextureOptions.reset();
	m_originalTerrainTexture = 0;
	m_originalTerrainWidth = 0;
	m_originalTerrainHeight = 0;
	if (m_terrainTexture != 0)
	{
		glDeleteTextures(1, &m_terrainTexture);
		m_terrainTexture = 0;
	}
	m_terrainWidth = 0;
	m_terrainHeight = 0;
	m_vertices.clear();
	m_level = nullptr;
	m_originalLevelRenderer = nullptr;
	m_originalRenderPlayer = nullptr;
	m_meshDirty = true;
	m_meshCenterValid = false;
	m_meshCenterX = 0.0f;
	m_meshCenterZ = 0.0f;
	m_meshMinY = 0.0f;
	m_meshMaxY = 0.0f;
	m_originalTesselatorVboInitialized = false;
	m_originalChunkOriginX = 0x3f3f3f3f;
	m_originalChunkOriginZ = 0x3f3f3f3f;
	m_originalChunkRebuildCursor = 0;
	m_initialized = false;
}

bool UwpWorldRenderer::loadOriginalTerrainTexture(AppPlatform* platform)
{
	if (!m_initialized || platform == nullptr)
		return false;
	if (m_originalTerrainTexture != 0)
		return true;

	// Keep the original owner in the renderer so the GL texture remains valid
	// after this frame.  The object is intentionally limited to terrain.png;
	// it does not construct Minecraft or replace the current mesh path.
	m_originalTextureOptions.reset(new Options());
	m_originalTextures.reset(new Textures(m_originalTextureOptions.get(), platform));
	const TextureId texture = m_originalTextures->loadAndBindTexture("terrain.png");
	const TextureData* image = Textures::isTextureIdValid(texture)
		? m_originalTextures->getTemporaryTextureData(texture)
		: nullptr;
	if (!image || !image->data || image->w <= 0 || image->h <= 0)
	{
		m_originalTextures.reset();
		m_originalTextureOptions.reset();
		return false;
	}

	m_originalTerrainTexture = texture;
	m_originalTerrainWidth = static_cast<unsigned int>(image->w);
	m_originalTerrainHeight = static_cast<unsigned int>(image->h);
	m_meshDirty = true;
	m_meshCenterValid = false;
	return true;
}

bool UwpWorldRenderer::loadOriginalFont()
{
	if (!m_initialized || !m_originalTextures || !m_originalTextureOptions)
		return false;
	if (m_originalFont)
		return true;

	// Check the original resource through the same Textures owner that will be
	// used by Font::draw().  This avoids constructing a Font whose glyph table
	// would be uninitialized when the packaged image is unavailable.
	const TextureId texture = m_originalTextures->loadTexture("font/default8.png");
	const TextureData* image = Textures::isTextureIdValid(texture)
		? m_originalTextures->getTemporaryTextureData(texture)
		: nullptr;
	if (!image || !image->data || image->w <= 0 || image->h <= 0)
		return false;

	m_originalFont.reset(new Font(m_originalTextureOptions.get(),
		"font/default8.png", m_originalTextures.get()));
	return m_originalFont != nullptr && originalFontWidth("Original Font OK") > 0;
}

int UwpWorldRenderer::originalFontWidth(const std::string& text) const
{
	return m_originalFont ? m_originalFont->width(text) : 0;
}

void UwpWorldRenderer::setTerrainTexturePixels(unsigned int width, unsigned int height,
	const unsigned char* pixels, unsigned int byteCount)
{
	if (!m_initialized || !pixels || width == 0 || height == 0 ||
		byteCount < width * height * 4u)
		return;
	if (m_terrainTexture == 0)
		glGenTextures(1, &m_terrainTexture);
	glBindTexture(GL_TEXTURE_2D, m_terrainTexture);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA,
		GL_UNSIGNED_BYTE, pixels);
	m_terrainWidth = width;
	m_terrainHeight = height;
	m_meshDirty = true;
	m_meshCenterValid = false;
}

void UwpWorldRenderer::setLevel(Level* level)
{
	if (m_level == level)
	{
		if (level == nullptr)
		{
			m_originalLevelRenderer = nullptr;
			m_originalRenderPlayer = nullptr;
		}
		return;
	}
	if (m_level != nullptr)
		m_level->removeListener(this);
	destroyOriginalChunks();
	// A replacement or cleared Level invalidates the borrowed original
	// renderer/player pair.  Preserve a pair installed before the first frame
	// binds the newly-created Level; AttachOriginalWorldForTick installs it
	// before OnGameRender's initial setLevel(level) call.
	if (level == nullptr || (m_level != nullptr && m_level != level))
	{
		m_originalLevelRenderer = nullptr;
		m_originalRenderPlayer = nullptr;
	}
	m_level = level;
	if (m_level != nullptr)
		m_level->addListener(this);
	m_meshDirty = true;
	m_meshCenterValid = false;
	m_meshMinY = 0.0f;
	m_meshMaxY = 0.0f;
	m_vertices.clear();
	m_maxMeshBuildMilliseconds = 0.0;
	m_maxRenderMilliseconds = 0.0;
}

void UwpWorldRenderer::setOriginalLevelRenderer(LevelRenderer* renderer,
	Mob* player)
{
	m_originalLevelRenderer = renderer;
	m_originalRenderPlayer = player;
}

void UwpWorldRenderer::setOriginalLevelRenderer(LevelRenderer* renderer,
	LocalPlayer* player)
{
	setOriginalLevelRenderer(renderer, static_cast<Mob*>(player));
}

UwpRenderBackend UwpWorldRenderer::activeRenderBackend() const
{
	const bool originalAllowed = m_renderBackend != UwpRenderBackend::UwpFallback &&
		m_originalTileRendererEnabled && m_originalChunkRendererEnabled;
	const bool nativeReady = originalAllowed && m_initialized &&
		m_originalTerrainTexture != 0 && m_originalLevelRenderer != nullptr &&
		m_originalRenderPlayer != nullptr;
	const bool originalReady = originalAllowed && m_initialized &&
		m_originalTerrainTexture != 0 && !m_originalChunks.empty() &&
		!m_originalChunkRebuildInProgress;
	if (nativeReady)
		return UwpRenderBackend::OriginalChunk;
	return originalReady ? UwpRenderBackend::OriginalChunk
		: UwpRenderBackend::UwpFallback;
}

void UwpWorldRenderer::tileColor(int tileId, float& r, float& g, float& b) const
{
	// These colors remain only as the no-atlas fallback.  When the original
	// terrain texture is available, appendBlock() uses its atlas pixels.
	switch (tileId)
	{
	case 2:  r = 0.30f; g = 0.68f; b = 0.20f; break; // grass
	case 3:  r = 0.45f; g = 0.25f; b = 0.14f; break; // dirt
	case 8:
	case 9:  r = 0.12f; g = 0.36f; b = 0.80f; break; // water
	case 12: r = 0.82f; g = 0.72f; b = 0.42f; break; // sand
	case 13: r = 0.42f; g = 0.42f; b = 0.42f; break; // gravel
	case 1:  r = 0.56f; g = 0.56f; b = 0.58f; break; // stone
	default: r = 0.52f; g = 0.34f; b = 0.19f; break;
	}
}

void UwpWorldRenderer::appendQuad(float x0, float y0, float z0,
	float x1, float y1, float z1,
	float x2, float y2, float z2,
	float x3, float y3, float z3,
	float r, float g, float b, int textureIndex, bool flipV)
{
	float u0 = 0.0f;
	float u1 = 0.0f;
	float v0 = 0.0f;
	float v1 = 0.0f;
	const bool useOriginalAtlas = m_originalTerrainTexture != 0 &&
		m_originalTerrainWidth >= 16 && m_originalTerrainHeight >= 16;
	const unsigned int atlasWidth = useOriginalAtlas
		? m_originalTerrainWidth : m_terrainWidth;
	const unsigned int atlasHeight = useOriginalAtlas
		? m_originalTerrainHeight : m_terrainHeight;
	if (textureIndex >= 0 && atlasWidth >= 16 && atlasHeight >= 16)
	{
		const int tileX = textureIndex & 15;
		const int tileY = textureIndex >> 4;
		// Tile::getTexture() returns the original MCPE 16x16 atlas index.  Use
		// the actual dimensions of the original Textures image instead of
		// assuming a 256x256 fallback atlas.  The small inset matches the
		// original renderer's 15.99f edge convention and prevents neighbouring
		// tiles from bleeding when the atlas is sampled with linear filtering.
		const float tileWidth = static_cast<float>(atlasWidth) / 16.0f;
		const float tileHeight = static_cast<float>(atlasHeight) / 16.0f;
		const float insetX = useOriginalAtlas ? 0.01f : 0.0f;
		const float insetY = useOriginalAtlas ? 0.01f : 0.0f;
		u0 = (static_cast<float>(tileX) * tileWidth + insetX) /
			static_cast<float>(atlasWidth);
		u1 = (static_cast<float>(tileX + 1) * tileWidth - insetX) /
			static_cast<float>(atlasWidth);
		// Keep the atlas row numbering used by MCPE's terrain.png.  The face
		// geometry controls whether the V direction needs reversing below.
		v0 = (static_cast<float>(tileY) * tileHeight + insetY) /
			static_cast<float>(atlasHeight);
		v1 = (static_cast<float>(tileY + 1) * tileHeight - insetY) /
			static_cast<float>(atlasHeight);
		if (flipV)
		{
			const float temporary = v0;
			v0 = v1;
			v1 = temporary;
		}
	}
	const Vertex a{ x0, y0, z0, r, g, b, 1.0f, u0, v0 };
	const Vertex c{ x1, y1, z1, r, g, b, 1.0f, u1, v0 };
	const Vertex d{ x2, y2, z2, r, g, b, 1.0f, u1, v1 };
	const Vertex e{ x3, y3, z3, r, g, b, 1.0f, u0, v1 };
	m_vertices.push_back(a);
	m_vertices.push_back(c);
	m_vertices.push_back(d);
	m_vertices.push_back(a);
	m_vertices.push_back(d);
	m_vertices.push_back(e);
}

void UwpWorldRenderer::appendBlock(int x, int y, int z, int tileId)
{
	float r = 0.0f;
	float g = 0.0f;
	float b = 0.0f;
	// The atlas already contains the material colour.  Tinting it with the
	// placeholder tileColor() makes grass sides green and dirt brown twice,
	// which is why their hue differs noticeably from the MCPE textures.  Keep
	// the fallback colours only while the atlas is unavailable; face-specific
	// multipliers below still provide the basic directional lighting.
	const bool hasAtlasTexture = m_originalTerrainTexture != 0 ||
		m_terrainTexture != 0;
	if (hasAtlasTexture)
		r = g = b = 1.0f;
	else
		tileColor(tileId, r, g, b);
	if (tileId <= 0 || tileId >= Tile::NUM_BLOCK_TYPES ||
		Tile::tiles[tileId] == nullptr)
		return;

	Tile* tile = Tile::tiles[tileId];
	// Keep the fallback's simple box path useful for the thin top-snow tile as
	// well.  It is deliberately not cube-shaped in the original code, but its
	// six axis-aligned faces still use the same TileRenderer face tests and
	// texture coordinates.  Other special shapes remain native-only.
	if (!tile->isCubeShaped() && tile != Tile::topSnow)
		return;
	const float x0 = static_cast<float>(x) + tile->xx0;
	const float x1 = static_cast<float>(x) + tile->xx1;
	const float y0 = static_cast<float>(y) + tile->yy0;
	const float y1 = static_cast<float>(y) + tile->yy1;
	const float z0 = static_cast<float>(z) + tile->zz0;
	const float z1 = static_cast<float>(z) + tile->zz1;
	float topR = r;
	float topG = g;
	float topB = b;
	if (hasAtlasTexture && tile == Tile::grass)
	{
		// terrain.png stores the grass-top artwork as a greyscale tile.  The
		// native renderer supplies the biome/PE grass colour separately.
		const int color = tile->getColor(m_level, x, y, z);
		topR = static_cast<float>((color >> 16) & 0xff) / 255.0f;
		topG = static_cast<float>((color >> 8) & 0xff) / 255.0f;
		topB = static_cast<float>(color & 0xff) / 255.0f;
	}

	// Face numbering is the same as TileRenderer: down, up, north, south,
	// west, east.  Tile::shouldRenderFace expects the coordinates of the
	// neighbouring cell, not the coordinates of the block being emitted.  The
	// previous call passed (x,y,z), so a solid block compared itself with itself
	// and every face was culled.  Pass the adjacent cell exactly as the native
	// TileRenderer does.
	if (tile->shouldRenderFace(m_level, x, y - 1, z, 0))
		appendQuad(x0, y0, z0, x1, y0, z0, x1, y0, z1, x0, y0, z1,
			r * 0.52f, g * 0.52f, b * 0.52f,
			tile->getTexture(m_level, x, y, z, 0), true);
	if (tile->shouldRenderFace(m_level, x, y + 1, z, 1))
		appendQuad(x1, y1, z1, x1, y1, z0, x0, y1, z0, x0, y1, z1,
			topR, topG, topB, tile->getTexture(m_level, x, y, z, 1), false);
	if (tile->shouldRenderFace(m_level, x, y, z - 1, 2))
		appendQuad(x1, y0, z0, x0, y0, z0, x0, y1, z0, x1, y1, z0,
			r * 0.68f, g * 0.68f, b * 0.68f,
			tile->getTexture(m_level, x, y, z, 2), true);
	if (tile->shouldRenderFace(m_level, x, y, z + 1, 3))
		appendQuad(x0, y0, z1, x1, y0, z1, x1, y1, z1, x0, y1, z1,
			r * 0.72f, g * 0.72f, b * 0.72f,
			tile->getTexture(m_level, x, y, z, 3), true);
	if (tile->shouldRenderFace(m_level, x - 1, y, z, 4))
		appendQuad(x0, y0, z0, x0, y0, z1, x0, y1, z1, x0, y1, z0,
			r * 0.60f, g * 0.60f, b * 0.60f,
			tile->getTexture(m_level, x, y, z, 4), true);
	if (tile->shouldRenderFace(m_level, x + 1, y, z, 5))
		appendQuad(x1, y0, z1, x1, y0, z0, x1, y1, z0, x1, y1, z1,
			r * 0.80f, g * 0.80f, b * 0.80f,
			tile->getTexture(m_level, x, y, z, 5), true);
}

void UwpWorldRenderer::rebuildTerrainMesh(float centerX, float centerZ)
{
	const auto meshStart = std::chrono::steady_clock::now();
	if (m_level == nullptr)
		return;
	// Build the replacement off the old vector's storage.  A Level can be
	// temporarily empty while a new chunk window is being attached; clearing
	// the last valid fallback first made that transient state render as a full
	// void.  Keep the previous mesh as a safety net if the replacement contains
	// no geometry at all.
	std::vector<Vertex> previousVertices = std::move(m_vertices);
	const float previousMinY = m_meshMinY;
	const float previousMaxY = m_meshMaxY;
	const float previousCenterX = m_meshCenterX;
	const float previousCenterZ = m_meshCenterZ;
	const bool previousCenterValid = m_meshCenterValid;
	m_vertices.clear();
	m_meshMinY = 0.0f;
	m_meshMaxY = 0.0f;

	// Keep a bounded fallback slice centered on the actual player.  The original
	// Chunk path can cover a moving window, while this mesh must remain useful
	// both when F7 forces the fallback and while a new original window rebuilds.
	constexpr int radius = 24;
	const int centerBlockX = static_cast<int>(std::floor(centerX));
	const int centerBlockZ = static_cast<int>(std::floor(centerZ));
	bool firstHeight = true;
	for (int z = centerBlockZ - radius; z < centerBlockZ + radius; ++z)
	{
		for (int x = centerBlockX - radius; x < centerBlockX + radius; ++x)
		{
			const int top = clampValue(m_level->getTopTileY(x, z), 0, 127);
			for (int y = 0; y <= top; ++y)
			{
				const int tileId = m_level->getTile(x, y, z);
				if (tileId <= 0)
					continue;
				if (firstHeight)
				{
					m_meshMinY = static_cast<float>(y);
					m_meshMaxY = static_cast<float>(y);
					firstHeight = false;
				}
				else
				{
					m_meshMinY = minValue(m_meshMinY, static_cast<float>(y));
					m_meshMaxY = maxValue(m_meshMaxY, static_cast<float>(y));
				}
				appendBlock(x, y, z, tileId);
			}
		}
	}
	if (firstHeight)
	{
		m_meshMinY = 63.0f;
		m_meshMaxY = 63.0f;
	}
	if (m_vertices.empty() && !previousVertices.empty())
	{
		m_vertices = std::move(previousVertices);
		m_meshMinY = previousMinY;
		m_meshMaxY = previousMaxY;
		m_meshCenterX = previousCenterX;
		m_meshCenterZ = previousCenterZ;
		m_meshCenterValid = previousCenterValid;
		m_meshDirty = false;
		m_maxMeshBuildMilliseconds = maxValue(m_maxMeshBuildMilliseconds,
			std::chrono::duration<double, std::milli>(
				std::chrono::steady_clock::now() - meshStart).count());
		return;
	}
	m_meshCenterX = static_cast<float>(centerBlockX);
	m_meshCenterZ = static_cast<float>(centerBlockZ);
	m_meshCenterValid = true;
	m_meshDirty = false;
	m_maxMeshBuildMilliseconds = maxValue(m_maxMeshBuildMilliseconds,
		std::chrono::duration<double, std::milli>(
			std::chrono::steady_clock::now() - meshStart).count());
}

void UwpWorldRenderer::render(float width, float height, float playerX,
	float playerY, float playerZ, float dayTime)
{
	const auto renderStart = std::chrono::steady_clock::now();
	if (!m_initialized || width <= 0.0f || height <= 0.0f || m_level == nullptr)
		return;
	const bool originalAllowed = m_renderBackend != UwpRenderBackend::UwpFallback &&
		m_originalTileRendererEnabled && m_originalChunkRendererEnabled;
	const bool nativeLevelRendererRequested = originalAllowed &&
		m_originalLevelRenderer != nullptr && m_originalRenderPlayer != nullptr &&
		m_originalTerrainTexture != 0;
	const bool nativeRebuildPending = nativeLevelRendererRequested &&
		m_originalLevelRenderer->hasPendingChunkUpdates();
	// Recenter the fallback only when it may be visible.  This avoids doing a
	// second full 48x48 scan during normal original-client frames, but guarantees
	// that fallback coverage is present during Chunk-window transitions or an
	// explicit UwpFallback selection.  An empty first scan is also retried:
	// original Level chunk generation can complete after this renderer's first
	// frame, and an empty mesh must not become a permanent cached result.
	const bool fallbackMayDraw = !originalAllowed || m_originalTerrainTexture == 0 ||
		m_vertices.empty() ||
		(nativeRebuildPending ||
			(!nativeLevelRendererRequested &&
				(m_originalChunks.empty() || m_originalChunkRebuildInProgress)));
	constexpr float fallbackRecenterDistance = 20.0f;
	if (fallbackMayDraw && (m_vertices.empty() || !m_meshCenterValid ||
		std::abs(playerX - m_meshCenterX) > fallbackRecenterDistance ||
		std::abs(playerZ - m_meshCenterZ) > fallbackRecenterDistance))
	{
		m_meshDirty = true;
	}
	if (m_meshDirty)
		rebuildTerrainMesh(playerX, playerZ);
	if (originalAllowed && m_originalTerrainTexture != 0 &&
		!nativeLevelRendererRequested)
	{
		ensureOriginalChunks(playerX, playerZ);
		rebuildOriginalChunks();
	}

	const float daylight = 0.55f + 0.45f * maxValue(0.0f,
		std::sin(dayTime * 6.2831853f));
	glViewport(0, 0, static_cast<GLsizei>(width), static_cast<GLsizei>(height));
	glClearColor(0.12f * daylight, 0.22f * daylight, 0.34f * daylight, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	glDepthMask(GL_TRUE);
	// Keep the fallback mesh on the same back-face convention as the original
	// TileRenderer instead of inheriting state from the UI/transparent pass.
	glFrontFace(GL_CCW);
	glCullFace(GL_BACK);
	glEnable(GL_CULL_FACE);
	glDisable(GL_BLEND);
	glUseProgram(m_program);
	// Resolve the effective backend before selecting the atlas.  A forced or
	// temporary fallback must not continue borrowing an original texture that
	// may be unavailable after a graphics reset.
	const bool originalWindowReady = originalAllowed && !nativeLevelRendererRequested &&
		m_originalTerrainTexture != 0 && !m_originalChunks.empty() &&
		!m_originalChunkRebuildInProgress;
	const UwpRenderBackend activeBackend = originalWindowReady
		? UwpRenderBackend::OriginalChunk : UwpRenderBackend::UwpFallback;
	glActiveTexture(GL_TEXTURE0);
	const GLuint terrainTexture = activeBackend == UwpRenderBackend::OriginalChunk
		? m_originalTerrainTexture : m_terrainTexture;
	glBindTexture(GL_TEXTURE_2D, terrainTexture);
	glUniform1i(m_texture, 0);
	glUniform1f(m_useTexture, terrainTexture != 0 ? 1.0f : 0.0f);

	// Camera coordinates come from the real MCPE Player entity.  The original
	// Chunk window recenters at 16-block boundaries, so do not clamp X/Z to the
	// original 48x48 fallback slice: crossing a chunk boundary must move the
	// camera and expose the adjacent original VBOs.
	const float cameraX = playerX;
	const float cameraZ = playerZ;
	const float fallbackY = (m_meshMinY + m_meshMaxY) * 0.5f + 2.0f;
	const float eyeY = playerY > 1.0f ? playerY : fallbackY;
	const float eyeZ = cameraZ;
	const Mat4 projection = perspective(1.10f, width / height, 0.05f, 256.0f);
	const Mat4 view = lookAt(cameraX, eyeY, eyeZ,
		cameraX, eyeY - 0.12f, cameraZ - 1.0f);
	const Mat4 mvp = multiply(projection, view);
	glUniformMatrix4fv(m_mvp, 1, GL_FALSE, mvp.v);

	// Once Minecraft has borrowed the UWP Level/Player, the original
	// LevelRenderer becomes the preferred terrain producer.  It uses the
	// shared GLES2 fixed-function compatibility shader and the same camera
	// orientation; MainPage still owns the EGL frame and swap.
	bool nativeLevelRendererAttempted = false;
	bool nativeLevelRendererDrawn = false;
	// Keep one explicit fallback draw helper.  During a long native rebuild the
	// already-built native chunks and the bounded UWP mesh are complementary:
	// the fallback prevents a void while the native queue drains, while the
	// native layers add the original top-snow/foliage geometry as they become
	// available.  Once the queue is empty, only the native path is submitted.
	const auto drawFallbackMesh = [&]() {
		if (m_vertices.empty())
			return;
		glUseProgram(m_program);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, m_terrainTexture);
		glUniform1i(m_texture, 0);
		glUniform1f(m_useTexture, m_terrainTexture != 0 ? 1.0f : 0.0f);
		glUniformMatrix4fv(m_mvp, 1, GL_FALSE, mvp.v);
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LEQUAL);
		glDepthMask(GL_TRUE);
		glFrontFace(GL_CCW);
		glCullFace(GL_BACK);
		glEnable(GL_CULL_FACE);
		glDisable(GL_BLEND);
		glVertexAttribPointer(m_position, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
			&m_vertices[0].x);
		glVertexAttribPointer(m_color, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex),
			&m_vertices[0].r);
		glVertexAttribPointer(m_texCoord, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
			&m_vertices[0].u);
		glEnableVertexAttribArray(m_position);
		glEnableVertexAttribArray(m_color);
		glEnableVertexAttribArray(m_texCoord);
		glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(m_vertices.size()));
		glDisableVertexAttribArray(m_position);
		glDisableVertexAttribArray(m_color);
		glDisableVertexAttribArray(m_texCoord);
	};
	bool fallbackDrawn = false;
	if (nativeRebuildPending && !m_vertices.empty())
	{
		drawFallbackMesh();
		fallbackDrawn = true;
	}
	if (nativeLevelRendererRequested)
	{
		nativeLevelRendererAttempted = true;
		nativeLevelRendererDrawn = renderOriginalLevelRenderer(
			projection.v, view.v, cameraX, eyeY, cameraZ);
	}
	// LevelRenderer can discover a player displacement and enqueue a new ring
	// during its render/sort pass, after the pre-frame pending check above.  If
	// that happened, fill the holes with the fallback in this same frame rather
	// than waiting one more frame with only a partially populated native ring.
	if (nativeLevelRendererAttempted &&
		m_originalLevelRenderer->hasPendingChunkUpdates() &&
		!fallbackDrawn && !m_vertices.empty())
	{
		drawFallbackMesh();
		fallbackDrawn = true;
	}
	if (nativeLevelRendererAttempted && !nativeLevelRendererDrawn && !fallbackDrawn)
	{
		// The native renderer may have found no compiled batches during its
		// first frame.  Restore the UWP program before drawing the fallback mesh.
		glUseProgram(m_program);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, m_terrainTexture);
		glUniform1i(m_texture, 0);
		glUniform1f(m_useTexture, m_terrainTexture != 0 ? 1.0f : 0.0f);
		glUniformMatrix4fv(m_mvp, 1, GL_FALSE, mvp.v);
	}

	// If the native path found no batch, use the fallback as the sole producer.
	// When a rebuild was detected, drawFallbackMesh() has already supplied the
	// base scene and native RenderLists have been overlaid on top of it.  Once
	// the queue drains, the original TileRenderer/VBO path becomes the sole
	// terrain producer again, avoiding persistent duplicate faces.
	if (!nativeLevelRendererDrawn && !fallbackDrawn &&
		(activeBackend == UwpRenderBackend::UwpFallback ||
		 nativeLevelRendererAttempted) && !m_vertices.empty())
	{
		drawFallbackMesh();
	}

	// Stage 6 seam: submit the original Chunk/RenderList VBO batches.  The
	// fallback is only used until this bounded window is ready, avoiding
	// z-fighting between two independent meshes for the same blocks.
	if (!nativeLevelRendererAttempted &&
		activeBackend == UwpRenderBackend::OriginalChunk)
		drawOriginalChunks(projection.v, view.v, cameraX, eyeY, cameraZ,
		true);

	// The single-block TileRenderer seam remains available for diagnostics, but
	// it is intentionally not submitted in the normal world frame.  Chunk owns
	// the original TileRenderer terrain batch; submitting this probe alongside
	// either backend would violate the one-terrain-backend invariant and could
	// reintroduce depth seams.

	glUseProgram(0);
	m_maxRenderMilliseconds = maxValue(m_maxRenderMilliseconds,
		std::chrono::duration<double, std::milli>(
		std::chrono::steady_clock::now() - renderStart).count());
}

// Keep the original GLES2 macro redirects local to the probe below.  The
// existing UWP terrain renderer above intentionally uses direct GLES2 calls.
#include "../../client/renderer/Tesselator.h"
#include "../../client/renderer/gles.h"
#include "../../client/renderer/TileRenderer.h"
#include "../../client/renderer/Chunk.h"
#include "../../client/renderer/RenderList.h"

bool UwpWorldRenderer::renderOriginalLevelRenderer(const float* projection,
	const float* view, float cameraX, float eyeY, float cameraZ)
{
	if (!m_initialized || m_level == nullptr || m_originalTerrainTexture == 0 ||
		m_originalLevelRenderer == nullptr || m_originalRenderPlayer == nullptr ||
		!m_originalLevelRenderer->hasLevel() || g_shader_program == 0 ||
		projection == nullptr || view == nullptr)
		return false;

	// Rebuilds are deliberately driven from the frame callback, where ANGLE's
	// context is current.  The LevelRenderer itself remains responsible for
	// dirty ordering, neighbor invalidation and Chunk::rebuild().  A bounded
	// update may leave the queue non-empty; already compiled native layers are
	// still useful in that frame and are overlaid on the fallback mesh by the
	// caller.  Returning early here made the bridge stay on the simplified
	// fallback for the entire rebuild window, which omitted top-snow and
	// produced a void whenever the fallback was recentered.
	const bool rebuildComplete =
		m_originalLevelRenderer->updateDirtyChunks(m_originalRenderPlayer, false);
	(void)rebuildComplete;

	shaderBind();
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, m_originalTerrainTexture);
	glEnable(GL_TEXTURE_2D);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	glFrontFace(GL_CCW);
	glCullFace(GL_BACK);

	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	glMultMatrixf(projection);
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();
	// RenderList applies the interpolated player translation.  Pass only the
	// camera orientation here so the world is not translated twice.
	float orientation[16];
	std::copy(view, view + 16, orientation);
	orientation[12] = 0.0f;
	orientation[13] = 0.0f;
	orientation[14] = 0.0f;
	glMultMatrixf(orientation);
	// Match GameRenderer's original terrain ordering.  Layer 0 contains solid
	// cubes (including the thin top-snow tile), layer 1 contains alpha-tested
	// leaves/plants, and layer 2 contains blended liquids.  The previous UWP
	// bridge submitted only layer 0, which made foliage appear opaque/missing
	// depending on the graphics option and left transparent geometry in the
	// wrong depth state.
	int submitted = 0;
	bool hasGeometry = false;

	// Opaque terrain.  Alpha test is kept enabled for the beta grass fringe;
	// fully opaque atlas texels are unaffected and zero-alpha texels are
	// correctly discarded by the GLES2 shader.
	glDisable(GL_BLEND);
	glEnable(GL_ALPHA_TEST);
	glEnable(GL_CULL_FACE);
	glDepthMask(GL_TRUE);
	submitted += m_originalLevelRenderer->render(
		m_originalRenderPlayer, 0, 0.0f);
	hasGeometry = hasGeometry || m_originalLevelRenderer->hasRenderableGeometry(0);

	// Alpha-tested foliage and crossed plants must write depth, but must not
	// enable ordinary source-alpha blending (the original fixed-function path
	// used alpha test for this layer).
	glDisable(GL_BLEND);
	glEnable(GL_ALPHA_TEST);
	glDepthMask(GL_TRUE);
	submitted += m_originalLevelRenderer->render(
		m_originalRenderPlayer, 1, 0.0f);
	hasGeometry = hasGeometry || m_originalLevelRenderer->hasRenderableGeometry(1);

	// Liquids and other translucent tiles are submitted last with the original
	// blend/depth-mask state.  Keeping this call here also prevents stale layer
	// 2 VBOs from being mistaken for an opaque draw after a block update.
	glEnable(GL_BLEND);
	glBlendFunc2(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_ALPHA_TEST);
	glDisable(GL_CULL_FACE);
	glDepthMask(GL_FALSE);
	submitted += m_originalLevelRenderer->render(
		m_originalRenderPlayer, 2, 0.0f);
	hasGeometry = hasGeometry || m_originalLevelRenderer->hasRenderableGeometry(2);

	glBindBuffer2(GL_ARRAY_BUFFER, 0);
	glDepthMask(GL_TRUE);
	glDisable(GL_BLEND);
	glEnable(GL_ALPHA_TEST);
	glEnable(GL_CULL_FACE);
	glPopMatrix();
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glDisable(GL_TEXTURE_2D);
	shaderUnbind();
	(void)cameraX;
	(void)eyeY;
	(void)cameraZ;
	return submitted > 0 && hasGeometry;
}

void UwpChunkDeleter::operator()(Chunk* chunk) const noexcept
{
	delete chunk;
}

void UwpWorldRenderer::destroyOriginalChunks()
{
	// Chunk does not own the VBO names supplied by LevelRenderer on the
	// original client, so release the objects before releasing those names.
	m_originalChunks.clear();
	for (std::array<GLuint, 3>& buffers : m_originalChunkBuffers)
	{
		GLuint liveBuffers[3] = {buffers[0], buffers[1], buffers[2]};
		bool hasBuffer = liveBuffers[0] != 0 || liveBuffers[1] != 0 || liveBuffers[2] != 0;
		if (hasBuffer)
			glDeleteBuffers(3, liveBuffers);
		buffers = {0, 0, 0};
	}
	m_originalChunkBuffers.clear();
	m_originalChunkOriginX = 0x3f3f3f3f;
	m_originalChunkOriginZ = 0x3f3f3f3f;
	m_originalChunkRebuildCursor = 0;
	m_originalChunkRebuildInProgress = false;
}

void UwpWorldRenderer::ensureOriginalChunks(float playerX, float playerZ)
{
	if (m_level == nullptr)
		return;

	// Keep a small original-client Chunk window around the player.  The window
	// is deliberately bounded here; LevelRenderer's full 16x16x16 scheduling
	// and frustum sort remain a later stage.
	const int playerChunkX = static_cast<int>(std::floor(playerX / static_cast<float>(kOriginalChunkSize)));
	const int playerChunkZ = static_cast<int>(std::floor(playerZ / static_cast<float>(kOriginalChunkSize)));
	const int originX = (playerChunkX - 1) * kOriginalChunkSize;
	const int originZ = (playerChunkZ - 1) * kOriginalChunkSize;
	const std::size_t expected = static_cast<std::size_t>(kOriginalChunkColumns *
		kOriginalChunkColumns * kOriginalChunkLayers);
	if (m_originalChunkOriginX == originX && m_originalChunkOriginZ == originZ &&
		m_originalChunks.size() == expected)
		return;

	if (m_originalChunks.size() == expected &&
		m_originalChunkBuffers.size() == expected)
	{
		// Reuse the existing original-client Chunk/VBO window when the player
		// crosses a chunk boundary.  Chunk::setPos() marks each entry dirty, but
		// the previous VBO contents remain valid until that entry is rebuilt.
		// Keeping those overlapping chunks visible avoids the all-world blank
		// frame caused by destroying 72 VBOs and rebuilding them incrementally.
		std::size_t index = 0;
		for (int cy = 0; cy < kOriginalChunkLayers; ++cy)
		{
			for (int cz = 0; cz < kOriginalChunkColumns; ++cz)
			{
				for (int cx = 0; cx < kOriginalChunkColumns; ++cx, ++index)
				{
					Chunk* chunk = m_originalChunks[index].get();
					if (chunk == nullptr)
						continue;
					chunk->setPos(originX + cx * kOriginalChunkSize,
						cy * kOriginalChunkSize,
						originZ + cz * kOriginalChunkSize);
					// The old VBO is intentionally drawable while this new
					// coordinate slot waits for its bounded rebuild turn.
					chunk->visible = true;
				}
			}
		}
	}
	else
	{
		destroyOriginalChunks();
		m_originalChunks.reserve(expected);
		m_originalChunkBuffers.reserve(expected);
		for (int cy = 0; cy < kOriginalChunkLayers; ++cy)
		{
			for (int cz = 0; cz < kOriginalChunkColumns; ++cz)
			{
				for (int cx = 0; cx < kOriginalChunkColumns; ++cx)
				{
					std::array<GLuint, 3> buffers = {0, 0, 0};
					glGenBuffers2(3, buffers.data());
					m_originalChunkBuffers.push_back(buffers);
					m_originalChunks.emplace_back(new Chunk(m_level,
						originX + cx * kOriginalChunkSize,
						cy * kOriginalChunkSize,
						originZ + cz * kOriginalChunkSize,
						kOriginalChunkSize, 0, m_originalChunkBuffers.back().data()));
					m_originalChunks.back()->visible = true;
					m_originalChunks.back()->setDirty();
				}
			}
		}
	}
	m_originalChunkOriginX = originX;
	m_originalChunkOriginZ = originZ;
	m_originalChunkRebuildCursor = 0;
	m_originalChunkRebuildInProgress = true;

	if (!m_originalTesselatorVboInitialized)
	{
		Tesselator::instance.init();
		m_originalTesselatorVboInitialized = true;
	}
}

void UwpWorldRenderer::rebuildOriginalChunks()
{
	if (m_level == nullptr || m_originalChunks.empty())
		return;

	// Rebuild only a few original chunks per frame.  This keeps the existing
	// fallback mesh responsive while Region/TileRenderer populate neighboring
	// LevelChunks lazily during the first window build.
	const int maxPerFrame = 4;
	int rebuilt = 0;
	int inspected = 0;
	const int count = static_cast<int>(m_originalChunks.size());
	while (inspected < count && rebuilt < maxPerFrame)
	{
		if (m_originalChunkRebuildCursor >= count)
			m_originalChunkRebuildCursor = 0;
		Chunk* chunk = m_originalChunks[m_originalChunkRebuildCursor].get();
		++m_originalChunkRebuildCursor;
		++inspected;
		if (chunk == nullptr || !chunk->isDirty())
			continue;
		chunk->rebuild();
		chunk->setClean();
		++rebuilt;
	}

	bool dirty = false;
	for (const UwpChunkPtr& chunk : m_originalChunks)
	{
		if (chunk != nullptr && chunk->isDirty())
		{
			dirty = true;
			break;
		}
	}
	m_originalChunkRebuildInProgress = dirty;
}

void UwpWorldRenderer::drawOriginalChunks(const float* projection,
	const float* view, float cameraX, float eyeY, float cameraZ, bool writeDepth)
{
	if (!m_initialized || m_originalTerrainTexture == 0 ||
		g_shader_program == 0 || projection == nullptr || view == nullptr ||
		m_originalChunks.empty())
		return;

	RenderList list;
	list.clear();
	list.init(cameraX, eyeY, cameraZ);
	for (const UwpChunkPtr& chunk : m_originalChunks)
	{
		if (chunk == nullptr || !chunk->visible)
			continue;
		const RenderChunk& renderChunk = chunk->getRenderChunk(0);
		if (renderChunk.vboId == 0 || renderChunk.vertexCount <= 0)
			continue;
		list.addR(renderChunk);
		list.next();
	}
	if (list.listIndex <= 0)
		return;

	shaderBind();
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, m_originalTerrainTexture);
	glEnable(GL_TEXTURE_2D);
	glDisable(GL_BLEND);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	// A partially rebuilt Chunk window is an additive seam over the fallback and
	// must not occlude it.  Once the window is complete, normal depth writes are
	// required so nearer block faces hide farther faces within the VBO batch.
	glDepthMask(writeDepth ? GL_TRUE : GL_FALSE);

	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	glMultMatrixf(projection);
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();
	// RenderList::render() applies the original camera translation itself via
	// glTranslatef2(-xOff,-yOff,-zOff).  Keep only the view orientation here so
	// the world-space vertices produced by Chunk are not translated twice.
	float chunkView[16];
	std::copy(view, view + 16, chunkView);
	chunkView[12] = 0.0f;
	chunkView[13] = 0.0f;
	chunkView[14] = 0.0f;
	glMultMatrixf(chunkView);
	list.render();
	glBindBuffer2(GL_ARRAY_BUFFER, 0);
	glDepthMask(GL_TRUE);
	glPopMatrix();
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glDisable(GL_TEXTURE_2D);
	shaderUnbind();
}

void UwpWorldRenderer::markOriginalChunksDirty(int x0, int y0, int z0,
	int x1, int y1, int z1)
{
	m_meshDirty = true;
	if (x1 < x0) std::swap(x0, x1);
	if (y1 < y0) std::swap(y0, y1);
	if (z1 < z0) std::swap(z0, z1);
	for (const UwpChunkPtr& chunk : m_originalChunks)
	{
		if (chunk == nullptr)
			continue;
		const bool overlap = chunk->x <= x1 && chunk->x + chunk->xs - 1 >= x0 &&
			chunk->y <= y1 && chunk->y + chunk->ys - 1 >= y0 &&
			chunk->z <= z1 && chunk->z + chunk->zs - 1 >= z0;
		if (overlap)
			chunk->setDirty();
	}
}

void UwpWorldRenderer::setTilesDirty(int x0, int y0, int z0,
	int x1, int y1, int z1)
{
	// Include the one-block border because TileRenderer tests neighboring
	// blocks when deciding whether a face is visible, especially at chunk edges.
	markOriginalChunksDirty(x0 - 1, y0 - 1, z0 - 1,
		x1 + 1, y1 + 1, z1 + 1);
}

void UwpWorldRenderer::tileChanged(int x, int y, int z)
{
	setTilesDirty(x, y, z, x, y, z);
}

void UwpWorldRenderer::allChanged()
{
	m_meshDirty = true;
	for (const UwpChunkPtr& chunk : m_originalChunks)
	{
		if (chunk != nullptr)
			chunk->setDirty();
	}
}

void UwpWorldRenderer::drawOriginalTerrainBlock(const float* projection,
	const float* view, float cameraX, float eyeY, float cameraZ, bool writeDepth)
{
	if (!m_initialized || m_originalTerrainTexture == 0 ||
		g_shader_program == 0 || projection == nullptr || view == nullptr ||
		m_vertices.size() < 6)
		return;

	// appendQuad() expands every real MCPE face to six vertices in the order
	// [a,c,d,a,d,e].  Pick the nearest face that is in front of the current
	// first-person camera, then recover that face's block coordinate from its
	// extents and winding.  This keeps the seam tied to real Level block data
	// while avoiding a hard-coded world coordinate that may be off-screen.
	std::size_t selected = m_vertices.size();
	float selectedScore = 1.0e30f;
	for (std::size_t i = 0; i + 5 < m_vertices.size(); i += 6)
	{
		const Vertex& a = m_vertices[i + 0];
		const Vertex& c = m_vertices[i + 1];
		const Vertex& d = m_vertices[i + 2];
		const float centerX = (a.x + c.x + d.x) / 3.0f;
		const float centerY = (a.y + c.y + d.y) / 3.0f;
		const float centerZ = (a.z + c.z + d.z) / 3.0f;
		const float depth = cameraZ - centerZ;
		if (depth <= 0.05f)
			continue;
		const float dx = centerX - cameraX;
		const float dy = centerY - eyeY;
		const float score = dx * dx * 2.0f + dy * dy + depth * depth * 0.01f;
		if (score < selectedScore)
		{
			selected = i;
			selectedScore = score;
		}
	}
	if (selected == m_vertices.size())
		return;

	const auto faceBlock = [](const Vertex& a, const Vertex& c,
		const Vertex& d, int& blockX, int& blockY, int& blockZ)
	{
		// Use the renderer-local helpers instead of std::min/std::max: Windows
		// headers may define those names as macros even in std::min syntax.
		const float minX = minValue(a.x, minValue(c.x, d.x));
		const float maxX = maxValue(a.x, maxValue(c.x, d.x));
		const float minY = minValue(a.y, minValue(c.y, d.y));
		const float maxY = maxValue(a.y, maxValue(c.y, d.y));
		const float minZ = minValue(a.z, minValue(c.z, d.z));
		const float maxZ = maxValue(a.z, maxValue(c.z, d.z));
		const float nx = (c.y - a.y) * (d.z - a.z) -
			(c.z - a.z) * (d.y - a.y);
		const float ny = (c.z - a.z) * (d.x - a.x) -
			(c.x - a.x) * (d.z - a.z);
		const float nz = (c.x - a.x) * (d.y - a.y) -
			(c.y - a.y) * (d.x - a.x);
		const float epsilon = 0.001f;
		blockX = std::abs(maxX - minX) > 0.5f
			? static_cast<int>(std::floor(minX + epsilon))
			: static_cast<int>(std::floor(minX + epsilon)) - (nx > 0.0f ? 1 : 0);
		blockY = std::abs(maxY - minY) > 0.5f
			? static_cast<int>(std::floor(minY + epsilon))
			: static_cast<int>(std::floor(minY + epsilon)) - (ny < 0.0f ? 1 : 0);
		blockZ = std::abs(maxZ - minZ) > 0.5f
			? static_cast<int>(std::floor(minZ + epsilon))
			: static_cast<int>(std::floor(minZ + epsilon)) - (nz > 0.0f ? 1 : 0);
	};

	int selectedBlockX = 0;
	int selectedBlockY = 0;
	int selectedBlockZ = 0;
	faceBlock(m_vertices[selected + 0], m_vertices[selected + 1],
		m_vertices[selected + 2], selectedBlockX, selectedBlockY, selectedBlockZ);

	if (!m_originalTesselatorVboInitialized)
	{
		Tesselator::instance.init();
		m_originalTesselatorVboInitialized = true;
	}

	// The custom pass has already populated the depth buffer.  LEQUAL lets the
	// original block faces replace the same pixels without changing the depth
	// result; depth writes stay disabled so this diagnostic seam cannot occlude
	// the remaining UWP mesh.
	shaderBind();
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, m_originalTerrainTexture);
	glEnable(GL_TEXTURE_2D);
	glDisable(GL_BLEND);
	glDisable(GL_ALPHA_TEST);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	glDepthMask(writeDepth ? GL_TRUE : GL_FALSE);

	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	glMultMatrixf(projection);
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();
	glMultMatrixf(view);

	Tesselator& tesselator = Tesselator::instance;
	const auto emit = [&tesselator](const Vertex& vertex)
	{
		const int r = clampValue(static_cast<int>(vertex.r * 255.0f), 0, 255);
		const int g = clampValue(static_cast<int>(vertex.g * 255.0f), 0, 255);
		const int b = clampValue(static_cast<int>(vertex.b * 255.0f), 0, 255);
		tesselator.color(r, g, b, 255);
		tesselator.vertexUV(vertex.x, vertex.y, vertex.z, vertex.u, vertex.v);
	};
	bool originalTileRendered = false;
	if (m_originalTileRendererEnabled && m_level != nullptr)
	{
		const int tileId = m_level->getTile(selectedBlockX, selectedBlockY,
			selectedBlockZ);
		Tile* tile = tileId > 0 && tileId < Tile::NUM_BLOCK_TYPES
			? Tile::tiles[tileId] : nullptr;
		if (tile != nullptr && tile->isCubeShaped())
		{
			tesselator.begin();
			TileRenderer originalTileRenderer(m_level);
			originalTileRendered = originalTileRenderer.tesselateBlockInWorld(
				tile, selectedBlockX, selectedBlockY, selectedBlockZ);
			if (originalTileRendered)
				tesselator.draw();
			else
				tesselator.clear();
		}
	}

	// Keep the previously validated batch as the fallback for a non-cube,
	// unsupported, or explicitly disabled original TileRenderer path.
	if (!originalTileRendered)
	{
		tesselator.begin();
		for (std::size_t i = 0; i + 5 < m_vertices.size(); i += 6)
		{
			int blockX = 0;
			int blockY = 0;
			int blockZ = 0;
			faceBlock(m_vertices[i + 0], m_vertices[i + 1], m_vertices[i + 2],
				blockX, blockY, blockZ);
			if (blockX != selectedBlockX || blockY != selectedBlockY ||
				blockZ != selectedBlockZ)
				continue;
			emit(m_vertices[i + 0]);
			emit(m_vertices[i + 1]);
			emit(m_vertices[i + 2]);
			emit(m_vertices[i + 5]);
		}
		tesselator.draw();
	}

	// Tesselator::draw() clears its CPU batch and unbinds the UWP VBO, but keep
	// the boundary explicit because the following frame still uses client
	// memory for the current terrain renderer.
	glBindBuffer2(GL_ARRAY_BUFFER, 0);
	glDepthMask(GL_TRUE);
	glPopMatrix();
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glDisable(GL_TEXTURE_2D);
	shaderUnbind();
}

void UwpWorldRenderer::drawOriginalFontProbe(float width, float height)
{
	if (!m_initialized || !m_originalFont || g_shader_program == 0 ||
		width <= 320.0f || height <= 64.0f)
		return;

	if (!m_originalTesselatorVboInitialized)
	{
		Tesselator::instance.init();
		m_originalTesselatorVboInitialized = true;
	}

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
	glOrthof(0.0f, width, height, 0.0f, -1.0f, 1.0f);
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();
	glScalef2(2.0f, 2.0f, 1.0f);

	// Keep the marker away from the existing top-left diagnostics and the
	// bottom status text.  The text is drawn by the original Font/Tesselator
	// path, not by UwpUiRenderer.
	m_originalFont->draw("Original Font OK", (width - 250.0f) * 0.5f,
		24.0f, 0xffffd070);

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

void UwpWorldRenderer::drawOriginalTesselatorProbe(float width, float height)
{
	if (!m_initialized || g_shader_program == 0 || width <= 48.0f || height <= 48.0f)
		return;

	if (!m_originalTesselatorVboInitialized)
	{
		Tesselator::instance.init();
		m_originalTesselatorVboInitialized = true;
	}

	shaderBind();
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_TEXTURE_2D);

	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	glOrthof(0.0f, width, height, 0.0f, -1.0f, 1.0f);
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();

	Tesselator& tesselator = Tesselator::instance;
	tesselator.begin();
	tesselator.color(255, 0, 255, 255);
	const float left = width - 32.0f;
	const float top = 8.0f;
	const float right = width - 8.0f;
	const float bottom = 32.0f;
	tesselator.vertex(left, top, 0.0f);
	tesselator.vertex(right, top, 0.0f);
	tesselator.vertex(right, bottom, 0.0f);
	tesselator.vertex(left, bottom, 0.0f);
	tesselator.draw();

	// Be explicit even though Tesselator::draw() also restores this on UWP;
	// this probe must never leak VBO state into the following XAML UI pass.
	glBindBuffer2(GL_ARRAY_BUFFER, 0);
	glPopMatrix();
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);

	glEnable(GL_DEPTH_TEST);
	shaderUnbind();
}
