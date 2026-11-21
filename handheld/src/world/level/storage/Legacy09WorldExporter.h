#pragma once

#include <cstddef>
#include <functional>
#include <string>

namespace legacy09 {

// UI-independent entry point for the PE 0.9+ LevelDB exporter.  The source
// may be a UWP V2 world or a current finite StorageVersion 3 Old/Indev world
// backed by chunks.dat. The source and destination
// are ordinary filesystem paths so this layer can be called by either the
// native game UI or the XAML conversion page.
struct ExportOptions {
    // Keep paths wide until they reach std::filesystem.  UWP LocalFolder can
    // live below a non-ASCII user profile, and converting it through the
    // process ANSI code page would make the destination unavailable.
    std::wstring source;
    std::wstring destination;
    std::wstring name;

    std::size_t maxChunks = 0;
    bool lowerSeaLevel = false;
    bool freezeWinterWater = false;
    bool skyBedrockFloor = false;
    bool floatingBedrockFloor = false;
};

using Log = std::function<void(const std::wstring&)>;

// Returns true after the destination has been written.  On failure, error is
// filled with a human-readable message and the destination should be treated
// as incomplete.
bool Export(const ExportOptions& options, const Log& log, std::wstring& error);

} // namespace legacy09
