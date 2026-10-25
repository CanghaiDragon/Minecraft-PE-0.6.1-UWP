#include "../../client/Minecraft.h"

// The full Win32 Minecraft translation unit is intentionally not linked into
// the current UWP vertical slice.  TileRenderer nevertheless reads this
// original client option when choosing its ambient-occlusion path.  Keep the
// same original default in the UWP renderer until Minecraft.cpp becomes part
// of the client host; this is a storage location for the real option, not a
// replacement implementation of TileRenderer.
bool Minecraft::useAmbientOcclusion = false;
