#include "legacy_biome.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>

namespace converter::legacy {
namespace {
constexpr std::uint32_t MatrixA = 0x9908b0dfU;
constexpr std::uint32_t UpperMask = 0x80000000U;
constexpr std::uint32_t LowerMask = 0x7fffffffU;

std::uint32_t wrapMultiply(std::int64_t a, std::int64_t b) {
    return static_cast<std::uint32_t>(static_cast<std::uint64_t>(a) *
                                      static_cast<std::uint64_t>(b));
}
}

Random::Random(std::int32_t seedValue) {
    seed(static_cast<std::uint32_t>(seedValue));
}

void Random::seed(std::uint32_t value) {
    state_[0] = value;
    for (int i = 1; i < 624; ++i) {
        state_[i] = 1812433253U * (state_[i - 1] ^ (state_[i - 1] >> 30)) +
                    static_cast<std::uint32_t>(i);
    }
    index_ = 624;
}

std::uint32_t Random::nextUInt() {
    if (index_ >= 624) {
        if (index_ == 625) {
            seed(5489U);
        }
        std::uint32_t y = 0;
        int k = 0;
        for (; k < 624 - 397; ++k) {
            y = (state_[k] & UpperMask) | (state_[k + 1] & LowerMask);
            state_[k] = state_[k + 397] ^ (y >> 1) ^ ((y & 1U) ? MatrixA : 0U);
        }
        for (; k < 623; ++k) {
            y = (state_[k] & UpperMask) | (state_[k + 1] & LowerMask);
            state_[k] = state_[k - (624 - 397)] ^ (y >> 1) ^ ((y & 1U) ? MatrixA : 0U);
        }
        y = (state_[623] & UpperMask) | (state_[0] & LowerMask);
        state_[623] = state_[396] ^ (y >> 1) ^ ((y & 1U) ? MatrixA : 0U);
        index_ = 0;
    }

    std::uint32_t y = state_[index_++];
    y ^= y >> 11;
    y ^= (y << 7) & 0x9d2c5680U;
    y ^= (y << 15) & 0xefc60000U;
    y ^= y >> 18;
    return y;
}

float Random::nextFloat() {
    return static_cast<float>(static_cast<double>(nextUInt()) / 4294967296.0);
}

int Random::nextInt(int bound) {
    return bound <= 0 ? 0 : static_cast<int>(nextUInt() % static_cast<std::uint32_t>(bound));
}

ImprovedNoise::ImprovedNoise(Random& random) {
    xOffset_ = random.nextFloat() * 256.0f;
    yOffset_ = random.nextFloat() * 256.0f;
    zOffset_ = random.nextFloat() * 256.0f;
    for (int i = 0; i < 256; ++i) {
        permutation_[i] = i;
    }
    for (int i = 0; i < 256; ++i) {
        const int j = random.nextInt(256 - i) + i;
        std::swap(permutation_[i], permutation_[j]);
        permutation_[i + 256] = permutation_[i];
    }
}

float ImprovedNoise::lerp(float t, float a, float b) {
    return a + t * (b - a);
}

float ImprovedNoise::grad2(int hash, float x, float z) {
    const int h = hash & 15;
    const float u = (1 - ((h & 8) >> 3)) * x;
    const float v = h < 4 ? 0.0f : (h == 12 || h == 14 ? x : z);
    return ((h & 1) == 0 ? u : -u) + ((h & 2) == 0 ? v : -v);
}

float ImprovedNoise::value2D(float inputX, float inputZ) const {
    const float x = inputX + xOffset_;
    const float z = inputZ + zOffset_;
    int floorX = static_cast<int>(x);
    int floorZ = static_cast<int>(z);
    if (x < static_cast<float>(floorX)) --floorX;
    if (z < static_cast<float>(floorZ)) --floorZ;

    const int X = floorX & 255;
    const int Z = floorZ & 255;
    const float localX = x - floorX;
    const float localZ = z - floorZ;
    const float u = localX * localX * localX * (localX * (localX * 6.0f - 15.0f) + 10.0f);
    const float w = localZ * localZ * localZ * (localZ * (localZ * 6.0f - 15.0f) + 10.0f);

    const int A = permutation_[X];
    const int AA = permutation_[A] + Z;
    const int B = permutation_[X + 1];
    const int BA = permutation_[B] + Z;
    const float near = lerp(u, grad2(permutation_[AA], localX, localZ),
                            grad2(permutation_[BA], localX - 1.0f, localZ));
    const float far = lerp(u, grad2(permutation_[AA + 1], localX, localZ - 1.0f),
                           grad2(permutation_[BA + 1], localX - 1.0f, localZ - 1.0f));
    return lerp(w, near, far);
}

PerlinNoise::PerlinNoise(std::int32_t seedValue, int levelCount) {
    Random random(seedValue);
    levels_.reserve(static_cast<std::size_t>(levelCount));
    for (int i = 0; i < levelCount; ++i) {
        levels_.emplace_back(random);
    }
}

float PerlinNoise::value2D(float x, float z) const {
    float value = 0.0f;
    float scale = 1.0f;
    for (const ImprovedNoise& level : levels_) {
        value += level.value2D(x * scale, z * scale) / scale;
        scale *= 0.5f;
    }
    return value;
}

std::int32_t BiomeSource::seedProduct(std::int64_t seedValue, std::int32_t multiplier) {
    return static_cast<std::int32_t>(wrapMultiply(seedValue, multiplier));
}

bool BiomeSource::isAlphaWinterSeed(std::int64_t seedValue) {
    // Keep this identical to WorldType::isAlphaWinter in the UWP/Win32 game.
    return (static_cast<std::uint64_t>(seedValue) & 3ULL) == 0ULL;
}

BiomeSource::BiomeSource(std::int64_t seedValue, bool skyWorld, bool alphaWorld,
                         bool plainsWorld)
    : temperature_(seedProduct(seedValue, 9871), 4),
      downfall_(seedProduct(seedValue, 39811), 4),
      noise_(seedProduct(seedValue, 543321), 2),
      skyWorld_(skyWorld),
      alphaWorld_(alphaWorld),
      plainsWorld_(plainsWorld),
      alphaWinter_(alphaWorld && isAlphaWinterSeed(seedValue)) {}

std::uint8_t BiomeSource::classify(float temperature, float downfall) {
    downfall *= temperature;
    // Values below are the target 0.9 biome IDs after applying the mapping
    // selected for this converter.  The branch structure is the original
    // UWP Biome::_getBiome function.
    if (temperature < 0.10f) return 12; // Tundra -> Ice Plains
    if (downfall < 0.20f) {
        if (temperature < 0.50f) return 12; // Tundra -> Ice Plains
        if (temperature < 0.95f) return 35; // Savanna
        return 2;                            // Desert
    }
    if (downfall > 0.5f && temperature < 0.7f) return 1; // Swampland -> Plains
    if (temperature < 0.50f) return 30;                   // Taiga -> Snowy Taiga/Cold Taiga
    if (temperature < 0.97f) {
        if (downfall < 0.35f) return 1; // Shrubland -> Plains
        return 4;                        // Forest
    }
    if (downfall < 0.45f) return 1;       // Plains
    if (downfall < 0.90f) return 4;       // Seasonal Forest -> Forest
    return 21;                             // Rainforest -> Jungle
}

std::uint8_t BiomeSource::targetBiomeId(int blockX, int blockZ) const {
    if (plainsWorld_) return 1; // Indev and EarlyInfdev map to Plains
    if (skyWorld_) return 12; // Sky -> Ice Plains
    if (alphaWorld_) return alphaWinter_ ? 12 : 1; // Winter -> Ice Plains, Alpha -> Plains

    constexpr float temperatureScale = 2.0f / 80.0f;
    constexpr float downfallScale = 2.0f / 40.0f;
    constexpr float noiseScale = 1.0f / 4.0f;
    const float climateNoise = noise_.value2D(blockX * noiseScale, blockZ * noiseScale) * 1.1f + 0.5f;
    float temperature = (temperature_.value2D(blockX * temperatureScale, blockZ * temperatureScale) * 0.15f + 0.7f) * 0.99f + climateNoise * 0.01f;
    float downfall = (downfall_.value2D(blockX * downfallScale, blockZ * downfallScale) * 0.15f + 0.5f) * 0.998f + climateNoise * 0.002f;
    temperature = 1.0f - (1.0f - temperature) * (1.0f - temperature);
    temperature = std::clamp(temperature, 0.0f, 1.0f);
    downfall = std::clamp(downfall, 0.0f, 1.0f);
    return classify(temperature, downfall);
}

std::uint32_t BiomeSource::targetBiomeColor(std::uint8_t id) const {
    // 0.9's LegacyTerrain stores the biome ID in the high byte and a 24-bit
    // grass/foliage tint in the low bytes.  These are the Beta/1.7-era
    // colors used by the 0.9 biome table, not the 0.6.1 colors.
    switch (id) {
    case 2:  return 0xFA9418; // Desert
    case 4:  return 0x79C05A; // Forest
    case 5:  return 0x86B783; // legacy Taiga fallback
    case 30: return 0x76AC8D; // Snowy Taiga/Cold Taiga
    case 12: return 0x80B497; // Ice Plains
    case 21: return 0x59C93C; // Jungle
    case 35: return 0xBDB25F; // Savanna
    case 1:
    default: return 0x91BD59; // Plains and the explicit fallback
    }
}

} // namespace converter::legacy
