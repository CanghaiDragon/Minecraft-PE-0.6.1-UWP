# MCPE 0.6.1 UWP → 0.9 conversion baseline

This document freezes the format assumptions used by the first four
conversion steps. It is deliberately kept next to the converter source so
future changes can be compared against a known baseline.

## Source

- `level.dat` is UWP v3 little-endian NBT.
- Infinite, Sky, and Alpha chunk files are named
  `chunks.r.<regionX>.<regionZ>.dat`.
- A finite `WorldType=0` source is accepted only when its NBT
  `StorageVersion` is `3`. Its chunks are read from the single `chunks.dat`
  region file, using only local slots `0..15` on both axes. Its global
  `entities.dat` TileEntities list is redistributed into the output chunk
  records.
- A chunk record contains 32768 block IDs, three 16384-byte nibble arrays
  (data, sky light, block light), a 256-byte UWP update map, and an optional
  version-2 NBT block-entity record. The version and payload length in this
  suffix are big-endian 32-bit values (the NBT payload itself is little-endian).

## Target

- `level.dat` is the 0.9 v4 little-endian NBT header used by this project.
- The database is LevelDB with zlib table compression (`compression = 0x02`).
- Each converted chunk writes the 0.9.5/0.14-compatible version key (`0x76` =
  `2`), a LegacyTerrain
  value (`0x30`), and tile entities (`0x31`). Ordinary non-player entities
  are intentionally omitted.
- The `0x31` value is written as concatenated unnamed little-endian NBT
  compound roots, one root for each chest, furnace, sign, or other block
  entity; it is not a wrapper compound containing a `TileEntities` list. Each
  root receives `isMovable: 1b` when the 0.6.1 source omitted it. Empty
  `Items` lists are normalized to list-of-compound type, matching the format
  produced by later clients for freshly placed empty containers. Because the
  0.6.1 UWP serializer filters empty chests and furnaces through
  `shouldSave()`, the converter also synthesizes missing `Chest`/`Furnace`
  roots by scanning block IDs 54, 61, and 62 in each converted chunk.
- The optional sea-level adjustment replaces source water and ice blocks at
  `y=63` (IDs 8, 9, and 79) with air, clears their block data and block light,
  and sets their sky light to 15. When the original `y=63` block is water or
  ice and `y=62` is dirt (ID 3), the dirt is changed to grass (ID 2). It is
  disabled by default and is available for finite, Infinite, and Alpha
  sources; all of these 0.6.1 world types use the y=63 sea surface.
- A separate Winter-only option can be used together with the sea-level
  adjustment. For an Alpha world whose seed selects `Biome::winter`, it
  changes remaining water at `y=62` (IDs 8 and 9) to ice (ID 79). It has no
  effect for ordinary Infinite, Sky, or non-Winter Alpha worlds.
- A separate Sky-only option replaces air at `y=0..4` with bedrock (ID 7) in
  each converted chunk. Existing non-air blocks in that range are preserved;
  the option has no effect for Infinite or Alpha worlds.
- LegacyTerrain is exactly 83200 bytes: 81920 shared terrain bytes, 256
  heightmap bytes, then 256 big-endian 32-bit biome-color entries. Each entry
  stores the 0.9 biome ID in the high byte and its 24-bit tint below it.

## Biome mapping

For ordinary Infinite worlds, the source climate calculation is the Beta-era
UWP `BiomeSource`/`Biome` algorithm, seeded from `RandomSeed`. Sky worlds use
their fixed Sky biome, and Alpha worlds use the seed-selected fixed Alpha or
Winter biome. The selected 0.9 mapping is:

| 0.6.1 biome | 0.9 biome |
|---|---|
| Plains | Plains (1) |
| Desert | Desert (2) |
| Forest | Forest (4) |
| Taiga | Snowy Taiga / Cold Taiga (30) |
| Rainforest | Jungle (21) |
| Seasonal Forest | Forest (4) |
| Savanna | Savanna (35) |
| Swampland | Plains (1) |
| Shrubland | Plains (1) |
| Tundra | Ice Plains (12) |
| Ice Desert | Ice Plains (12) |
| Sky | Ice Plains (12) |
| Alpha | Plains (1) |
| Winter | Ice Plains (12) |

Alpha worlds use the same seed rule as the source game: `seed & 3 == 0`
selects the fixed Winter biome; all other seeds select the fixed Alpha biome.

When a legacy column contains a snow layer (IDs 78 or 80), its target biome
is forced to Snowy Plains / Ice Plains (12) unless it is already Ice Plains
or Snowy Taiga. This preserves old high-altitude snow in newer versions that
validate snow against biome climate.

## First validation run

The GUI performs a full conversion by default. An optional 64-chunk cap can
be enabled for a quick validation run; it selects already-generated chunks
nearest the saved player (falling back to `SpawnX/SpawnZ`).
