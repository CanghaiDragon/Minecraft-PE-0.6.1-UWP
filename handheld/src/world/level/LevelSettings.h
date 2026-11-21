#ifndef NET_MINECRAFT_WORLD_LEVEL__LevelSettings_H__
#define NET_MINECRAFT_WORLD_LEVEL__LevelSettings_H__

//package net.minecraft.world.level;

namespace GameType {
	const int Undefined = -1;
	const int Survival = 0;
	const int Creative = 1;

	const int Default = Creative;
}

namespace WorldType {
	const int Old      = 0; // finite 256×256 world
	const int Infinite = 1; // infinite procedural world
	const int Sky      = 2; // infinite floating-island terrain, normal dimension
	const int Alpha    = 3; // infinite Java Alpha-style terrain, Alpha/Winter biome
	const int Indev    = 4; // finite Java Indev-style terrain
	const int EarlyInfdev = 5; // infinite world using the 20100227 generator

	// Alpha's Winter Mode is selected once from the world seed.  Using the
	// stable low two bits gives the requested 1/4 distribution while ensuring
	// that reopening a world cannot reroll its biome or weather state.
	inline bool isAlphaWinter(long seed) {
		return ((unsigned long long)seed & 3ULL) == 0ULL;
	}

	// Indev's hidden Infdev variant is selected once from the world seed.
	inline bool isIndevInfdev(long seed) {
		return ((unsigned long long)seed & 3ULL) == 0ULL;
	}
}

namespace IndevType {
	const int Inland = 0;
	const int Island = 1;
	const int Floating = 2;
	const int Flat = 3;
}

namespace BiomeMapping {
	const int None = 0;
	const int Plains = 1;
}

class LevelSettings
{
public:
    LevelSettings(long seed, int gameType, int worldType = WorldType::Old,
		int indevType = IndevType::Inland, bool resolveIndevInfdev = true)
    :   seed(seed),
        gameType(gameType),
        worldType(worldType),
		indevType(indevType),
		resolveIndevInfdev(resolveIndevInfdev)
    {
    }
	static LevelSettings None() {
		return LevelSettings(-1,-1);
	}

    long getSeed() const {
        return seed;
    }

    int getGameType() const {
        return gameType;
    }

    int getWorldType() const {
        return worldType;
    }

	int getIndevType() const { return indevType; }
	bool shouldResolveIndevInfdev() const { return resolveIndevInfdev; }

	//
	// Those two should actually not be here
	// @todo: Move out when we add LevelSettings.cpp :p
	//
	static int validateGameType(int gameType) {
        switch (gameType) {
		case GameType::Creative:
		case GameType::Survival:
            return gameType;
        }
        return GameType::Default;
    }

	static std::string gameTypeToString(int gameType) {
		if (gameType == GameType::Survival) return "Survival";
		if (gameType == GameType::Creative) return "Creative";
		return "Undefined";
	}

private:
    const long seed;
    const int gameType;
    const int worldType;
	const int indevType;
	const bool resolveIndevInfdev;
};

#endif /*NET_MINECRAFT_WORLD_LEVEL__LevelSettings_H__*/
