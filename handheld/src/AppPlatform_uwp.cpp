#include "AppPlatform_uwp.h"

#include <windows.applicationmodel.h>
#include <windows.graphics.display.h>
#include <windows.system.h>
#include <windows.ui.viewmanagement.h>

#include <cstdio>
#include <string>
#include <utility>

using namespace Platform;
using namespace Windows::ApplicationModel;
using namespace Windows::Foundation;
using namespace Windows::Graphics::Display;
using namespace Windows::System;
using namespace Windows::UI::ViewManagement;

namespace
{
	bool isSafeAssetName(const std::string& filename)
	{
		return !filename.empty()
			&& filename[0] != '\\'
			&& filename[0] != '/'
			&& filename.find("..") == std::string::npos
			&& filename.find(':') == std::string::npos;
	}

	std::wstring widenAsciiPath(const std::string& value)
	{
		std::wstring result;
		result.reserve(value.size());
		for (unsigned char ch : value)
			result.push_back(static_cast<wchar_t>(ch));
		return result;
	}

	std::string textureKey(const std::string& filename, bool textureFolder)
	{
		return (textureFolder ? "images/" : "") + filename;
	}
}

AppPlatform_uwp::AppPlatform_uwp(int screenWidth, int screenHeight)
	: m_screenWidth(screenWidth),
	  m_screenHeight(screenHeight)
{
}

BinaryBlob AppPlatform_uwp::readAssetFile(const std::string& filename)
{
	if (!isSafeAssetName(filename))
		return BinaryBlob();

	try
	{
		// MCPE's original platform API passes paths relative to data/, such as
		// lang/en_US.lang.  Packaged UWP assets are read-only but available
		// through InstalledLocation without introducing a separate resource
		// format or copying them into local storage.
		std::wstring path = Package::Current->InstalledLocation->Path->Data();
		path += L"\\data\\";
		path += widenAsciiPath(filename);
		for (wchar_t& ch : path)
			if (ch == L'/') ch = L'\\';

		FILE* file = nullptr;
		if (_wfopen_s(&file, path.c_str(), L"rb") != 0 || file == nullptr)
			return BinaryBlob();

		if (std::fseek(file, 0, SEEK_END) != 0)
		{
			std::fclose(file);
			return BinaryBlob();
		}
		const long size = std::ftell(file);
		if (size <= 0 || std::fseek(file, 0, SEEK_SET) != 0)
		{
			std::fclose(file);
			return BinaryBlob();
		}

		unsigned char* bytes = new unsigned char[static_cast<size_t>(size)];
		const size_t read = std::fread(bytes, 1, static_cast<size_t>(size), file);
		std::fclose(file);
		if (read != static_cast<size_t>(size))
		{
			delete[] bytes;
			return BinaryBlob();
		}
		return BinaryBlob(bytes, static_cast<int>(size));
	}
	catch (...)
	{
		return BinaryBlob();
	}
}

TextureData AppPlatform_uwp::loadTexture(const std::string& filename, bool textureFolder)
{
	TextureData texture;
	m_lastTextureLoadError.clear();
	if (!isSafeAssetName(filename))
	{
		m_lastTextureLoadError = "invalid resource name";
		return texture;
	}

	const std::string key = textureKey(filename, textureFolder);
	const auto cached = m_textureCache.find(key);
	if (cached == m_textureCache.end())
	{
		const auto failure = m_texturePreloadFailures.find(key);
		m_lastTextureLoadError = failure == m_texturePreloadFailures.end()
			? "texture has not been preloaded"
			: failure->second;
		return TextureData();
	}

	// Original Textures owns the returned allocation.  The UWP side owns only
	// the cached decoded source pixels, populated asynchronously before client
	// initialization, so this synchronous contract never waits on an STA task.
	const CachedTexture& source = cached->second;
	texture.data = new unsigned char[source.pixels.size()];
	std::memcpy(texture.data, source.pixels.data(), source.pixels.size());
	texture.w = static_cast<int>(source.width);
	texture.h = static_cast<int>(source.height);
	texture.numBytes = static_cast<int>(source.pixels.size());
	texture.format = TEXF_UNCOMPRESSED_8888;
	texture.transparent = source.transparent;
	texture.memoryHandledExternally = false;
	return texture;
}

