#pragma once

#ifndef GL_GLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES 1
#endif
#ifndef GL_GLES_PROTOTYPES
#define GL_GLES_PROTOTYPES 1
#endif
#include <GLES2/gl2.h>
#include "../../client/Options.h"
#include "../../client/gui/Font.h"
#include "../../client/renderer/Textures.h"
#include "../../world/level/LevelListener.h"
#include <cstdint>
#include <array>
#include <memory>
#include <vector>

class Level;
class AppPlatform;
class Chunk;
class LevelRenderer;
class Mob;
class LocalPlayer;

// The terrain renderer is selected as one backend per frame.  Auto keeps the
// original-client path preferred, but falls back while its texture/chunk
// window is unavailable or being rebuilt.  The explicit modes are useful for
// isolating UWP/ANGLE issues without changing the Level or Chunk data.
enum class UwpRenderBackend
{
	Auto,
	OriginalChunk,
	UwpFallback
};

// Keep Chunk forward-declared in this public platform header.  The default
// std::default_delete requires a complete type at every consumer's destructor
// instantiation (including XAML-generated translation units), so the deleter
// is defined in UwpWorldRenderer.cpp after the original Chunk.h is included.
struct UwpChunkDeleter
{
	void operator()(Chunk* chunk) const noexcept;
};
using UwpChunkPtr = std::unique_ptr<Chunk, UwpChunkDeleter>;

// The UWP frame coordinator consumes real MCPE Level data.  It now prefers
// the original LevelRenderer/Chunk VBO path through the shared ANGLE
// compatibility layer and retains the bounded mesh only as a fallback.
class UwpWorldRenderer final : public LevelListener
{
public:
	UwpWorldRenderer() = default;
	~UwpWorldRenderer();
	UwpWorldRenderer(const UwpWorldRenderer&) = delete;
	UwpWorldRenderer& operator=(const UwpWorldRenderer&) = delete;

	bool initialize();
	void shutdown();
	void setLevel(Level* level);
	// When non-null, the original LevelRenderer owns the active chunk cache and
	// terrain submission.  UwpWorldRenderer remains the frame/presentation
	// coordinator and can still fall back to its bounded UWP mesh.
	void setOriginalLevelRenderer(LevelRenderer* renderer, Mob* player);
	void setOriginalLevelRenderer(LevelRenderer* renderer, LocalPlayer* player);
	bool hasOriginalLevelRenderer() const
	{
		return m_originalLevelRenderer != nullptr && m_originalRenderPlayer != nullptr;
	}
	void setTilesDirty(int x0, int y0, int z0, int x1, int y1, int z1) override;
	void tileChanged(int x, int y, int z) override;
	void allChanged() override;
	void setTerrainTexturePixels(unsigned int width, unsigned int height,
		const unsigned char* pixels, unsigned int byteCount);
	// Loads terrain.png through the original MCPE Textures contract and keeps
	// that owner alive.  The current UWP mesh remains unchanged; render() only
	// borrows the resulting GL texture for its draw call.
	bool loadOriginalTerrainTexture(AppPlatform* platform);
	bool hasOriginalTerrainTexture() const { return m_originalTerrainTexture != 0; }
	unsigned int originalTerrainWidth() const { return m_originalTerrainWidth; }
	unsigned int originalTerrainHeight() const { return m_originalTerrainHeight; }
	// Constructs the original bitmap Font on the renderer-owned Textures
	// object.  UwpUiRenderer borrows it for the first Stage 6 HUD seam.
	bool loadOriginalFont();
	bool hasOriginalFont() const { return m_originalFont != nullptr; }
	Font* originalFont() const { return m_originalFont.get(); }
	int originalFontWidth(const std::string& text) const;
	// Unified Stage 6 backend selection.  The old two boolean setters below are
	// retained as compatibility controls; both gates must be enabled before an
	// original Chunk/TileRenderer frame can become active.
	void setRenderBackend(UwpRenderBackend backend) { m_renderBackend = backend; }
	UwpRenderBackend renderBackend() const { return m_renderBackend; }
	UwpRenderBackend activeRenderBackend() const;

