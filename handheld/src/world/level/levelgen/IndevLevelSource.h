#ifndef NET_MINECRAFT_WORLD_LEVEL_LEVELGEN__IndevLevelSource_H__
#define NET_MINECRAFT_WORLD_LEVEL_LEVELGEN__IndevLevelSource_H__

#include "../chunk/ChunkSource.h"
#include "LargeCaveFeature.h"
#include "synth/PerlinNoise.h"
#include <map>
#include <utility>
#include <vector>

class Level;
class LevelChunk;
class AlphaLevelSource;

// Java Indev's octave convention (low frequencies have larger weights) is
// opposite to the PE PerlinNoise convenience wrapper, so keep it explicit.
class IndevNoiseOctaves {
public:
	IndevNoiseOctaves(Random* random, int count);
	~IndevNoiseOctaves();
	float sample(float x, float z) const;
private:
	std::vector<PerlinNoise*> octaves;
};

// Finite Indev world using Indev raising/erosion/soiling and shared early
// Overworld population rules for trees, ores and vegetation.
class IndevLevelSource: public ChunkSource
{
public:
	IndevLevelSource(Level* level, long seed, int type, bool spawnMobs);
	virtual ~IndevLevelSource();
	virtual bool hasChunk(int x, int z);
	virtual LevelChunk* create(int x, int z);
	virtual LevelChunk* getChunk(int x, int z);
	virtual void postProcess(ChunkSource* parent, int x, int z);
	virtual bool tick();
	virtual bool shouldSave();
	virtual std::string gatherStats();
	virtual Biome::MobList getMobsAt(const MobCategory& category, int x, int y, int z);

private:
	void getColumnHeights(int x, int z, int& ground, int& stoneTop, int& surfaceTop) const;
	void buildWaterConnectivity();
	void generateTerrain(int chunkX, int chunkZ, unsigned char* blocks);
	void buildSurfaces(int chunkX, int chunkZ, unsigned char* blocks);
	Level* level;
	int type;
	bool spawnMobs;
	Random random;
	IndevNoiseOctaves raiseSourceA;
	IndevNoiseOctaves raiseWarpA;
	IndevNoiseOctaves raiseSourceB;
	IndevNoiseOctaves raiseWarpB;
	IndevNoiseOctaves selectNoise;
	IndevNoiseOctaves islandNoise;
	IndevNoiseOctaves erosionSourceA;
	IndevNoiseOctaves erosionWarpA;
	IndevNoiseOctaves erosionSourceB;
	IndevNoiseOctaves erosionWarpB;
	IndevNoiseOctaves soilNoiseA;
	IndevNoiseOctaves soilNoiseB;
	IndevNoiseOctaves sandNoise;
	IndevNoiseOctaves gravelNoise;
	std::vector<unsigned char> waterConnectivity;
	AlphaLevelSource* decorationSource;
	std::map<std::pair<int, int>, LevelChunk*> chunks;
	LargeCaveFeature caveFeature;
};

#endif
