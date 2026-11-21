#include "EarlyInfdevLevelSource.h"

#include "feature/FeatureInclude.h"
#include "../Level.h"
#include "../LevelConstants.h"
#include "../biome/Biome.h"
#include "../biome/BiomeSource.h"
#include "../chunk/LevelChunk.h"
#include "../tile/Tile.h"
#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace {
	static void placeOre(Random& random, Level* level, int x, int z, int y,
		int tile, int size) {
	if (y < 0 || y >= Level::DEPTH) return;
	OreFeature feature(tile, size);
	feature.place(level, &random, x, y, z);
	}

	// Java Random with nextInt(512), used by the 20100227 region structure
	// placement. Because 512 is a power of two, nextInt(512) is the high nine
	// bits of next(31), equivalent to state >> 39.
	int javaEarlyInfdevRandom512(std::int32_t seed, int drawIndex) {
		const unsigned long long mask = (1ULL << 48) - 1ULL;
		unsigned long long state =
			((unsigned long long)(long long)seed ^ 0x5deece66dULL) & mask;
		int value = 0;
		for (int i = 0; i <= drawIndex; ++i) {
			state = (state * 0x5deece66dULL + 0xbULL) & mask;
			value = (int)(state >> 39);
		}
		return value;
	}
}

EarlyInfdevNoiseOctaves::EarlyInfdevNoiseOctaves(Random* random, int count) {
	for (int i = 0; i < count; ++i)
		octaves.push_back(new PerlinNoise(random, 1));
}

EarlyInfdevNoiseOctaves::~EarlyInfdevNoiseOctaves() {
	for (size_t i = 0; i < octaves.size(); ++i)
		delete octaves[i];
}

float EarlyInfdevNoiseOctaves::sample(float x, float z) const {
	float value = 0.0f;
	float scale = 1.0f;
	for (size_t i = 0; i < octaves.size(); ++i, scale *= 2.0f)
		value += octaves[i]->getValue(x / scale, z / scale) * scale;
	return value;
}

float EarlyInfdevNoiseOctaves::sample(float x, float y, float z) const {
	float value = 0.0f;
	float scale = 1.0f;
	for (size_t i = 0; i < octaves.size(); ++i, scale *= 2.0f)
		value += octaves[i]->getValue(x / scale, y / scale, z / scale) * scale;
	return value;
}

EarlyInfdevLevelSource::EarlyInfdevLevelSource(Level* level, long seed, bool spawnMobs)
	: level(level), spawnMobs(spawnMobs), random(seed),
	noiseGen1(&random, 16), noiseGen2(&random, 16),
	noiseGen3(&random, 8), noiseGen4(&random, 4),
	noiseGen5(&random, 4), noiseGen6(&random, 5) {
}

EarlyInfdevLevelSource::~EarlyInfdevLevelSource() {
	// ChunkCache owns generated LevelChunk instances.
}

bool EarlyInfdevLevelSource::hasChunk(int, int) {
	return true;
}

LevelChunk* EarlyInfdevLevelSource::create(int x, int z) {
	return getChunk(x, z);
}

LevelChunk* EarlyInfdevLevelSource::getChunk(int chunkX, int chunkZ) {
	const std::pair<int, int> key(chunkX, chunkZ);
	std::map<std::pair<int, int>, LevelChunk*>::iterator found = chunks.find(key);
	if (found != chunks.end())
		return found->second;

	unsigned char* blocks = new unsigned char[LevelChunk::ChunkBlockCount];
	std::memset(blocks, 0, LevelChunk::ChunkBlockCount);
	LevelChunk* chunk = new LevelChunk(level, blocks, chunkX, chunkZ);
	chunks.insert(std::make_pair(key, chunk));
	generateTerrain(chunkX, chunkZ, blocks);
	chunk->recalcHeightmap();
	return chunk;
}

