#include "Legacy17WorldExporter.h"

#include <filesystem>

#include "../../../../../Converter/src/converter.h"

namespace legacy17 {

bool Export(const ExportOptions& options, const Log& log, std::wstring& error) {
    converter::Options17 coreOptions;
    coreOptions.source = std::filesystem::path(options.source);
    coreOptions.destination = std::filesystem::path(options.destination);
    coreOptions.name = options.name;
    coreOptions.maxChunks = options.maxChunks;
    coreOptions.writeVoidAnchorChunk = options.writeVoidAnchorChunk;

    converter::Log coreLog = [&log](const std::wstring& message) {
        if (log)
            log(message);
    };

    std::wstring coreError;
    const bool converted = converter::Convert17(
        coreOptions, coreLog, coreError);
    error = coreError;
    return converted;
}

} // namespace legacy17
