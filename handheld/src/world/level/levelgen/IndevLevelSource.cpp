#include "IndevLevelSource.h"
#include "AlphaLevelSource.h"

#include "../Level.h"
#include "../LevelSettings.h"
#include "../MobSpawner.h"
#include "../biome/Biome.h"
#include "../biome/BiomeSource.h"
#include "../chunk/LevelChunk.h"
#include "../levelgen/feature/FeatureInclude.h"
#include "../tile/Tile.h"
#include "../LevelConstants.h"
#include "../../../util/Random.h"
#include <algorithm>
#include <cmath>
#include <cstring>

IndevNoiseOctaves::IndevNoiseOctaves(Random* random, int count) {
	for (int i = 0; i < count; ++i) octaves.push_back(new PerlinNoise(random, 1));
}

IndevNoiseOctaves::~IndevNoiseOctaves() {
	for (size_t i = 0; i < octaves.size(); ++i) delete octaves[i];
}

float IndevNoiseOctaves::sample(float x, float z) const {
	float value = 0.0f;
	float scale = 1.0f;
	for (size_t i = 0; i < octaves.size(); ++i, scale *= 2.0f)
		value += octaves[i]->getValue(x / scale, z / scale) * scale;
	return value;
}

IndevLevelSource::IndevLevelSource(Level* level, long seed, int type, bool spawnMobs)
	: level(level), type(type), spawnMobs(spawnMobs), random(seed),
	raiseSourceA(&random, 8), raiseWarpA(&random, 8),
	raiseSourceB(&random, 8), raiseWarpB(&random, 8),
	selectNoise(&random, 6), islandNoise(&random, 2),
	erosionSourceA(&random, 8), erosionWarpA(&random, 8),
	erosionSourceB(&random, 8), erosionWarpB(&random, 8),
	soilNoiseA(&random, 8), soilNoiseB(&random, 8),
	sandNoise(&random, 8), gravelNoise(&random, 8),
	decorationSource(new AlphaLevelSource(level, seed, false))
{
	if (type < IndevType::Inland || type > IndevType::Flat)
		this->type = IndevType::Inland;
	if (this->type != IndevType::Floating)
		buildWaterConnectivity();
}

IndevLevelSource::~IndevLevelSource() {
	delete decorationSource;
}

bool IndevLevelSource::hasChunk(int x, int z) {
	return x >= 0 && z >= 0 && x < LEVEL_WIDTH / 16 && z < LEVEL_DEPTH / 16;
}

LevelChunk* IndevLevelSource::create(int x, int z) { return getChunk(x, z); }

LevelChunk* IndevLevelSource::getChunk(int x, int z) {
	if (!hasChunk(x, z)) return NULL;
	const std::pair<int, int> key(x, z);
	std::map<std::pair<int, int>, LevelChunk*>::iterator found = chunks.find(key);
	if (found != chunks.end()) return found->second;
	random.setSeed((long)((long long)x * 0x4f9939f508LL + (long long)z * 0x1ef1565bd5LL));
	unsigned char* blocks = new unsigned char[LevelChunk::ChunkBlockCount];
	std::memset(blocks, 0, LevelChunk::ChunkBlockCount);
	LevelChunk* chunk = new LevelChunk(level, blocks, x, z);
	chunks.insert(std::make_pair(key, chunk));
	generateTerrain(x, z, blocks);
	buildSurfaces(x, z, blocks);
	caveFeature.apply(this, level, x, z, blocks, LevelChunk::ChunkBlockCount);
	chunk->recalcHeightmap();
	return chunk;
}

void IndevLevelSource::generateTerrain(int chunkX, int chunkZ, unsigned char* blocks) {
	// All four presets retain the full Indev 64-block vertical profile.
	const bool floating = type == IndevType::Floating;
	Random bedrockRandom((long)((long long)chunkX * 0x4f9939f508LL
		+ (long long)chunkZ * 0x1ef1565bd5LL));
	const int sourceHeight = 64;
	const int outputHeight = sourceHeight;
	const int waterLevel = 32;
	const int x0 = chunkX * 16, z0 = chunkZ * 16;
	for (int z = 0; z < 16; ++z) for (int x = 0; x < 16; ++x) {
		const int wx = x0 + x, wz = z0 + z;
		const float edgeX = fabsf(((float)wx / (float)(LEVEL_WIDTH - 1) - 0.5f) * 2.0f);
		const float edgeZ = fabsf(((float)wz / (float)(LEVEL_DEPTH - 1) - 0.5f) * 2.0f);
		int ground, stoneTop, top;
		getColumnHeights(wx, wz, ground, stoneTop, top);

		const float corner = edgeX > edgeZ ? edgeX : edgeZ;
		const float edge = corner * corner * corner;
		const float soil = soilNoiseB.sample(wx * 2.3f, wz * 2.3f) / 24.0f;
		int floatingFloor = (int)(sqrt(fabsf(soil)) * (soil < 0 ? -20.0f : 20.0f)) + waterLevel;
		floatingFloor = (int)(floatingFloor * (1.0f - edge) + edge * sourceHeight);
		if (floatingFloor > waterLevel) floatingFloor = sourceHeight;

		for (int y = 0; y < outputHeight; ++y) {
			int id = 0;
			if (y <= ground) id = Tile::dirt->id;
			if (y <= stoneTop) id = Tile::rock->id;
			if (floating && y < floatingFloor) id = 0;
			if (!floating && y <= bedrockRandom.nextInt(5))
				id = Tile::unbreakable->id;
			// LevelChunk stores Y as the fastest-changing coordinate:
			// (x * 16 + z) * 128 + y. Java Indev's whole-world byte array
			// uses Y-major ordering, so its expression cannot be copied here.
			blocks[(x * 16 + z) * Level::DEPTH + y] = (unsigned char)id;
		}
	}
}

