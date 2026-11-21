#ifndef NET_MINECRAFT_WORLD_LEVEL_LEVELGEN__EarlyInfdevLevelSource_H__
#define NET_MINECRAFT_WORLD_LEVEL_LEVELGEN__EarlyInfdevLevelSource_H__

#include "../chunk/ChunkSource.h"
#include "synth/PerlinNoise.h"
#include "../../../util/Random.h"
#include <map>
#include <utility>
#include <vector>

class Level;
class LevelChunk;

// Infdev's octave sum uses increasing amplitudes at decreasing frequencies.
// PerlinNoise's built-in octave convention is the reverse, so keep the source
// version's convention explicit here.
class EarlyInfdevNoiseOctaves {
public:
	EarlyInfdevNoiseOctaves(Random* random, int count);
	~EarlyInfdevNoiseOctaves();
	float sample(float x, float z) const;
	float sample(float x, float y, float z) const;
private:
	std::vector<PerlinNoise*> octaves;
};

// The 20100227 terrain profile, hosted by the project's infinite chunk cache
// and region-file storage. Terrain structures are placed with the base terrain;
// Alpha-style ore veins are added once during chunk population.
class EarlyInfdevLevelSource: public ChunkSource {
public:
	EarlyInfdevLevelSource(Level* level, long seed, bool spawnMobs);
	virtual ~EarlyInfdevLevelSource();
	virtual bool hasChunk(int x, int z);
	virtual LevelChunk* create(int x, int z);
	virtual LevelChunk* getChunk(int x, int z);
	virtual void postProcess(ChunkSource* parent, int x, int z);
	virtual bool tick();
	virtual bool shouldSave();
	virtual std::string gatherStats();
	virtual Biome::MobList getMobsAt(const MobCategory& category,
		int x, int y, int z);

private:
	void generateTerrain(int chunkX, int chunkZ, unsigned char* blocks);

	Level* level;
	bool spawnMobs;
	Random random;
	EarlyInfdevNoiseOctaves noiseGen1;
	EarlyInfdevNoiseOctaves noiseGen2;
	EarlyInfdevNoiseOctaves noiseGen3;
	EarlyInfdevNoiseOctaves noiseGen4;
	EarlyInfdevNoiseOctaves noiseGen5;
	EarlyInfdevNoiseOctaves noiseGen6;
	std::map<std::pair<int, int>, LevelChunk*> chunks;
};

#endif
