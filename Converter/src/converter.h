#pragma once
#include <cstddef>
#include <filesystem>
#include <functional>
#include <string>

namespace converter {
struct Options {
  std::filesystem::path source;
  std::filesystem::path destination;
  std::wstring name;

  // 0 means all available chunks.  The GUI exposes a 64-chunk test cap as
  // an opt-in mode; normal conversion is complete by default.
  std::size_t maxChunks = 0;

  // Remove source water at y=63 so old chunks meet the 0.9 sea level at y=62.
  bool lowerSeaLevel = false;

  // Winter-only option. Applied only with lowerSeaLevel and for an Alpha
  // world whose saved AlphaWinterMode selects Winter.
  bool freezeWinterWater = false;

  // Alpha/Winter surface snow is derived automatically when lowerSeaLevel is
  // enabled: a y=63 water/ice block cleared above a solid non-water y=62
  // block receives one top-snow layer in the PE 0.9 LegacyTerrain output.
  // In addition, all columns mapped to a snowy target biome receive one
  // persisted snow layer on their highest eligible exposed solid block in
  // both the PE 0.9 and Bedrock 1.17 exporters.

  // Sky-only option. Replace air at y=0..4 with bedrock in converted chunks.
  bool skyBedrockFloor = false;

  // Indev Floating-only option. Replace every block at y=0 with bedrock.
  bool floatingBedrockFloor = false;
};

// Initial Bedrock 1.17.41 exporter contract.  This is intentionally separate
// from Options: the PE 0.9 writer uses LegacyTerrain (0x30), while 1.17 uses
// SubChunkPrefix (0x2f) records and a StorageVersion 8 level.dat.
struct Options17 {
  std::filesystem::path source;
  std::filesystem::path destination;
  std::wstring name;

  // 0 means all source chunks.  A non-zero value keeps the same nearest-to-
  // player test mode as the PE 0.9 exporter.
  std::size_t maxChunks = 0;

  // The exporter writes a pure-void anchor and converts any existing 0.6.1
  // chunks into modern 1.17 subchunk palettes. The option remains available
  // so callers can omit the anchor when they only want copied chunks.
  bool writeVoidAnchorChunk = true;
};
using Log = std::function<void(const std::wstring&)>;
bool Convert(const Options& options, const Log& log, std::wstring& error);
bool Convert17(const Options17& options, const Log& log, std::wstring& error);
}
