#pragma once

#include <windows.storage.h>
#include <ppltasks.h>
#include <string>

namespace UwpZip
{
	// Creates a ZIP file at an app-owned path from the complete contents of
	// the supplied StorageFolder.  The returned task completes only after the
	// central directory has been finalized and the file has been closed.
	concurrency::task<void> CreateFromFolder(
		Windows::Storage::StorageFolder^ source,
		const std::wstring& archivePath);
}
