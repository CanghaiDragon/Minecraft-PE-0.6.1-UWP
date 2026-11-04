#pragma once

// Updates the application's primary live-tile queue.  The queue is owned by
// the UWP shell so every UWP architecture uses the same tile implementation.
// 0 = off, 1 = simple (grass only), 2 = full (all current tile pages).
void UwpLiveTileSetMode(int mode);

// Legacy boolean entry point retained for callers outside the options bridge.
// true maps to the full mode.
void UwpLiveTileSetEnabled(bool enabled);

// Refreshes the internal saved-world count and the current tile queue.
void UwpLiveTileWorldsChanged();
unsigned int UwpLiveTileGetWorldCount();
