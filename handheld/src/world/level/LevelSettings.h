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

	// Alpha's Winter Mode is selected once from the world seed.  Using the
	// stable low two bits gives the requested 1/4 distribution while ensuring
	// that reopening a world cannot reroll its biome or weather state.
	inline bool isAlphaWinter(long seed) {
		return ((unsigned long long)seed & 3ULL) == 0ULL;
	}
}

class LevelSettings
{
public:
    LevelSettings(long seed, int gameType, int worldType = WorldType::Old)
    :   seed(seed),
        gameType(gameType),
        worldType(worldType)
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
};

#endif /*NET_MINECRAFT_WORLD_LEVEL__LevelSettings_H__*/
