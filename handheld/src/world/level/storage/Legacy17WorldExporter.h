#pragma once

#include <cstddef>
#include <functional>
#include <string>

namespace legacy17 {

// UI-independent entry point for the Bedrock 1.17.41 exporter. It creates the
// 1.17 metadata/void preset and converts existing 0.6.1 chunks without using
// the PE 0.9 LegacyTerrain path.
struct ExportOptions {
    std::wstring source;
    std::wstring destination;
    std::wstring name;
    std::size_t maxChunks = 0;
    bool writeVoidAnchorChunk = true;
};

using Log = std::function<void(const std::wstring&)>;

bool Export(const ExportOptions& options, const Log& log, std::wstring& error);

} // namespace legacy17