	// Legacy compatibility controls.  Disabling either original renderer gate
	// forces the effective frame backend to UwpFallback without destroying the
	// cached original VBOs, so re-enabling it can resume safely.
	void setOriginalTileRendererEnabled(bool enabled)
	{
		m_originalTileRendererEnabled = enabled;
	}
	bool originalTileRendererEnabled() const { return m_originalTileRendererEnabled; }
	// The original Chunk/RenderList gate is kept separate for diagnostics, but
	// it participates in the same one-backend-per-frame decision.
	void setOriginalChunkRendererEnabled(bool enabled)
	{
		m_originalChunkRendererEnabled = enabled;
	}
	bool originalChunkRendererEnabled() const { return m_originalChunkRendererEnabled; }
	void render(float width, float height, float playerX, float playerY, float playerZ,
		float dayTime);

	bool isInitialized() const { return m_initialized; }
	bool hasWorld() const
	{
		return m_level != nullptr &&
			(hasOriginalLevelRenderer() || !m_vertices.empty());
	}
	double maxMeshBuildMilliseconds() const { return m_maxMeshBuildMilliseconds; }
	double maxRenderMilliseconds() const { return m_maxRenderMilliseconds; }

private:
	struct Vertex
	{
		float x;
		float y;
		float z;
		float r;
		float g;
		float b;
		float a;
		float u;
		float v;
	};

	bool createProgram();
	void destroyProgram();
	void drawOriginalFontProbe(float width, float height);
	void drawOriginalTesselatorProbe(float width, float height);
	void drawOriginalTerrainBlock(const float* projection, const float* view,
		float cameraX, float eyeY, float cameraZ, bool writeDepth);
	void destroyOriginalChunks();
	void ensureOriginalChunks(float playerX, float playerZ);
	void rebuildOriginalChunks();
	void drawOriginalChunks(const float* projection, const float* view,
		float cameraX, float eyeY, float cameraZ, bool writeDepth);
	bool renderOriginalLevelRenderer(const float* projection, const float* view,
		float cameraX, float eyeY, float cameraZ);
	void markOriginalChunksDirty(int x0, int y0, int z0,
		int x1, int y1, int z1);
	void rebuildTerrainMesh(float centerX, float centerZ);
	void appendQuad(float x0, float y0, float z0,
		float x1, float y1, float z1,
		float x2, float y2, float z2,
		float x3, float y3, float z3,
		float r, float g, float b, int textureIndex = -1,
		bool flipV = false);
	void appendBlock(int x, int y, int z, int tileId);
	void tileColor(int tileId, float& r, float& g, float& b) const;

	GLuint m_program = 0;
	GLint m_position = -1;
	GLint m_color = -1;
	GLint m_texCoord = -1;
	GLint m_mvp = -1;
	GLint m_texture = -1;
	GLint m_useTexture = -1;
	GLuint m_terrainTexture = 0;
	unsigned int m_terrainWidth = 0;
	unsigned int m_terrainHeight = 0;
	std::unique_ptr<Options> m_originalTextureOptions;
	std::unique_ptr<Textures> m_originalTextures;
	std::unique_ptr<Font> m_originalFont;
	GLuint m_originalTerrainTexture = 0;
	unsigned int m_originalTerrainWidth = 0;
	unsigned int m_originalTerrainHeight = 0;
	bool m_initialized = false;
	bool m_originalTesselatorVboInitialized = false;
	bool m_originalTileRendererEnabled = true;
	bool m_originalChunkRendererEnabled = true;
	UwpRenderBackend m_renderBackend = UwpRenderBackend::Auto;
	Level* m_level = nullptr;
	LevelRenderer* m_originalLevelRenderer = nullptr;
	Mob* m_originalRenderPlayer = nullptr;
	bool m_meshDirty = true;
	bool m_meshCenterValid = false;
	float m_meshCenterX = 0.0f;
	float m_meshCenterZ = 0.0f;
	float m_meshMinY = 0.0f;
	float m_meshMaxY = 0.0f;
	double m_maxMeshBuildMilliseconds = 0.0;
	double m_maxRenderMilliseconds = 0.0;
	std::vector<Vertex> m_vertices;
	static const int kOriginalChunkSize = 16;
	static const int kOriginalChunkColumns = 3;
	static const int kOriginalChunkLayers = 8;
	std::vector<UwpChunkPtr> m_originalChunks;
	std::vector<std::array<GLuint, 3>> m_originalChunkBuffers;
	int m_originalChunkOriginX = 0x3f3f3f3f;
	int m_originalChunkOriginZ = 0x3f3f3f3f;
	int m_originalChunkRebuildCursor = 0;
	bool m_originalChunkRebuildInProgress = false;
};