void EarlyInfdevLevelSource::generateTerrain(int chunkX, int chunkZ,
	unsigned char* blocks) {
	const int seaLevel = 63;
	const unsigned long flowerSeed = (unsigned long)level->getSeed() ^
		(unsigned long)(unsigned int)chunkX * 341873128UL ^
		(unsigned long)(unsigned int)chunkZ * 132897987UL ^ 0x1f3d5b79UL;
	Random flowerRandom((long)flowerSeed);
	Random bedrockRandom((long)((long long)chunkX * 341872712LL
		+ (long long)chunkZ * 132899541LL));

	for (int x = 0; x < 16; ++x) {
		for (int z = 0; z < 16; ++z) {
			const int worldX = chunkX * 16 + x;
			const int worldZ = chunkZ * 16 + z;
			const float fx = (float)worldX;
			const float fz = (float)worldZ;

			float heightNoise = (noiseGen1.sample(fx / 0.03125f, 0.0f,
				fz / 0.03125f) - noiseGen2.sample(fx / 0.015625f, 0.0f,
				fz / 0.015625f)) / 512.0f / 4.0f;
			float terrainNoise = noiseGen5.sample(fx / 4.0f, fz / 4.0f);
			const float detailScale = noiseGen6.sample(fx / 8.0f,
				fz / 8.0f) / 8.0f;
			if (terrainNoise > 0.0f) {
				terrainNoise = noiseGen3.sample(fx * 0.25714284f * 2.0f,
					fz * 0.25714284f * 2.0f) * detailScale / 4.0f;
			} else {
				terrainNoise = noiseGen4.sample(fx * 0.25714284f,
					fz * 0.25714284f) * detailScale;
			}

			int surfaceY = (int)(heightNoise + 64.0f + terrainNoise);
			if (noiseGen5.sample(fx, fz) < 0.0f) {
				surfaceY = (surfaceY / 2) * 2;
				// The Java source divides integer block coordinates by 5 before
				// converting them to double; preserve that truncation behavior.
				if (noiseGen5.sample((float)(worldX / 5),
					(float)(worldZ / 5)) < 0.0f)
					++surfaceY;
			}

			// Infdev's region key uses Java integer division (truncation toward
			// zero), including the narrow negative-coordinate regions near zero.
			const int regionX = worldX / 1024;
			const int regionZ = worldZ / 1024;
			const std::uint32_t regionSeedBits =
				(std::uint32_t)regionX + (std::uint32_t)regionZ * 13871U;
			const std::int32_t regionSeed = (std::int32_t)regionSeedBits;
			const long long centerX = (long long)regionX * 1024LL + 128LL +
				javaEarlyInfdevRandom512(regionSeed, 0);
			const long long centerZ = (long long)regionZ * 1024LL + 128LL +
				javaEarlyInfdevRandom512(regionSeed, 1);
			const long long distanceX = std::llabs((long long)worldX - centerX);
			const long long distanceZ = std::llabs((long long)worldZ - centerZ);
			const long long distance = distanceX > distanceZ
				? distanceX : distanceZ;
			long long brickTop = 127LL - distance;
			if (brickTop < surfaceY)
				brickTop = surfaceY;

			for (int y = 0; y < Level::DEPTH; ++y) {
				int tile = 0;
				if (y == surfaceY + 1 && surfaceY >= seaLevel &&
					flowerRandom.nextDouble() < 0.02)
					tile = Tile::flower->id;
				else if (y == surfaceY && surfaceY >= seaLevel)
					tile = Tile::grass->id;
				else if (y <= surfaceY - 2)
					tile = Tile::rock->id;
				else if (y <= surfaceY)
					tile = Tile::dirt->id;
				else if (y <= seaLevel)
					tile = Tile::calmWater->id;

				// The two coordinate planes are the original obsidian walls.
				// Outside them, the region-distance cap replaces air and water
				// with brick, while preserving the terrain beneath it.
				if ((worldX == 0 || worldZ == 0) &&
					(long long)y <= (long long)surfaceY + 2LL) {
					tile = Tile::obsidian->id;
				} else if ((long long)y <= brickTop &&
					(tile == 0 || tile == Tile::calmWater->id)) {
					tile = Tile::redBrick->id;
				}

				if (y <= bedrockRandom.nextInt(5))
					tile = Tile::unbreakable->id;
				blocks[(x * 16 + z) * Level::DEPTH + y] = (unsigned char)tile;
			}
		}
	}
}

void EarlyInfdevLevelSource::postProcess(ChunkSource*, int chunkX, int chunkZ) {
	// Early Infdev uses the Alpha ore-vein distribution while keeping its own
	// terrain generator and excluding Alpha's later lakes, caves and trees.
	Random populationRandom(level->getSeed());
	const long xScale = populationRandom.nextInt() / 2 * 2 + 1;
	const long zScale = populationRandom.nextInt() / 2 * 2 + 1;
	populationRandom.setSeed((long)(((long long)chunkX * xScale
		+ (long long)chunkZ * zScale) ^ (long long)level->getSeed()));

	const int xOrigin = chunkX * 16;
	const int zOrigin = chunkZ * 16;
	for (int i = 0; i < 20; ++i)
		placeOre(populationRandom, level,
			xOrigin + populationRandom.nextInt(16),
			zOrigin + populationRandom.nextInt(16),
			populationRandom.nextInt(128), Tile::coalOre->id, 16);
	for (int i = 0; i < 20; ++i)
		placeOre(populationRandom, level,
			xOrigin + populationRandom.nextInt(16),
			zOrigin + populationRandom.nextInt(16),
			populationRandom.nextInt(64), Tile::ironOre->id, 8);
	for (int i = 0; i < 2; ++i)
		placeOre(populationRandom, level,
			xOrigin + populationRandom.nextInt(16),
			zOrigin + populationRandom.nextInt(16),
			populationRandom.nextInt(32), Tile::goldOre->id, 8);
	for (int i = 0; i < 8; ++i)
		placeOre(populationRandom, level,
			xOrigin + populationRandom.nextInt(16),
			zOrigin + populationRandom.nextInt(16),
			populationRandom.nextInt(16), Tile::redStoneOre->id, 7);
	for (int i = 0; i < 1; ++i)
		// PE 0.6.1's emeraldOre slot is the existing diamond equivalent.
		placeOre(populationRandom, level,
			xOrigin + populationRandom.nextInt(16),
			zOrigin + populationRandom.nextInt(16),
			populationRandom.nextInt(16), Tile::emeraldOre->id, 7);
}

bool EarlyInfdevLevelSource::tick() { return false; }

bool EarlyInfdevLevelSource::shouldSave() { return true; }

std::string EarlyInfdevLevelSource::gatherStats() { return "EarlyInfdevLevelSource"; }

Biome::MobList EarlyInfdevLevelSource::getMobsAt(const MobCategory& category,
	int x, int, int z) {
	BiomeSource* biomeSource = level->getBiomeSource();
	if (biomeSource == NULL)
		return Biome::MobList();
	Biome* biome = biomeSource->getBiome(x, z);
	return biome ? biome->getMobs(category) : Biome::MobList();
}
