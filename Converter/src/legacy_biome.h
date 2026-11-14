#pragma once

#include <cstdint>
#include <vector>

// The 0.6.1 UWP branch still uses the Beta-era climate generator.  The
// converter cannot link the game sources directly, so this is a small,
// dependency-free port of the pieces needed to rebuild a biome map.
namespace converter::legacy {

class Random {
public:
    explicit Random(std::int32_t seed);

    float nextFloat();
    int nextInt(int bound);

private:
    std::uint32_t state_[624]{};
    int index_ = 625;

    void seed(std::uint32_t value);
    std::uint32_t nextUInt();
};

class ImprovedNoise {
public:
    explicit ImprovedNoise(Random& random);

    float value2D(float x, float z) const;

private:
    float xOffset_ = 0.0f;
    float yOffset_ = 0.0f;
    float zOffset_ = 0.0f;
    int permutation_[512]{};

    static float lerp(float t, float a, float b);
    static float grad2(int hash, float x, float z);
};

class PerlinNoise {
public:
    PerlinNoise(std::int32_t seed, int levels);

    float value2D(float x, float z) const;

private:
    std::vector<ImprovedNoise> levels_;
};

class BiomeSource {
public:
    // skyWorld and alphaWorld correspond to WorldType::Sky and
    // WorldType::Alpha in the UWP source.  Both use a FixedBiomeSource there,
    // so no climate noise is sampled for those world types.  Alpha's fixed
    // biome is selected from the seed in the same way as the game: seeds whose
    // low two bits are zero use Winter, all other seeds use Alpha.
    BiomeSource(std::int64_t seed, bool skyWorld, bool alphaWorld);

    std::uint8_t targetBiomeId(int blockX, int blockZ) const;
    std::uint32_t targetBiomeColor(std::uint8_t id) const;

private:
    PerlinNoise temperature_;
    PerlinNoise downfall_;
    PerlinNoise noise_;
    bool skyWorld_ = false;
    bool alphaWorld_ = false;
    bool alphaWinter_ = false;

    static std::int32_t seedProduct(std::int64_t seed, std::int32_t multiplier);
    static bool isAlphaWinterSeed(std::int64_t seed);
    static std::uint8_t classify(float temperature, float downfall);
};

} // namespace converter::legacy