void IndevLevelSource::getColumnHeights(int wx, int wz, int& ground, int& stoneTop, int& surfaceTop) const {
	const int waterLevel = 32;
	const int topLimit = 62;
	const float edgeX = fabsf(((float)wx / (float)(LEVEL_WIDTH - 1) - 0.5f) * 2.0f);
	const float edgeZ = fabsf(((float)wz / (float)(LEVEL_DEPTH - 1) - 0.5f) * 2.0f);
	int raisedHeight = 0;
	if (type != IndevType::Flat) {
		const float distortAX = raiseWarpA.sample(wx * 1.3f, wz * 1.3f);
		const float distortBX = raiseWarpB.sample(wx * 1.3f, wz * 1.3f);
		double a = raiseSourceA.sample(wx * 1.3f + distortAX, wz * 1.3f) / 6.0 - 4.0;
		double b = raiseSourceB.sample(wx * 1.3f + distortBX, wz * 1.3f) / 5.0 + 6.0;
		if (selectNoise.sample((float)wx, (float)wz) / 8.0f > 0.0f) b = a;
		double raised = (a > b ? a : b) / 2.0;
		if (type == IndevType::Island) {
			double falloff = sqrt(edgeX * edgeX + edgeZ * edgeZ) * 1.2;
			const double islandLimit = (double)islandNoise.sample(wx * 0.05f, wz * 0.05f) / 4.0 + 1.0;
			falloff = falloff < islandLimit ? falloff : islandLimit;
			const float edgeMax = edgeX > edgeZ ? edgeX : edgeZ;
			falloff = falloff > edgeMax ? falloff : edgeMax;
			falloff = falloff < 0.0 ? 0.0 : (falloff > 1.0 ? 1.0 : falloff);
			falloff *= falloff;
			raised = raised * (1.0 - falloff) - falloff * 10.0 + 5.0;
			if (raised < 0.0) raised -= raised * raised * 0.2;
		} else if (raised < 0.0) raised *= 0.8;

		raisedHeight = (int)raised;
		const float erosionWarp = erosionWarpA.sample(wx * 2.0f, wz * 2.0f);
		const float erosion = erosionSourceA.sample(wx * 2.0f + erosionWarp, wz * 2.0f) / 8.0f;
		const float parityWarp = erosionWarpB.sample(wx * 2.0f, wz * 2.0f);
		const int parity = erosionSourceB.sample(wx * 2.0f + parityWarp, wz * 2.0f) > 0.0f ? 1 : 0;
		if (erosion > 2.0f) raisedHeight = ((raisedHeight - parity) / 2) * 2 + parity;
	}

	ground = raisedHeight + waterLevel;
	stoneTop = ground + (int)(soilNoiseA.sample((float)wx, (float)wz) / 24.0f) - 4;
	surfaceTop = ground > stoneTop ? ground : stoneTop;
	if (surfaceTop > topLimit) surfaceTop = topLimit;
	if (surfaceTop < 1) surfaceTop = 1;
	if (ground > surfaceTop) ground = surfaceTop;
	if (stoneTop > surfaceTop) stoneTop = surfaceTop;
	if (stoneTop < 0) stoneTop = 0;
}

void IndevLevelSource::buildWaterConnectivity() {
	const int width = LEVEL_WIDTH;
	const int depth = LEVEL_DEPTH;
	const int floodLevel = type == IndevType::Island ? 32 : 17;
	const int waterPlane = floodLevel - 1;
	std::vector<unsigned char> openAtWaterPlane(width * depth, 0);
	waterConnectivity.assign(width * depth, 0);
	for (int z = 0; z < depth; ++z) for (int x = 0; x < width; ++x) {
		int ground, stoneTop, surfaceTop;
		getColumnHeights(x, z, ground, stoneTop, surfaceTop);
		(void)ground;
		(void)stoneTop;
		openAtWaterPlane[x + z * width] = surfaceTop < waterPlane;
	}

	std::vector<int> queue;
	queue.reserve(width * 2 + depth * 2);
	auto enqueue = [&](int x, int z) {
		const int index = x + z * width;
		if (openAtWaterPlane[index] && !waterConnectivity[index]) {
			waterConnectivity[index] = 1;
			queue.push_back(index);
		}
	};
	for (int x = 0; x < width; ++x) {
		enqueue(x, 0);
		enqueue(x, depth - 1);
	}
	for (int z = 1; z < depth - 1; ++z) {
		enqueue(0, z);
		enqueue(width - 1, z);
	}
	for (size_t i = 0; i < queue.size(); ++i) {
		const int index = queue[i];
		const int x = index % width, z = index / width;
		if (x > 0) enqueue(x - 1, z);
		if (x + 1 < width) enqueue(x + 1, z);
		if (z > 0) enqueue(x, z - 1);
		if (z + 1 < depth) enqueue(x, z + 1);
	}
}

