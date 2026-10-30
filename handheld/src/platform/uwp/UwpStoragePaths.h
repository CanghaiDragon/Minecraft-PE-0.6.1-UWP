#pragma once

#include <ppltasks.h>
#include <string>
#include <vector>

// UWP's package installation directory is read-only at runtime.  This
// adapter keeps all mutable MCPE data below ApplicationData::Current's local
// folder, while preserving the directory layout used by the original game.
namespace UwpStoragePaths
{
	Platform::String^ localFolderPath();
	Platform::String^ localCacheFolderPath();
	std::string localFolderPathUtf8();
	std::string localCacheFolderPathUtf8();
	Platform::String^ worldsFolderPath();
	Platform::String^ temporaryWorldsFolderPath();
	Platform::String^ skinFolderPath();
	Platform::String^ localSkinPath();
	bool localSkinExistsBlocking();

	concurrency::task<Windows::Storage::StorageFolder^> ensureWorldsFolderAsync();
	concurrency::task<Windows::Storage::StorageFolder^> ensureTemporaryWorldsFolderAsync();
	concurrency::task<Windows::Storage::StorageFolder^> ensureSkinFolderAsync();
	// Synchronous directory enumeration for the original LevelStorageSource
	// contract.  It is safe to call from the original SelectWorldScreen UI tick
	// because the implementation uses the app-local Win32 directory API rather
	// than blocking on a WinRT task.  Original code remains responsible for
	// opening each level.dat and producing summaries.
	std::vector<std::string> enumerateWorldFolderNamesBlocking();
}
