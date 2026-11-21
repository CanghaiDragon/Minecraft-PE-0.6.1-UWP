#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "AppPlatform_uwp.h"
#include "client/renderer/gles.h"

#include <windows.h>
#include <windows.applicationmodel.h>
#include <windows.devices.haptics.h>
#include <windows.foundation.metadata.h>
#include <windows.graphics.display.h>
#include <windows.system.h>
#include <windows.system.profile.h>
#include <windows.system.userprofile.h>
#include <windows.storage.h>
#include <windows.ui.viewmanagement.h>

// Windows.Phone.Devices.Notification belongs to the optional Windows Mobile
// Extension SDK.  Keep the phone-specific path when that SDK is installed,
// but do not make the desktop/UWP build depend on its header.
#if defined(__has_include)
#if __has_include(<windows.phone.devices.notification.h>)
#include <windows.phone.devices.notification.h>
#define MCPE_HAS_PHONE_VIBRATION_API 1
#endif
#endif

#include <cstdio>
#include <cstdint>
#include <ctime>
#include <cwchar>
#include <limits>
#include <ppltasks.h>
#include <string>
#include <utility>
#include <zlib.h>

using namespace Platform;
using namespace Concurrency;
using namespace Windows::ApplicationModel;
using namespace Windows::Foundation;
using namespace Windows::Storage;
using namespace Windows::Devices::Haptics;
using namespace Windows::Graphics::Display;
using namespace Windows::System::UserProfile;
using namespace Windows::System;
using namespace Windows::Foundation::Metadata;
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

	void appendBigEndian32(std::vector<unsigned char>& destination,
		std::uint32_t value)
	{
		destination.push_back(static_cast<unsigned char>((value >> 24) & 0xff));
		destination.push_back(static_cast<unsigned char>((value >> 16) & 0xff));
		destination.push_back(static_cast<unsigned char>((value >> 8) & 0xff));
		destination.push_back(static_cast<unsigned char>(value & 0xff));
	}

	void appendPngChunk(std::vector<unsigned char>& png, const char type[4],
		const unsigned char* data, std::uint32_t length)
	{
		appendBigEndian32(png, length);
		const size_t crcStart = png.size();
		for (int i = 0; i < 4; ++i)
			png.push_back(static_cast<unsigned char>(type[i]));
		if (length > 0)
			png.insert(png.end(), data, data + length);
		const uLong crc = crc32(0L, png.data() + crcStart,
			static_cast<uInt>(4u + length));
		appendBigEndian32(png, static_cast<std::uint32_t>(crc));
	}
}

AppPlatform_uwp::AppPlatform_uwp(int screenWidth, int screenHeight)
	: m_screenWidth(screenWidth),
	  m_screenHeight(screenHeight),
	  m_debugScreenshotResult(0),
	  m_debugScreenshotPending(false)
{
}

