#include "Legacy09WorldExporter.h"

#include <filesystem>

// The Converter sources contain the complete GUI-free NBT/region/LevelDB
// conversion core.  This adapter is the only UWP-facing dependency; the
// desktop Win32 main.cpp is deliberately not included in the UWP target.
#include "../../../../../Converter/src/converter.h"

namespace legacy09 {

bool Export(const ExportOptions& options, const Log& log, std::wstring& error) {
    converter::Options coreOptions;
    coreOptions.source = std::filesystem::path(options.source);
    coreOptions.destination = std::filesystem::path(options.destination);
    coreOptions.name = options.name;
    coreOptions.maxChunks = options.maxChunks;
    coreOptions.lowerSeaLevel = options.lowerSeaLevel;
    coreOptions.freezeWinterWater = options.freezeWinterWater;
    coreOptions.skyBedrockFloor = options.skyBedrockFloor;
    coreOptions.floatingBedrockFloor = options.floatingBedrockFloor;

    // The converter emits progress messages after it has written the output.
    // Keep the callback non-empty even when the UWP caller does not request
    // progress reporting; otherwise std::function::operator() throws
    // std::bad_function_call at the end of an otherwise successful export.
    converter::Log coreLog = [&log](const std::wstring& message) {
        if (log)
            log(message);
    };

    std::wstring coreError;
    const bool converted = converter::Convert(coreOptions, coreLog, coreError);
    error = coreError;
    return converted;
}

} // namespace legacy09