int AppPlatform_uwp::getScreenWidth()
{
	return m_screenWidth;
}

int AppPlatform_uwp::getScreenHeight()
{
	return m_screenHeight;
}

float AppPlatform_uwp::getPixelsPerMillimeter()
{
	try
	{
		return DisplayInformation::GetForCurrentView()->LogicalDpi / 25.4f;
	}
	catch (...)
	{
		return 96.0f / 25.4f;
	}
}

bool AppPlatform_uwp::supportsTouchscreen()
{
	return true;
}

std::string AppPlatform_uwp::getPlatformStringVar(int stringId)
{
	if (stringId == PlatformStringVars::DEVICE_BUILD_MODEL)
		return "UWP";
	return AppPlatform::getPlatformStringVar(stringId);
}

std::string AppPlatform_uwp::getDateString(int seconds)
{
	return std::to_string(seconds) + " s (UTC)";
}

void AppPlatform_uwp::openUrl(const std::string& url)
{
	if (url.empty())
		return;
	try
	{
		// The original call is fire-and-forget.  LaunchUriAsync starts activation
		// without making this synchronous platform adapter wait for its result.
		Launcher::LaunchUriAsync(ref new Uri(ref new String(widenAsciiPath(url).c_str())));
	}
	catch (...)
	{
	}
}

void AppPlatform_uwp::showKeyboard()
{
	AppPlatform::showKeyboard();
	try
	{
		InputPane::GetForCurrentView()->TryShow();
	}
	catch (...)
	{
	}
}

void AppPlatform_uwp::hideKeyboard()
{
	AppPlatform::hideKeyboard();
	try
	{
		InputPane::GetForCurrentView()->TryHide();
	}
	catch (...)
	{
	}
}

void AppPlatform_uwp::setScreenSize(int screenWidth, int screenHeight)
{
	m_screenWidth = screenWidth;
	m_screenHeight = screenHeight;
}

void AppPlatform_uwp::cacheTexturePixels(const std::string& filename, bool textureFolder,
	unsigned int width, unsigned int height, const unsigned char* pixels, unsigned int byteCount)
{
	const std::string key = textureKey(filename, textureFolder);
	if (!isSafeAssetName(filename) || width == 0 || height == 0 || pixels == nullptr
		|| byteCount != width * height * 4)
	{
		setTexturePreloadFailure(filename, textureFolder, "invalid preloaded pixel buffer");
		return;
	}

	CachedTexture texture;
	texture.width = width;
	texture.height = height;
	texture.transparent = false;
	texture.pixels.assign(pixels, pixels + byteCount);
	for (unsigned int index = 3; index < byteCount; index += 4)
	{
		if (texture.pixels[index] != 0xff)
		{
			texture.transparent = true;
			break;
		}
	}
	m_texturePreloadFailures.erase(key);
	m_textureCache[key] = std::move(texture);
}

void AppPlatform_uwp::setTexturePreloadFailure(const std::string& filename, bool textureFolder,
	const std::string& reason)
{
	const std::string key = textureKey(filename, textureFolder);
	m_textureCache.erase(key);
	m_texturePreloadFailures[key] = reason;
}

void AppPlatform_uwp::clearTexturePreload(const std::string& filename, bool textureFolder)
{
	const std::string key = textureKey(filename, textureFolder);
	m_textureCache.erase(key);
	m_texturePreloadFailures.erase(key);
}

const std::string& AppPlatform_uwp::lastTextureLoadError() const
{
	return m_lastTextureLoadError;
}

// The original Minecraft client exposes these Win32 input hooks from its
// desktop entry point.  UWP input is fed directly from CoreWindow/XAML by
// MainPage, so the hooks remain valid platform seams while the original
// client dependency object is still lifecycle-only.
void setWin32TouchInputEnabled(bool)
{
}

void setWin32GuiInputEnabled(bool)
{
}