void IndevLevelSource::buildSurfaces(int chunkX, int chunkZ, unsigned char* blocks) {
	// Indev's water pass starts at the map edges and spreads only through air
	// connected to the water plane. The precomputed mask prevents enclosed
	// basins from being filled as if they were an ocean.
	const int terrainWaterLevel = 32;
	const int floodLevel = type == IndevType::Island ? terrainWaterLevel : terrainWaterLevel - 15;
	for (int z = 0; z < 16; ++z) for (int x = 0; x < 16; ++x) {
		const int wx = chunkX * 16 + x, wz = chunkZ * 16 + z;
		int top = -1;
		const int columnIndex = wx + wz * LEVEL_WIDTH;
		for (int y = Level::DEPTH - 1; y >= 0; --y) {
			const int id = blocks[(x * 16 + z) * Level::DEPTH + y];
			if (id == Tile::rock->id || id == Tile::dirt->id) { top = y; break; }
		}
		if (top < 0) continue;
		const int topIndex = (x * 16 + z) * Level::DEPTH + top;
		const int originalTop = blocks[topIndex];
		const float sand = sandNoise.sample((float)wx, (float)wz);
		const float gravel = gravelNoise.sample((float)wx, (float)wz);
		const int sourceTop = top;
		const bool beach = sourceTop <= terrainWaterLevel - 1 &&
			sand > (type == IndevType::Island ? -8.0f : 8.0f);
		const bool gravelBeach = sourceTop <= terrainWaterLevel - 1 && gravel > 12.0f;
		// GrowGrassOnDirt runs after water placement in Indev. Only exposed
		// dirt can become grass; submerged soil keeps its original material.
		const bool submerged = type != IndevType::Floating && top < floodLevel &&
			columnIndex < (int)waterConnectivity.size() && waterConnectivity[columnIndex];
		int surface = originalTop == Tile::dirt->id && !submerged
			? Tile::grass->id : originalTop;
		if (gravelBeach) surface = Tile::gravel->id;
		// Indev's Growing stage applies gravel first, then beach sand. Sand
		// therefore wins when both surface conditions match.
		if (beach) surface = Tile::sand->id;
		blocks[topIndex] = (unsigned char)surface;
		for (int d = 1; d <= 3 && top - d >= 0; ++d) {
			const int pos = (x * 16 + z) * Level::DEPTH + top - d;
			if (blocks[pos] == Tile::rock->id) blocks[pos] = (unsigned char)(beach ? Tile::sand->id : Tile::dirt->id);
		}
		// Inland and Flat maps use Indev's lowered sea plane; Island keeps the
		// raising plane so its edge falloff becomes an ocean moat. Floating has
		// no ocean at all.
		if (type != IndevType::Floating && columnIndex < (int)waterConnectivity.size() && waterConnectivity[columnIndex])
			for (int y = top + 1; y < floodLevel; ++y) {
				const int pos = (x * 16 + z) * Level::DEPTH + y;
				if (blocks[pos] == 0) blocks[pos] = (unsigned char)Tile::calmWater->id;
			}
	}
}

void IndevLevelSource::postProcess(ChunkSource* parent, int x, int z) {
	// AlphaLevelSource already owns the project's shared pre-Beta tree, ore,
	// lake and vegetation placement rules. Reuse only that population pass;
	// terrain always comes from this Indev source.
	decorationSource->postProcess(parent, x, z);
	Biome* biome = level->getBiomeSource()->getBiome(x * 16 + 8, z * 16 + 8);
	if (spawnMobs && biome) {
		Random population(level->getSeed() ^ (x * 341873128712LL) ^ (z * 132897987541LL));
		MobSpawner::postProcessSpawnMobs(level, biome, x * 16, z * 16, 16, 16, &population);
	}
}

bool IndevLevelSource::tick() { return false; }
bool IndevLevelSource::shouldSave() { return true; }
std::string IndevLevelSource::gatherStats() { return "IndevLevelSource"; }
Biome::MobList IndevLevelSource::getMobsAt(const MobCategory& category, int x, int y, int z) {
	(void)y;
	Biome* biome = level->getBiomeSource()->getBiome(x, z);
	return biome ? biome->getMobs(category) : Biome::MobList();
}
