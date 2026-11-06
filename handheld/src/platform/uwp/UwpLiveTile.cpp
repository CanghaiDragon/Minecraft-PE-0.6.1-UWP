#include "pch.h"
#include "UwpLiveTile.h"
#include "UwpStoragePaths.h"
#include "../../world/level/storage/ExternalFileLevelStorage.h"
#include "../../world/level/storage/LevelData.h"

#include <windows.data.xml.dom.h>
#include <windows.ui.notifications.h>
#include <ctime>
#include <sstream>
#include <string>

using namespace Platform;
using namespace Windows::Data::Xml::Dom;
using namespace Windows::UI::Notifications;

namespace
{
	int g_tileMode = 0;
	unsigned int g_worldCount = 0;
	std::wstring g_latestWorldName;
	std::wstring g_latestWorldTime;

	std::wstring Utf8ToWide(const std::string& value)
	{
		if (value.empty())
			return std::wstring();

		const int required = MultiByteToWideChar(
			CP_UTF8, MB_ERR_INVALID_CHARS, value.c_str(), -1, nullptr, 0);
		if (required <= 1)
			return std::wstring();

		std::wstring result(static_cast<size_t>(required), L'\0');
		if (MultiByteToWideChar(
			CP_UTF8, MB_ERR_INVALID_CHARS, value.c_str(), -1,
			&result[0], required) <= 1)
			return std::wstring();
		result.resize(static_cast<size_t>(required - 1));
		return result;
	}

	std::wstring EscapeXmlText(const std::wstring& value)
	{
		std::wstring escaped;
		for (const wchar_t character : value)
		{
			switch (character)
			{
			case L'&': escaped += L"&amp;"; break;
			case L'<': escaped += L"&lt;"; break;
			case L'>': escaped += L"&gt;"; break;
			default: escaped += character; break;
			}
		}
		return escaped;
	}

	std::wstring FormatLastOpenedTime(long lastPlayed)
	{
		if (lastPlayed <= 0)
			return std::wstring();

		const std::time_t rawTime = static_cast<std::time_t>(lastPlayed);
		tm localTime = {};
		if (localtime_s(&localTime, &rawTime) != 0)
			return std::wstring();

		std::wostringstream formatted;
		formatted << (localTime.tm_year + 1900) << L'-';
		if (localTime.tm_mon + 1 < 10)
			formatted << L'0';
		formatted << (localTime.tm_mon + 1) << L'-';
		if (localTime.tm_mday < 10)
			formatted << L'0';
		formatted << localTime.tm_mday << L' ';
		if (localTime.tm_hour < 10)
			formatted << L'0';
		formatted << localTime.tm_hour << L':';
		if (localTime.tm_min < 10)
			formatted << L'0';
		formatted << localTime.tm_min;
		return formatted.str();
	}

	void RefreshLatestWorld()
	{
		g_latestWorldName.clear();
		g_latestWorldTime.clear();

		const std::string worldsRoot =
			UwpStoragePaths::localFolderPathUtf8() +
			"\\games\\com.mojang\\minecraftWorlds";
		long latestLastPlayed = 0;
		for (const std::string& folderName :
			UwpStoragePaths::enumerateWorldFolderNamesBlocking())
		{
			LevelData data;
			const std::string worldPath = worldsRoot + "\\" + folderName;
			if (!ExternalFileLevelStorage::readLevelData(worldPath, data))
				continue;

			const long lastPlayed = data.getLastPlayed();
			if (lastPlayed <= latestLastPlayed)
				continue;

			const std::wstring worldName = Utf8ToWide(data.getLevelName());
			const std::wstring worldTime = FormatLastOpenedTime(lastPlayed);
			if (worldName.empty() || worldTime.empty())
				continue;

			latestLastPlayed = lastPlayed;
			g_latestWorldName = worldName;
			g_latestWorldTime = worldTime;
		}
	}