bool AppPlatform_uwp::saveDebugScreenshot(const std::string& filename,
	int glWidth, int glHeight)
{
	if (!isSafeAssetName(filename) || glWidth <= 0 || glHeight <= 0)
		return false;
	if (m_debugScreenshotPending.exchange(true))
		return false;

	const std::uint64_t pixelCount = static_cast<std::uint64_t>(glWidth) *
		static_cast<std::uint64_t>(glHeight);
	if (pixelCount > std::numeric_limits<std::uint32_t>::max() / 4u)
	{
		m_debugScreenshotPending.store(false);
		return false;
	}
	const std::uint64_t rawRowBytes = static_cast<std::uint64_t>(glWidth) * 3u + 1u;
	const std::uint64_t rawBytes = rawRowBytes * glHeight;
	if (rawBytes > std::numeric_limits<uLong>::max() ||
		rawBytes > std::numeric_limits<size_t>::max())
	{
		m_debugScreenshotPending.store(false);
		return false;
	}

	try
	{
		std::vector<unsigned char> rgba(static_cast<size_t>(pixelCount) * 4u);
		GLint previousPackAlignment = 4;
		glGetIntegerv(GL_PACK_ALIGNMENT, &previousPackAlignment);
		glPixelStorei(GL_PACK_ALIGNMENT, 1);
		glReadPixels(0, 0, glWidth, glHeight, GL_RGBA, GL_UNSIGNED_BYTE,
			rgba.data());
		glPixelStorei(GL_PACK_ALIGNMENT, previousPackAlignment);

		std::vector<unsigned char> raw(static_cast<size_t>(rawBytes));
		for (int y = 0; y < glHeight; ++y)
		{
			const int sourceY = glHeight - 1 - y;
			const size_t rowStart = static_cast<size_t>(y) * rawRowBytes;
			raw[rowStart] = 0; // PNG filter: None
			for (int x = 0; x < glWidth; ++x)
			{
				const size_t source =
					(static_cast<size_t>(sourceY) * glWidth + x) * 4u;
				const size_t destination = rowStart + 1u +
					static_cast<size_t>(x) * 3u;
				raw[destination] = rgba[source];
				raw[destination + 1] = rgba[source + 1];
				raw[destination + 2] = rgba[source + 2];
			}
		}

		uLongf compressedSize = compressBound(static_cast<uLong>(raw.size()));
		std::vector<unsigned char> compressed(compressedSize);
		if (compress2(compressed.data(), &compressedSize, raw.data(),
			static_cast<uLong>(raw.size()), Z_BEST_SPEED) != Z_OK)
		{
			m_debugScreenshotPending.store(false);
			return false;
		}
		compressed.resize(compressedSize);

		std::vector<unsigned char> png;
		static const unsigned char signature[8] =
			{ 137, 80, 78, 71, 13, 10, 26, 10 };
		png.insert(png.end(), signature, signature + sizeof(signature));
		unsigned char ihdr[13] = {};
		ihdr[0] = static_cast<unsigned char>((glWidth >> 24) & 0xff);
		ihdr[1] = static_cast<unsigned char>((glWidth >> 16) & 0xff);
		ihdr[2] = static_cast<unsigned char>((glWidth >> 8) & 0xff);
		ihdr[3] = static_cast<unsigned char>(glWidth & 0xff);
		ihdr[4] = static_cast<unsigned char>((glHeight >> 24) & 0xff);
		ihdr[5] = static_cast<unsigned char>((glHeight >> 16) & 0xff);
		ihdr[6] = static_cast<unsigned char>((glHeight >> 8) & 0xff);
		ihdr[7] = static_cast<unsigned char>(glHeight & 0xff);
		ihdr[8] = 8; // bit depth
		ihdr[9] = 2; // RGB
		appendPngChunk(png, "IHDR", ihdr, sizeof(ihdr));
		appendPngChunk(png, "IDAT", compressed.data(),
			static_cast<std::uint32_t>(compressed.size()));
		appendPngChunk(png, "IEND", nullptr, 0);

		std::wstring requestedName = widenAsciiPath(filename);
		const size_t extensionPos = requestedName.find_last_of(L'.');
		if (extensionPos != std::wstring::npos)
			requestedName.erase(extensionPos);
		requestedName += L".png";
		Array<unsigned char>^ pngBytes = ref new Array<unsigned char>(
			png.data(), static_cast<unsigned int>(png.size()));
		m_debugScreenshotResult.store(0);
		create_task(KnownFolders::PicturesLibrary->CreateFolderAsync(
			ref new String(L"MinecraftPE"), CreationCollisionOption::OpenIfExists))
			.then([requestedName, pngBytes](StorageFolder^ screenshotFolder)
			{
				return create_task(screenshotFolder->CreateFileAsync(
					ref new String(requestedName.c_str()),
					CreationCollisionOption::GenerateUniqueName))
					.then([pngBytes](StorageFile^ file)
					{
						return create_task(FileIO::WriteBytesAsync(file, pngBytes))
							.then([file](task<void> writeTask)
							{
								try
								{
									writeTask.get();
								}
								catch (...)
								{
									create_task(file->DeleteAsync()).then(
										[](task<void> deleteTask)
										{
											try { deleteTask.get(); }
											catch (...) {}
										});
									throw;
								}
							});
					});
			})
			.then([this](task<void> completed)
			{
				try
				{
					completed.get();
					m_debugScreenshotResult.store(1);
				}
				catch (...)
				{
					m_debugScreenshotResult.store(-1);
				}
				m_debugScreenshotPending.store(false);
			});
		return true;
	}
	catch (...)
	{
		m_debugScreenshotPending.store(false);
		return false;
	}
}

