#include "pch.h"
#include "UwpStoragePaths.h"

#include <cwchar>
#include <windows.h>
#include <windows.storage.h>
#include "../log.h"

using namespace concurrency;
using namespace Platform;
using namespace Windows::Storage;

namespace
{
	const wchar_t* kGamesFolder = L"games";
	const wchar_t* kMojangFolder = L"com.mojang";
	const wchar_t* kWorldsFolder = L"minecraftWorlds";
	const wchar_t* kOptionsFile = L"options.txt";

	task<StorageFolder^> createChildFolder(StorageFolder^ parent, const wchar_t* name)
	{
		return create_task(parent->CreateFolderAsync(
			ref new String(name),
			CreationCollisionOption::OpenIfExists));
	}

	task<StorageFolder^> ensureWorldTreeAsync(StorageFolder^ root)
	{
		return createChildFolder(root, kGamesFolder)
			.then([](StorageFolder^ games)
			{
				return createChildFolder(games, kMojangFolder);
			})
			.then([](StorageFolder^ mojang)
			{
				return createChildFolder(mojang, kWorldsFolder);
			});
	}

	task<StorageFolder^> ensureSkinTreeAsync(StorageFolder^ root)
	{
		return createChildFolder(root, L"data")
			.then([](StorageFolder^ data)
			{
				return createChildFolder(data, L"images");
			})
			.then([](StorageFolder^ images)
			{
				return createChildFolder(images, L"skins");
			});
	}

	bool narrowAscii(const wchar_t* source, std::string& destination)
	{
		destination.clear();
		if (source == nullptr)
			return false;
		for (const wchar_t* character = source; *character != L'\0'; ++character)
		{
			if (*character > 0x7f)
				return false;
			destination.push_back(static_cast<char>(*character));
		}
		return true;
	}

	std::string utf8Path(String^ path)
	{
		if (path == nullptr || path->Length() == 0)
			return std::string();

		const wchar_t* source = path->Data();
		const int required = WideCharToMultiByte(
			CP_UTF8, WC_ERR_INVALID_CHARS, source, -1, nullptr, 0, nullptr, nullptr);
		if (required <= 1)
			return std::string();

		std::string result(static_cast<size_t>(required), '\0');
		const int converted = WideCharToMultiByte(
			CP_UTF8, WC_ERR_INVALID_CHARS, source, -1,
			&result[0], required, nullptr, nullptr);
		if (converted <= 1)
			return std::string();
		result.resize(static_cast<size_t>(converted - 1));
		return result;
	}
}

namespace UwpStoragePaths
{
	String^ localFolderPath()
	{
		return ApplicationData::Current->LocalFolder->Path;
	}

	String^ localCacheFolderPath()
	{
		return ApplicationData::Current->LocalCacheFolder->Path;
	}

	std::string localFolderPathUtf8()
	{
		return utf8Path(localFolderPath());
	}

	std::string localCacheFolderPathUtf8()
	{
		return utf8Path(localCacheFolderPath());
	}

	String^ worldsFolderPath()
	{
		return localFolderPath() + L"\\games\\com.mojang\\minecraftWorlds";
	}

	String^ temporaryWorldsFolderPath()
	{
		return ApplicationData::Current->LocalCacheFolder->Path
			+ L"\\games\\com.mojang\\minecraftWorlds";
	}

	String^ skinFolderPath()
	{
		return localFolderPath() + L"\\data\\images\\skins";
	}

	String^ localSkinPath()
	{
		return skinFolderPath() + L"\\local.png";
	}

	bool localSkinExistsBlocking()
	{
		const std::wstring path(localSkinPath()->Data());
		const DWORD attributes = GetFileAttributesW(path.c_str());
		return attributes != INVALID_FILE_ATTRIBUTES &&
			(attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
	}

	task<StorageFolder^> ensureWorldsFolderAsync()
	{
		return ensureWorldTreeAsync(ApplicationData::Current->LocalFolder);
	}

	task<StorageFolder^> ensureTemporaryWorldsFolderAsync()
	{
		return ensureWorldTreeAsync(ApplicationData::Current->LocalCacheFolder);
	}

	task<StorageFolder^> ensureSkinFolderAsync()
	{
		return ensureSkinTreeAsync(ApplicationData::Current->LocalFolder);
	}

	task<StorageFile^> ensureOptionsFileAsync()
	{
		return create_task(ApplicationData::Current->LocalFolder->CreateFileAsync(
			ref new String(kOptionsFile),
			CreationCollisionOption::OpenIfExists));
	}

	std::vector<std::string> enumerateWorldFolderNamesBlocking()
	{
		std::vector<std::string> names;
		// SelectWorldScreen::init() calls LevelStorageSource::getLevelList()
		// synchronously from the original client tick.  Blocking on a WinRT
		// StorageFolder task here runs on the XAML STA and can re-enter the
		// dispatcher/COM wait path, which caused an access violation while the
		// original world-selection screen was being constructed.  The app-local
		// folder is already inside the UWP package sandbox, so enumerate that
		// directory directly with the synchronous Win32 file API instead.
		const std::wstring root = worldsFolderPath()->Data();
		const std::wstring pattern = root + L"\\*";
		LOGI("UWP world enumeration root=%ls\n", root.c_str());
		WIN32_FIND_DATAW data{};
		HANDLE search = FindFirstFileW(pattern.c_str(), &data);
		if (search == INVALID_HANDLE_VALUE)
		{
			LOGW("UWP world enumeration failed, Win32 error=%lu\n",
				(unsigned long)GetLastError());
			return names;
		}

		do
		{
			if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
				continue;
			if (wcscmp(data.cFileName, L".") == 0 ||
				wcscmp(data.cFileName, L"..") == 0)
				continue;

			std::string name;
			if (narrowAscii(data.cFileName, name))
				names.push_back(name);
		}
		while (FindNextFileW(search, &data) != 0);
		FindClose(search);
		LOGI("UWP world enumeration found=%u\n", (unsigned)names.size());
		return names;
	}
}