	XmlDocument^ CreateTileXml(
		const wchar_t* squareImagePath,
		const wchar_t* wideImagePath)
	{
		auto xml = ref new XmlDocument();
		xml->LoadXml(ref new String(
			L"<tile><visual version=\"2\">"
			L"<binding template=\"TileSquare150x150Image\">"
			L"<image id=\"1\" src=\"\"/>"
			L"</binding>"
			// Use a dedicated 310x150 source for the legacy wide tile.  This
			// avoids the WP shell treating a square image as a full-bleed wide
			// image and cropping it to the top half.
			L"<binding template=\"TileWide310x150Image\">"
			L"<image id=\"1\" src=\"\"/>"
			L"</binding>"
			L"</visual></tile>"));

		auto images = xml->GetElementsByTagName(ref new String(L"image"));
		for (unsigned int i = 0; i < images->Length; ++i)
		{
			const wchar_t* imagePath = i == 0
				? squareImagePath
				: wideImagePath;
			auto image = images->GetAt(i);
			if (image != nullptr && image->Attributes != nullptr)
			{
				auto source = image->Attributes->GetNamedItem(
					ref new String(L"src"));
				if (source != nullptr)
					source->NodeValue = ref new String(imagePath);
			}
		}
		return xml;
	}

	XmlDocument^ CreateWorldCountTileXml()
	{
		std::wstring worldCountText;
		if (g_worldCount == 0)
			worldCountText = L"There are no saves yet";
		else if (g_worldCount == 1)
			worldCountText = L"There is 1 save";
		else
			worldCountText = std::wstring(L"Currently there are ") +
				std::to_wstring(g_worldCount) +
				L" saves";
		if (!g_latestWorldName.empty() && !g_latestWorldTime.empty())
		{
			worldCountText += L"\nLast opened: ";
			worldCountText += EscapeXmlText(g_latestWorldName);
			worldCountText += L"\n";
			worldCountText += g_latestWorldTime;
		}
		std::wstring xmlText =
			L"<tile><visual version=\"2\">"
			L"<binding template=\"TileSquare150x150Text04\">"
			L"<text id=\"1\">";
		xmlText += worldCountText;
		xmlText +=
			L"</text></binding>"
			L"<binding template=\"TileWide310x150Text04\">"
			L"<text id=\"1\">";
		xmlText += worldCountText;
		xmlText += L"</text></binding></visual></tile>";

		auto xml = ref new XmlDocument();
		xml->LoadXml(ref new String(xmlText.c_str()));
		return xml;
	}

	void QueueTileNotifications()
	{
		try
		{
			auto updater = TileUpdateManager::CreateTileUpdaterForApplication();
			if (updater == nullptr)
				return;

			const int tileMode = g_tileMode < 0 ? 0 : (g_tileMode > 2 ? 2 : g_tileMode);
			updater->EnableNotificationQueue(tileMode != 0);
			updater->Clear();
			if (tileMode == 0)
				return;

			// Windows rotates the queued notifications while the tile is pinned.
			// Both images are packaged as application resources, so this also works
			// when the game is installed on Windows 10 Mobile.
			struct TileImagePaths
			{
				const wchar_t* square;
				const wchar_t* wide;
			};

			static const TileImagePaths imagePaths[] =
			{
				{
					L"ms-appx:///Assets/Metro_Grass_Square.png",
					L"ms-appx:///Assets/Metro_Grass_Wide.png"
				},
				{
					L"ms-appx:///Assets/Metro_Crafting_Table_Square.png",
					L"ms-appx:///Assets/Metro_Crafting_Table_Wide.png"
				}
			};

			const unsigned int imageCount = tileMode == 1
				? 1
				: static_cast<unsigned int>(sizeof(imagePaths) / sizeof(imagePaths[0]));
			for (unsigned int i = 0; i < imageCount; ++i)
			{
				auto notification = ref new TileNotification(
					CreateTileXml(imagePaths[i].square, imagePaths[i].wide));
				updater->Update(notification);
			}

			if (tileMode == 2)
			{
				// The third queued notification is a text-only page showing the
				// current number of saved worlds.
				updater->Update(ref new TileNotification(CreateWorldCountTileXml()));
			}
		}
		catch (Exception^)
		{
			// Live tiles are optional.  A device or shell that does not expose the
			// updater must not prevent the game from starting or changing options.
		}
	}
}

void UwpLiveTileSetMode(int mode)
{
	g_tileMode = mode < 0 ? 0 : (mode > 2 ? 2 : mode);
	g_worldCount = UwpStoragePaths::countValidWorldsBlocking();
	RefreshLatestWorld();
	QueueTileNotifications();
}

void UwpLiveTileSetEnabled(bool enabled)
{
	UwpLiveTileSetMode(enabled ? 2 : 0);
}

void UwpLiveTileWorldsChanged()
{
	g_worldCount = UwpStoragePaths::countValidWorldsBlocking();
	RefreshLatestWorld();
	QueueTileNotifications();
}

unsigned int UwpLiveTileGetWorldCount()
{
	return g_worldCount;
}
