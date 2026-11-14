# MCPE 0.6.1 UWP -> Bedrock 1.17.41 conversion baseline

This is the first five-step target contract for a separate Bedrock 1.17.41
exporter. The existing PE 0.9 exporter remains unchanged.

## Level metadata

- `level.dat` uses the little-endian NBT header version `8`.
- `StorageVersion` is `8`.
- `WorldVersion` is `1`.
- `Generator` is `2` (flat).
- `FlatWorldLayers` is a JSON NBT string using `encoding_version: 5`.
- The initial target uses an empty `block_layers` array, which is the
  1.17 representation being tested for a pure-void flat preset.

## LevelDB keys used by the initial writer

Keys are little-endian `int32 x`, little-endian `int32 z`, followed by the
record kind. `0x2f` appends one signed subchunk index byte.

- `0x2f + subchunk`: modern paletted subchunk data.
- `0x2d`: 16-bit heightmap data for the 16x16 chunk columns.
- `0x36`: conversion-data marker, value `2`.

The writer emits one explicit all-air anchor chunk and then translates every
saved 0.6.1 region chunk into modern palettes, writing the converted records
over the corresponding coordinates. The source's 0..127 Y range becomes eight
modern subchunks at indices 0..7.

## Void subchunk

The initial subchunk uses SubChunkPrefix version `9`, one paletted storage,
one-bit indices, and a single `minecraft:air` block palette entry with the
1.17.41 block-state version used by the reference save.

## Converted chunk records

- `0x2c` is written as chunk format version `40`.
- `0x2d` contains 256 little-endian height values followed by 256 biome IDs.
- `0x2f + 0..7` contains the eight converted subchunks.
- `0x31` and `0x32` preserve block entities and legacy per-chunk entities
  when the source chunk has them.
- `~local_player` preserves the source player compound.

For Sky conversions, the writer also emits the world-level `BiomeData` key
with a biome-12 entry whose `snowAccumulation` is `0.0`. The chunks remain
biome 12, so the cold climate, existing snow layers, and ice are preserved;
the override is intended to prevent Bedrock from adding new snow layers to
the converted Sky surface. The generated Sky `level.dat` explicitly sets the
root-level byte tag `doweathercycle` to `1`, so normal weather changes remain
enabled now that the snow cover is materialized in the converted chunks.
Other world types keep the default weather-cycle behavior.

During chunk export, every column mapped to Ice Plains (biome 12) or Snowy
Taiga/Cold Taiga (biome 30) receives one real `minecraft:snow_layer` block on
the highest eligible exposed solid block. This makes the converted snow
cover part of the saved chunk instead of relying on first-load weather
processing.

The first pass maps the 0.6.1 numeric IDs and common data values to Bedrock
1.17 persistent block-state compounds. Any unrecognized legacy block ID is
temporarily mapped to stone and reported through the converter log; this keeps
the output loadable while the remaining rare IDs are audited.
