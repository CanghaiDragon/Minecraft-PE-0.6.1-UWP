# MCPE 0.6.1 UWP → 0.9 Converter

Native Windows GUI converter for UWP V2 Infinite/Sky/Alpha saves and
StorageVersion 3 finite saves. It exports terrain, block IDs/data, both light
arrays, player NBT, and tile entities. Non-player entities are deliberately
not exported.

Finite-world input is intentionally restricted to the current UWP
`StorageVersion=3` format. It reads the single `chunks.dat` file, converts its
16x16 finite chunk area, and imports block entities from the global
`entities.dat` file. Older finite formats are rejected instead of attempting
legacy block-light or player-data migration.

The sea-level checkbox removes old `y=63` water/ice. A separate Winter-only
checkbox can also freeze `y=62` water, but it is applied only to Alpha worlds
whose seed selects the Winter biome and only when sea-level lowering is enabled.

A separate Sky-only checkbox can fill air at `y=0..4` with bedrock, giving
converted Sky chunks a continuous bottom floor.

Generate the Visual Studio solution with:

```powershell
cmake -S . -B vs2022 -G "Visual Studio 17 2022" -A x64
```

Then open `vs2022/MCPE061To09Converter.sln` and build `x64-Release`.

The converter always creates a new destination folder and never alters the source world. Sky and Alpha worlds retain their saved terrain, but MCPE 0.9 treats the destination as a normal Infinite world outside converted chunks.