int AppPlatform_uwp::consumeDebugScreenshotResult()
{
	return m_debugScreenshotResult.exchange(0);
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

std::string AppPlatform_uwp::getLanguageCode()
{
	try
	{
		String^ language = GlobalizationPreferences::Languages->GetAt(0);
		if (language != nullptr && language->Length() >= 2 &&
			language->Data()[0] == L'z' && language->Data()[1] == L'h')
			return "zh_CN";
	}
	catch (...)
	{
		// Keep the game usable if Windows does not provide a language preference.
	}
	return "en_US";
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

bool AppPlatform_uwp::isMobileDevice()
{
	try
	{
		::Platform::String^ family =
			Windows::System::Profile::AnalyticsInfo::VersionInfo->DeviceFamily;
		return family != nullptr &&
			std::wcscmp(family->Data(), L"Windows.Mobile") == 0;
	}
	catch (...)
	{
		return false;
	}
}

std::string AppPlatform_uwp::getPlatformStringVar(int stringId)
{
	if (stringId == PlatformStringVars::DEVICE_BUILD_MODEL)
		return "UWP";
	return AppPlatform::getPlatformStringVar(stringId);
}

std::string AppPlatform_uwp::getDateString(int seconds)
{
	if (seconds <= 0)
		return "Unknown";

	const time_t rawTime = static_cast<time_t>(seconds);
	tm localTime = {};
	if (localtime_s(&localTime, &rawTime) != 0)
		return "Unknown";

	char date[32] = {};
	if (strftime(date, sizeof(date), "%Y-%m-%d", &localTime) == 0)
		return "Unknown";

	return date;
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

void AppPlatform_uwp::vibrate(int milliSeconds)
{
	if (milliSeconds <= 0)
		return;

	// Windows.Devices.Haptics is part of the universal contract from Windows
	// 10.0.15063 onward. Older Windows 10 versions and devices without a
	// vibration motor simply take the no-op path below.
	try
	{
#if defined(MCPE_HAS_PHONE_VIBRATION_API)
		// Windows 10 Mobile exposes the phone motor through the phone-specific
		// notification API. Do not gate this path with the generic runtime type
		// check: the Mobile Extension SDK and the Windows.Mobile device family
		// already identify the supported target, while the generic haptics API
		// does not reliably drive the phone motor.
		if (isMobileDevice())
		{
			Windows::Phone::Devices::Notification::VibrationDevice^ phoneDevice =
				Windows::Phone::Devices::Notification::VibrationDevice::GetDefault();
			if (phoneDevice == nullptr)
				return;

			TimeSpan duration;
			duration.Duration = static_cast<long long>(milliSeconds) * 10000;
			phoneDevice->Vibrate(duration);
			return;
		}
#endif

		if (!ApiInformation::IsTypePresent(
			L"Windows.Devices.Haptics.VibrationDevice"))
			return;

		// GetDefaultAsync is intentionally kept asynchronous. Block destruction
		// runs in the game loop, so waiting for WinRT device discovery here would
		// introduce a frame stall on the first vibration.
		create_task(VibrationDevice::GetDefaultAsync()).then(
			[milliSeconds](VibrationDevice^ device)
			{
				if (device == nullptr || device->SimpleHapticsController == nullptr)
					return;

				SimpleHapticsController^ controller = device->SimpleHapticsController;
				if (controller->SupportedFeedback == nullptr
					|| controller->SupportedFeedback->Size == 0)
					return;

				SimpleHapticsControllerFeedback^ feedback =
					controller->SupportedFeedback->GetAt(0);
				if (feedback == nullptr)
					return;

				TimeSpan duration;
				duration.Duration = static_cast<long long>(milliSeconds) * 10000;
				if (controller->IsPlayDurationSupported)
					controller->SendHapticFeedbackForDuration(feedback, 1.0, duration);
				else
					controller->SendHapticFeedback(feedback, 1.0);
			})
			.then([](task<void> completed)
			{
				try
				{
					completed.get();
				}
				catch (...)
				{
					// Haptics are optional and must never interrupt gameplay.
				}
			});
	}
	catch (...)
	{
		// A missing haptics contract, device, or permission must not affect block
		// destruction or the game loop.
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
