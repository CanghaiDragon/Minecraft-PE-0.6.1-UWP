#include "pch.h"
#include "UwpLiveTile.h"
#include "UwpStoragePaths.h"

#include <windows.data.xml.dom.h>
#include <windows.ui.notifications.h>
#include <string>

using namespace Platform;
using namespace Windows::Data::Xml::Dom;
using namespace Windows::UI::Notifications;

namespace
{
	int g_tileMode = 0;
	unsigned int g_worldCount = 0;

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
		const std::wstring worldCountText = g_worldCount == 0
			? std::wstring(L"There are no saves yet")
			: std::wstring(L"Currently there are ") +
				std::to_wstring(g_worldCount) +
				L" saves";
		std::wstring xmlText =
			L"<tile><visual version=\"2\">"
			L"<binding template=\"TileSquare150x150Text04\">"
			L"<text id=\"1\">";
		xmlText += worldCountText;
		xmlText +=
			L"</text></binding>"
			L"<binding template=\"TileWide310x150Text03\">"
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
	QueueTileNotifications();
}

void UwpLiveTileSetEnabled(bool enabled)
{
	UwpLiveTileSetMode(enabled ? 2 : 0);
}

void UwpLiveTileWorldsChanged()
{
	g_worldCount = UwpStoragePaths::countValidWorldsBlocking();
	QueueTileNotifications();
}

unsigned int UwpLiveTileGetWorldCount()
{
	return g_worldCount;
}
