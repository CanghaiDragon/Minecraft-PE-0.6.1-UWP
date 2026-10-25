#include "UwpLevelDatSmoke.h"

#include <windows.storage.h>

#include "UwpStoragePaths.h"
#include "../../world/level/storage/ExternalFileLevelStorage.h"
#include "../../world/level/storage/LevelData.h"

namespace
{
	bool narrowPath(const std::wstring& source, std::string& destination)
	{
		destination.clear();
		destination.reserve(source.size());
		for (const wchar_t character : source)
		{
			// The original storage implementation uses stdio's narrow-path API.
			// UWP's LocalCache location is normally ASCII; report a precise
			// platform-adaptation gap rather than silently corrupt a path.
			if (character > 0x7f)
				return false;
			destination.push_back(static_cast<char>(character));
		}
		return true;
	}
}

UwpLevelDatSmokeResult UwpLevelDatSmoke::run()
{
	UwpLevelDatSmokeResult result;

	std::wstring widePath = UwpStoragePaths::temporaryWorldsFolderPath()->Data();
	widePath += L"\\__uwp_leveldat_smoke";
	std::string levelPath;
	if (!narrowPath(widePath, levelPath))
	{
		result.detail = L"level.dat FAIL (non-ASCII LocalCache path)";
		return result;
	}

	// These calls are the unmodified original serializer/deserializer.  The
	// instance constructor creates the isolated directory using the same
	// original storage path used by regular worlds.
	ExternalFileLevelStorage storage("__uwp_leveldat_smoke", levelPath);
	LevelData written;
	written.setSeed(610061L);
	written.setLevelName("UWP level.dat smoke");
	written.setSpawn(12, 64, -8);
	written.setTime(24000L);
	written.setGameType(0);
	storage.saveLevelData(written, nullptr);

	LevelData readBack;
	const bool read = ExternalFileLevelStorage::readLevelData(levelPath, readBack);
	result.succeeded = read
		&& readBack.getSeed() == written.getSeed()
		&& readBack.getLevelName() == written.getLevelName()
		&& readBack.getXSpawn() == written.getXSpawn()
		&& readBack.getYSpawn() == written.getYSpawn()
		&& readBack.getZSpawn() == written.getZSpawn()
		&& readBack.getTime() == written.getTime();
	result.detail = result.succeeded
		? L"level.dat OK (write + read)"
		: L"level.dat FAIL (round-trip mismatch)";
	return result;
}
