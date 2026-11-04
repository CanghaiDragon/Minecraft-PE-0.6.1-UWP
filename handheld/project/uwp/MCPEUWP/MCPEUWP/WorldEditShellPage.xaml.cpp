#include "pch.h"
#include "WorldEditShellPage.xaml.h"

#include "../../../../src/platform/uwp/UwpLiveTile.h"
#include "../../../../src/platform/uwp/UwpStoragePaths.h"
#include "../../../../src/world/level/LevelSettings.h"
#include "../../../../src/world/level/storage/ExternalFileLevelStorage.h"
#include "../../../../src/world/level/storage/LevelData.h"

#include <windows.h>
#include <windows.storage.h>
#include <windows.storage.pickers.h>
#include <windows.storage.streams.h>
#include <windows.ui.core.h>
#include <windows.ui.startscreen.h>
#include <windows.ui.xaml.controls.h>
#include <windows.ui.xaml.media.h>
#include <windows.ui.xaml.media.animation.h>
#include <ppltasks.h>
#include <cstdio>
#include <cstdint>
#include <ctime>
#include <cwchar>
#include <iomanip>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <vector>
#include <zlib.h>

using namespace MCPEUWP;
using namespace concurrency;
using namespace Windows::Storage;
using namespace Windows::Storage::Pickers;
using namespace Windows::UI::StartScreen;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Media::Animation;

namespace
{
	std::wstring FormatLastOpened(long seconds)
	{
		if (seconds <= 0)
			return L"Unavailable";

		const time_t rawTime = static_cast<time_t>(seconds);
		tm localTime = {};
		if (localtime_s(&localTime, &rawTime) != 0)
			return L"Unavailable";

		wchar_t date[32] = {};
		if (wcsftime(date, sizeof(date) / sizeof(*date), L"%Y-%m-%d %H:%M:%S", &localTime) == 0)
			return L"Unavailable";

		return date;
	}

}

namespace
{
	std::string WideToUtf8(const wchar_t* value)
	{
		if (value == nullptr || *value == L'\0')
			return std::string();
		const int required = WideCharToMultiByte(
			CP_UTF8, WC_ERR_INVALID_CHARS, value, -1, nullptr, 0, nullptr, nullptr);
		if (required <= 1)
			return std::string();
		std::string result(static_cast<size_t>(required), '\0');
		const int converted = WideCharToMultiByte(
			CP_UTF8, WC_ERR_INVALID_CHARS, value, -1,
			&result[0], required, nullptr, nullptr);
		if (converted <= 1)
			return std::string();
		result.resize(static_cast<size_t>(converted - 1));
		return result;
	}

	std::wstring WorldShortcutTileId(const std::wstring& worldId)
	{
		// Keep the tile ID short and stable even when a world folder contains
		// spaces or punctuation.  The world ID itself remains in Arguments.
		std::uint32_t hash = 2166136261u;
		for (const wchar_t character : worldId)
		{
			hash ^= static_cast<std::uint32_t>(character);
			hash *= 16777619u;
		}
		return std::wstring(L"MCPEWorld_") + std::to_wstring(hash);
	}

	std::wstring Utf8ToWide(const std::string& value)
	{
		if (value.empty())
			return std::wstring();
		const int required = MultiByteToWideChar(
			CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
			static_cast<int>(value.size()), nullptr, 0);
		if (required <= 0)
			return std::wstring(value.begin(), value.end());
		std::wstring result(static_cast<size_t>(required), L'\0');
		MultiByteToWideChar(
			CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
			static_cast<int>(value.size()), &result[0], required);
		return result;
	}

	std::uint64_t DirectorySizeBytes(const std::wstring& directory)
	{
		if (directory.empty())
			return 0;

		std::uint64_t total = 0;
		const std::wstring pattern = directory + L"\\*";
		WIN32_FIND_DATAW data{};
		HANDLE search = FindFirstFileW(pattern.c_str(), &data);
		if (search == INVALID_HANDLE_VALUE)
			return 0;

		do
		{
			if (wcscmp(data.cFileName, L".") == 0 ||
				wcscmp(data.cFileName, L"..") == 0)
				continue;

			const std::wstring child = directory + L"\\" + data.cFileName;
			if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
			{
				// Do not follow reparse points. This prevents junctions or symbolic
				// links from making the size scan leave the world directory.
				if ((data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0)
					total += DirectorySizeBytes(child);
			}
			else
			{
				ULARGE_INTEGER fileSize{};
				fileSize.HighPart = data.nFileSizeHigh;
				fileSize.LowPart = data.nFileSizeLow;
				total += fileSize.QuadPart;
			}
		}
		while (FindNextFileW(search, &data) != 0);

		FindClose(search);
		return total;
	}

	std::wstring GameModeName(int gameType)
	{
		if (gameType == GameType::Creative)
			return L"Creative";
		if (gameType == GameType::Survival)
			return L"Survival";
		return L"Undefined";
	}

	std::wstring WorldTypeName(int worldType)
	{
		if (worldType == WorldType::Infinite)
			return L"Infinite";
		if (worldType == WorldType::Sky)
			return L"Sky / Nether Reactor";
		return L"Old / Finite";
	}

	enum class UwpStorageMarkerKind
	{
		Missing,
		V1Raw,
		V2Zlib,
		Unknown
	};

	UwpStorageMarkerKind ReadUwpStorageMarker(const std::string& directory)
	{
		if (directory.empty())
			return UwpStorageMarkerKind::Missing;

		const std::string filename = directory + "\\uwp_storage.version";
		FILE* file = fopen(filename.c_str(), "rb");
		if (file == nullptr)
			return UwpStorageMarkerKind::Missing;

		char contents[256] = {};
		const size_t bytesRead = fread(contents, 1, sizeof(contents) - 1, file);
		fclose(file);

		const std::string marker(contents, bytesRead);
		if (marker ==
			"MCPEUWP_STORAGE_V2\n"
			"CHUNK_CODEC=zlib\n"
			"ENTITY_STORAGE=per_chunk\n")
		{
			return UwpStorageMarkerKind::V2Zlib;
		}
		if (marker ==
			"MCPEUWP_STORAGE_V1\n"
			"CHUNK_CODEC=raw\n"
			"ENTITY_STORAGE=per_chunk\n")
		{
			return UwpStorageMarkerKind::V1Raw;
		}
		return UwpStorageMarkerKind::Unknown;
	}

	std::wstring WorldTypeDisplayName(
		int worldType,
		UwpStorageMarkerKind marker)
	{
		const bool isNewUwpFormat =
			marker == UwpStorageMarkerKind::V1Raw ||
			marker == UwpStorageMarkerKind::V2Zlib;

		if (worldType == WorldType::Infinite)
			return isNewUwpFormat ? L"Infinite" : L"Infinite Legacy (Unsupported)";
		if (worldType == WorldType::Sky)
			return isNewUwpFormat ? L"Sky" : L"Sky Legacy (Unsupported)";
		return L"Old / Finite";
	}

	struct ZipEntry
	{
		std::string name;
		std::uint32_t crc = 0;
		std::uint32_t compressedSize = 0;
		std::uint32_t uncompressedSize = 0;
		std::uint32_t localHeaderOffset = 0;
	};

	class ZipWriter
	{
	public:
		explicit ZipWriter(const std::wstring& path)
			: m_path(path), m_file(_wfopen(path.c_str(), L"wb"))
		{
		}

		~ZipWriter()
		{
			if (m_file != nullptr)
				fclose(m_file);
		}

		bool valid() const
		{
			return m_file != nullptr;
		}

		bool addFile(const std::string& name,
			const std::vector<unsigned char>& data)
		{
			if (m_file == nullptr || name.empty() || name.size() > 0xffff)
				return false;

			const uLong sourceSize = static_cast<uLong>(data.size());
			const std::uint32_t crc = static_cast<std::uint32_t>(::crc32(
				0L, data.empty() ? Z_NULL : data.data(), sourceSize));
			std::vector<unsigned char> compressed(
				static_cast<size_t>(::compressBound(sourceSize)));
			uLongf compressedSize = static_cast<uLongf>(compressed.size());
			if (::compress2(compressed.data(), &compressedSize,
				data.empty() ? Z_NULL : data.data(), sourceSize,
				Z_DEFAULT_COMPRESSION) != Z_OK)
				return false;
			if (compressedSize > 0xffffffffUL || sourceSize > 0xffffffffUL)
				return false;

			const long offset = ftell(m_file);
			if (offset < 0 || static_cast<unsigned long long>(offset) > 0xffffffffULL)
				return false;
			if (!Write32(0x04034b50) || !Write16(20) || !Write16(0) ||
				!Write16(8) || !Write16(0) || !Write16(0) || !Write32(crc) ||
				!Write32(static_cast<std::uint32_t>(compressedSize)) ||
				!Write32(static_cast<std::uint32_t>(sourceSize)) ||
				!Write16(static_cast<std::uint16_t>(name.size())) || !Write16(0) ||
				!WriteBytes(name.data(), name.size()) ||
				!WriteBytes(compressed.data(), static_cast<size_t>(compressedSize)))
				return false;

			ZipEntry entry;
			entry.name = name;
			entry.crc = crc;
			entry.compressedSize = static_cast<std::uint32_t>(compressedSize);
			entry.uncompressedSize = static_cast<std::uint32_t>(sourceSize);
			entry.localHeaderOffset = static_cast<std::uint32_t>(offset);
			m_entries.push_back(entry);
			return true;
		}

		bool finish()
		{
			if (m_file == nullptr || m_entries.size() > 0xffff)
				return false;

			const long centralOffset = ftell(m_file);
			if (centralOffset < 0 || static_cast<unsigned long long>(centralOffset) > 0xffffffffULL)
				return false;
			for (const ZipEntry& entry : m_entries)
			{
				if (!Write32(0x02014b50) || !Write16(20) || !Write16(20) ||
					!Write16(0) || !Write16(8) || !Write16(0) || !Write16(0) ||
					!Write32(entry.crc) || !Write32(entry.compressedSize) ||
					!Write32(entry.uncompressedSize) ||
					!Write16(static_cast<std::uint16_t>(entry.name.size())) ||
					!Write16(0) || !Write16(0) || !Write16(0) || !Write16(0) ||
					!Write32(0) || !Write32(entry.localHeaderOffset) ||
					!WriteBytes(entry.name.data(), entry.name.size()))
					return false;
			}

			const long endOffset = ftell(m_file);
			if (endOffset < 0 || endOffset < centralOffset ||
				static_cast<unsigned long long>(endOffset) > 0xffffffffULL)
				return false;
			const std::uint32_t centralSize = static_cast<std::uint32_t>(
				endOffset - centralOffset);
			if (!Write32(0x06054b50) || !Write16(0) || !Write16(0) ||
				!Write16(static_cast<std::uint16_t>(m_entries.size())) ||
				!Write16(static_cast<std::uint16_t>(m_entries.size())) ||
				!Write32(centralSize) ||
				!Write32(static_cast<std::uint32_t>(centralOffset)) || !Write16(0))
				return false;

			const bool flushed = fflush(m_file) == 0;
			fclose(m_file);
			m_file = nullptr;
			return flushed;
		}

		void discard()
		{
			if (m_file != nullptr)
			{
				fclose(m_file);
				m_file = nullptr;
			}
			_wremove(m_path.c_str());
		}

	private:
		bool WriteBytes(const void* data, size_t size)
		{
			return size == 0 || fwrite(data, 1, size, m_file) == size;
		}

		bool Write16(std::uint16_t value)
		{
			unsigned char bytes[2] = {
				static_cast<unsigned char>(value & 0xff),
				static_cast<unsigned char>((value >> 8) & 0xff) };
			return WriteBytes(bytes, sizeof(bytes));
		}

		bool Write32(std::uint32_t value)
		{
			unsigned char bytes[4] = {
				static_cast<unsigned char>(value & 0xff),
				static_cast<unsigned char>((value >> 8) & 0xff),
				static_cast<unsigned char>((value >> 16) & 0xff),
				static_cast<unsigned char>((value >> 24) & 0xff) };
			return WriteBytes(bytes, sizeof(bytes));
		}

		std::wstring m_path;
		FILE* m_file = nullptr;
		std::vector<ZipEntry> m_entries;
	};

	task<std::vector<unsigned char>> ReadStorageFileBytes(StorageFile^ file)
	{
		return create_task(file->OpenReadAsync())
			.then([](Windows::Storage::Streams::IRandomAccessStreamWithContentType^ stream)
			{
				if (stream->Size > 0xffffffffULL)
					throw std::runtime_error("file is too large for ZIP export");
				auto buffer = ref new Windows::Storage::Streams::Buffer(
					static_cast<unsigned int>(stream->Size));
				return create_task(stream->ReadAsync(
					buffer, buffer->Capacity,
					Windows::Storage::Streams::InputStreamOptions::None));
			})
			.then([](Windows::Storage::Streams::IBuffer^ buffer)
			{
				std::vector<unsigned char> result(buffer->Length);
				if (!result.empty())
				{
					auto reader = Windows::Storage::Streams::DataReader::FromBuffer(buffer);
					reader->ReadBytes(Platform::ArrayReference<unsigned char>(
						result.data(), result.size()));
				}
				return result;
			});
	}

	task<void> AddFolderToZip(StorageFolder^ source,
		const std::shared_ptr<ZipWriter>& writer,
		const std::string& prefix)
	{
		return create_task(source->GetFilesAsync())
			.then([writer, prefix](Windows::Foundation::Collections::IVectorView<StorageFile^>^ files)
			{
				task<void> filesTask = create_task([]() {});
				for (StorageFile^ file : files)
				{
					filesTask = filesTask.then([writer, prefix, file]()
					{
						return ReadStorageFileBytes(file)
							.then([writer, prefix, file](const std::vector<unsigned char>& data)
							{
								const std::string name = prefix +
									WideToUtf8(file->Name->Data());
								if (!writer->addFile(name, data))
									throw std::runtime_error("could not write ZIP entry");
							});
					});
				}
				return filesTask;
			})
			.then([source]()
			{
				return create_task(source->GetFoldersAsync());
			})
			.then([writer, prefix](Windows::Foundation::Collections::IVectorView<StorageFolder^>^ folders)
			{
				task<void> foldersTask = create_task([]() {});
				for (StorageFolder^ folder : folders)
				{
					foldersTask = foldersTask.then([writer, prefix, folder]()
					{
						return AddFolderToZip(folder, writer,
							prefix + WideToUtf8(folder->Name->Data()) + "/");
					});
				}
				return foldersTask;
			});
	}
}

namespace
{
	void WorldEditShellDiagnostic(const wchar_t* message)
	{
#if defined(_DEBUG)
		OutputDebugStringW(L"[WorldEditDiag] Shell: ");
		OutputDebugStringW(message);
		OutputDebugStringW(L"\n");
#else
		(void)message;
#endif
	}
}

WorldEditShellPage::WorldEditShellPage()
{
	WorldEditShellDiagnostic(L"constructor begin");
	WorldEditShellDiagnostic(L"before InitializeComponent");
	InitializeComponent();
	if (StatusText != nullptr)
		StatusText->Text = ref new Platform::String(L"Editor shell loaded.");
	WorldEditShellDiagnostic(L"after InitializeComponent");
	WorldEditShellDiagnostic(L"constructor end");
}

bool WorldEditShellPage::HandleEscapeKey()
{
	if (WorldTileIconOverlay != nullptr &&
		WorldTileIconOverlay->Visibility == Windows::UI::Xaml::Visibility::Visible)
	{
		HideWorldTileIconPage();
		return true;
	}
	return false;
}

void WorldEditShellPage::SetWorldId(Platform::String^ worldId)
{
	WorldEditShellDiagnostic(L"SetWorldId");
	m_worldId = worldId == nullptr ? ref new Platform::String(L"") : worldId;
	m_selectedTileIcon = nullptr;
	m_pinOperationPending = false;
	if (FolderNameValueText != nullptr)
		FolderNameValueText->Text = m_worldId;
	if (StartMenuIconValueText != nullptr)
		StartMenuIconValueText->Text = ref new Platform::String(L"Not configured");
	if (SizeOnDiskValueText != nullptr)
		SizeOnDiskValueText->Text = ref new Platform::String(L"Unavailable");
	if (StorageVersionValueText != nullptr)
		StorageVersionValueText->Text = ref new Platform::String(L"Unavailable");
	if (WorldIdValueText != nullptr)
		WorldIdValueText->Text = m_worldId->Length() == 0
			? ref new Platform::String(L"Unavailable")
			: m_worldId;
	ScheduleLoadWorldData();
}

void WorldEditShellPage::ShowWorldTileIconPage()
{
	if (WorldTileIconOverlay == nullptr || WorldTileIconFrame == nullptr)
		return;

	if (m_worldTileIconPage == nullptr)
	{
		m_worldTileIconPage = ref new WorldTileIconPage();
		m_worldTileIconPage->BackRequested +=
			ref new Windows::Foundation::EventHandler<Platform::Object^>(
				[this](Platform::Object^, Platform::Object^)
			{
				HideWorldTileIconPage();
			});
		m_worldTileIconPage->IconSelected +=
			ref new Windows::Foundation::EventHandler<Platform::String^>(
				[this](Platform::Object^, Platform::String^ iconName)
			{
				if (m_pinOperationPending)
					return;
				m_selectedTileIcon = iconName;
				if (StartMenuIconValueText != nullptr)
					StartMenuIconValueText->Text = iconName;
				HideWorldTileIconPage();
				PinWorldShortcut(iconName);
			});
	}

	WorldTileIconFrame->Content = m_worldTileIconPage;
	WorldTileIconOverlay->Visibility = Windows::UI::Xaml::Visibility::Visible;
	AnimateWorldTileIconPage(true);
}

void WorldEditShellPage::HideWorldTileIconPage()
{
	AnimateWorldTileIconPage(false);
}

void WorldEditShellPage::AnimateWorldTileIconPage(bool entering)
{
	if (WorldTileIconOverlay == nullptr)
		return;

	if (m_worldTileIconTransition != nullptr)
	{
		m_worldTileIconTransition->Stop();
		if (m_worldTileIconTransitionCompletedToken.Value != 0)
		{
			m_worldTileIconTransition->Completed -=
				m_worldTileIconTransitionCompletedToken;
			m_worldTileIconTransitionCompletedToken.Value = 0;
		}
		m_worldTileIconTransition = nullptr;
	}

	if (entering)
	{
		WorldTileIconOverlay->Visibility =
			Windows::UI::Xaml::Visibility::Visible;
		WorldTileIconOverlay->Opacity = 0.0;
	}

	auto transition = ref new Storyboard();
	auto fade = ref new DoubleAnimation();
	fade->From = entering ? 0.0 : WorldTileIconOverlay->Opacity;
	fade->To = entering ? 1.0 : 0.0;
	Windows::Foundation::TimeSpan duration;
	duration.Duration = entering ? 2200000 : 1800000;
	fade->Duration = duration;
	Storyboard::SetTarget(fade, WorldTileIconOverlay);
	Storyboard::SetTargetProperty(
		fade, ref new Platform::String(L"Opacity"));
	transition->Children->Append(fade);

	m_worldTileIconTransition = transition;
	m_worldTileIconTransitionCompletedToken = transition->Completed +=
		ref new Windows::Foundation::EventHandler<Platform::Object^>(
			[this, transition, entering](Platform::Object^, Platform::Object^)
		{
				if (m_worldTileIconTransition != transition)
					return;

				if (!entering && WorldTileIconOverlay != nullptr)
				{
					WorldTileIconOverlay->Visibility =
						Windows::UI::Xaml::Visibility::Collapsed;
					WorldTileIconOverlay->Opacity = 0.0;
				}
				else if (entering && WorldTileIconOverlay != nullptr)
				{
					WorldTileIconOverlay->Opacity = 1.0;
				}

				if (m_worldTileIconTransitionCompletedToken.Value != 0)
				{
					transition->Completed -=
						m_worldTileIconTransitionCompletedToken;
					m_worldTileIconTransitionCompletedToken.Value = 0;
				}
				m_worldTileIconTransition = nullptr;
			});
	transition->Begin();
}

void WorldEditShellPage::PinWorldShortcut(Platform::String^ iconName)
{
	if (m_pinOperationPending || m_worldId == nullptr ||
		m_worldId->Length() == 0 || iconName == nullptr ||
		iconName->Length() == 0)
		return;

	const std::wstring worldId(m_worldId->Data());
	const std::wstring tileIdValue = WorldShortcutTileId(worldId);
	const std::wstring assetPath =
		L"ms-appx:///Assets/" + std::wstring(iconName->Data());
	const std::wstring argumentValue = L"open-world:" + worldId;

	Platform::String^ tileId =
		ref new Platform::String(tileIdValue.c_str());
	Platform::String^ arguments =
		ref new Platform::String(argumentValue.c_str());
	Platform::String^ displayName = m_worldId;
	if (WorldNameValueText != nullptr && WorldNameValueText->Text != nullptr &&
		WorldNameValueText->Text->Length() > 0 &&
		std::wcscmp(WorldNameValueText->Text->Data(), L"Unavailable") != 0)
	{
		displayName = WorldNameValueText->Text;
	}
	if (displayName == nullptr || displayName->Length() == 0)
		displayName = ref new Platform::String(L"Minecraft World");

	try
	{
		auto logo = ref new Windows::Foundation::Uri(
			ref new Platform::String(assetPath.c_str()));
		const bool alreadyPinned = SecondaryTile::Exists(tileId);
		auto tile = alreadyPinned
			? ref new SecondaryTile(tileId)
			: ref new SecondaryTile(tileId, displayName, arguments, logo,
				TileSize::Default);
		tile->DisplayName = displayName;
		tile->Arguments = arguments;
		tile->VisualElements->Square150x150Logo = logo;
		tile->VisualElements->Wide310x150Logo = logo;
		tile->VisualElements->Square310x310Logo = logo;
		tile->VisualElements->ShowNameOnSquare150x150Logo = true;
		tile->VisualElements->ShowNameOnWide310x150Logo = true;
		tile->VisualElements->ShowNameOnSquare310x310Logo = true;

		m_pinOperationPending = true;
		SetStatus(alreadyPinned
			? L"Updating world shortcut..."
			: L"Pinning world shortcut...");

		// Updating an existing tile does not show a confirmation flyout.  A new
		// tile uses RequestCreateAsync(), which asks the user to confirm the pin.
		// Use the WinRT Completed callbacks here instead of PPL .then().  This
		// project uses an older PPL projection whose continuation constraints are
		// not reliable for these C++/CX async result types.
		Windows::UI::Core::CoreDispatcher^ dispatcher = Dispatcher;
		auto reportResult = [this, dispatcher](int result)
		{
			if (dispatcher == nullptr)
			{
				m_pinOperationPending = false;
				return;
			}
			dispatcher->RunAsync(
				Windows::UI::Core::CoreDispatcherPriority::Normal,
				ref new Windows::UI::Core::DispatchedHandler(
					[this, result]()
				{
						m_pinOperationPending = false;
						SetStatus(result == 0
							? L"World shortcut pinned."
							: result == 1
								? L"Pin request cancelled."
								: L"Could not pin world shortcut.");
					}));
		};
		if (alreadyPinned)
		{
			auto operation = tile->UpdateAsync();
			operation->Completed =
				ref new Windows::Foundation::AsyncOperationCompletedHandler<bool>(
					[reportResult](Windows::Foundation::IAsyncOperation<bool>^ completed,
						Windows::Foundation::AsyncStatus status)
					{
						if (status != Windows::Foundation::AsyncStatus::Completed)
						{
							reportResult(2);
							return;
						}
						try
						{
							reportResult(completed->GetResults() ? 0 : 2);
						}
						catch (...)
						{
							reportResult(2);
						}
					});
		}
		else
		{
			auto operation = tile->RequestCreateAsync();
			operation->Completed =
				ref new Windows::Foundation::AsyncOperationCompletedHandler<bool>(
					[reportResult](Windows::Foundation::IAsyncOperation<bool>^ completed,
						Windows::Foundation::AsyncStatus status)
					{
						if (status != Windows::Foundation::AsyncStatus::Completed)
						{
							reportResult(2);
							return;
						}
						try
						{
							reportResult(completed->GetResults() ? 0 : 1);
						}
						catch (...)
						{
							reportResult(2);
						}
					});
		}
	}
	catch (Platform::Exception^)
	{
		m_pinOperationPending = false;
		SetStatus(L"Could not prepare world shortcut.");
	}
}

void WorldEditShellPage::ScheduleLoadWorldData()
{
	if (m_operationPending || Dispatcher == nullptr)
		return;

	Dispatcher->RunAsync(
		Windows::UI::Core::CoreDispatcherPriority::Low,
		ref new Windows::UI::Core::DispatchedHandler([this]()
		{
			LoadWorldData();
		}));
}

std::string WorldEditShellPage::Utf8(Platform::String^ value) const
{
	return value == nullptr ? std::string() : WideToUtf8(value->Data());
}

std::string WorldEditShellPage::WorldDirectory() const
{
	const std::string root = WideToUtf8(
		UwpStoragePaths::worldsFolderPath()->Data());
	const std::string id = Utf8(m_worldId);
	return root.empty() || id.empty() ? std::string() : root + "\\" + id;
}

void WorldEditShellPage::SetStatus(const std::wstring& text)
{
	if (StatusText != nullptr)
		StatusText->Text = ref new Platform::String(text.c_str());
}

void WorldEditShellPage::ShowSecondaryDialog(
	int kind,
	Platform::String^ title,
	Platform::String^ message,
	Platform::String^ initialText)
{
	m_secondaryDialogKind = kind;
	if (SecondaryDialogTitle != nullptr)
		SecondaryDialogTitle->Text = title == nullptr
			? ref new Platform::String(L"")
			: title;
	if (SecondaryDialogMessage != nullptr)
	{
		SecondaryDialogMessage->Text = message == nullptr
			? ref new Platform::String(L"")
			: message;
		SecondaryDialogMessage->Visibility =
			message == nullptr || message->Length() == 0
				? Windows::UI::Xaml::Visibility::Collapsed
				: Windows::UI::Xaml::Visibility::Visible;
	}
	if (SecondaryDialogTextBox != nullptr)
	{
		SecondaryDialogTextBox->Text = initialText == nullptr
			? ref new Platform::String(L"")
			: initialText;
		SecondaryDialogTextBox->Visibility = kind == 1 || kind == 2
			? Windows::UI::Xaml::Visibility::Visible
			: Windows::UI::Xaml::Visibility::Collapsed;
	}
	if (SecondaryDialogPrimaryButton != nullptr)
		SecondaryDialogPrimaryButton->Content = kind == 3
			? ref new Platform::String(L"Delete")
			: ref new Platform::String(L"Save");
	if (SecondaryDialogOverlay != nullptr)
	{
		SecondaryDialogOverlay->Visibility = Windows::UI::Xaml::Visibility::Visible;
		AnimateSecondaryDialog(true);
	}
}

void WorldEditShellPage::HideSecondaryDialog()
{
	m_secondaryDialogKind = 0;
	if (SecondaryDialogOverlay != nullptr)
		AnimateSecondaryDialog(false);
}

void WorldEditShellPage::AnimateSecondaryDialog(bool entering)
{
	if (SecondaryDialogOverlay == nullptr)
		return;

	if (m_secondaryDialogTransition != nullptr)
	{
		m_secondaryDialogTransition->Stop();
		if (m_secondaryDialogTransitionCompletedToken.Value != 0)
		{
			m_secondaryDialogTransition->Completed -=
				m_secondaryDialogTransitionCompletedToken;
			m_secondaryDialogTransitionCompletedToken.Value = 0;
		}
		m_secondaryDialogTransition = nullptr;
	}

	if (entering)
	{
		SecondaryDialogOverlay->Visibility =
			Windows::UI::Xaml::Visibility::Visible;
		SecondaryDialogOverlay->Opacity = 0.0;
	}

	auto transition = ref new Storyboard();
	auto fade = ref new DoubleAnimation();
	fade->From = entering ? 0.0 : SecondaryDialogOverlay->Opacity;
	fade->To = entering ? 1.0 : 0.0;
	Windows::Foundation::TimeSpan duration;
	duration.Duration = entering ? 2200000 : 1800000;
	fade->Duration = duration;
	Storyboard::SetTarget(fade, SecondaryDialogOverlay);
	Storyboard::SetTargetProperty(
		fade, ref new Platform::String(L"Opacity"));
	transition->Children->Append(fade);

	m_secondaryDialogTransition = transition;
	m_secondaryDialogTransitionCompletedToken = transition->Completed +=
		ref new Windows::Foundation::EventHandler<Platform::Object^>(
			[this, transition, entering](Platform::Object^, Platform::Object^)
		{
				if (m_secondaryDialogTransition == transition)
				{
					if (!entering && SecondaryDialogOverlay != nullptr)
					{
						SecondaryDialogOverlay->Visibility =
							Windows::UI::Xaml::Visibility::Collapsed;
						SecondaryDialogOverlay->Opacity = 0.0;
					}
					else if (entering && SecondaryDialogOverlay != nullptr)
					{
						SecondaryDialogOverlay->Opacity = 1.0;
					}

					if (m_secondaryDialogTransitionCompletedToken.Value != 0)
					{
						transition->Completed -=
							m_secondaryDialogTransitionCompletedToken;
						m_secondaryDialogTransitionCompletedToken.Value = 0;
					}
					m_secondaryDialogTransition = nullptr;
				}
			});
	transition->Begin();
}

void WorldEditShellPage::LoadWorldData()
{
	if (m_worldId == nullptr || m_worldId->Length() == 0)
		return;

	try
	{
		LevelData data;
		if (!ExternalFileLevelStorage::readLevelData(WorldDirectory(), data))
		{
			SetStatus(L"Could not read level.dat.");
			return;
		}

		if (WorldNameValueText != nullptr)
		{
			const std::wstring name = Utf8ToWide(data.levelName);
			WorldNameValueText->Text = ref new Platform::String(name.c_str());
		}

		if (LastOpenedValueText != nullptr)
			LastOpenedValueText->Text = ref new Platform::String(
				FormatLastOpened(data.getLastPlayed()).c_str());

		if (PlayTimeValueText != nullptr)
		{
			std::wstringstream playTime;
			if (data.getTime() >= 0)
				playTime << (static_cast<long long>(data.getTime()) / 24000LL)
					<< L" in-game days";
			else
				playTime << L"Unavailable";
			PlayTimeValueText->Text =
				ref new Platform::String(playTime.str().c_str());
		}

		if (SeedValueText != nullptr)
		{
			std::wstringstream seed;
			seed << data.getSeed();
			SeedValueText->Text = ref new Platform::String(seed.str().c_str());
		}

		if (GameModeValueText != nullptr)
			GameModeValueText->Text = ref new Platform::String(
				GameModeName(data.getGameType()).c_str());
		if (WorldTypeValueText != nullptr)
			WorldTypeValueText->Text = ref new Platform::String(
				WorldTypeName(data.getWorldType()).c_str());

		const UwpStorageMarkerKind marker =
			ReadUwpStorageMarker(WorldDirectory());
		if (WorldTypeValueText != nullptr)
			WorldTypeValueText->Text = ref new Platform::String(
				WorldTypeDisplayName(data.getWorldType(), marker).c_str());

		if (SizeOnDiskValueText != nullptr)
		{
			std::wstringstream sizeOnDisk;
			const std::uint64_t bytes = DirectorySizeBytes(
				Utf8ToWide(WorldDirectory()));
			const double megabytes = static_cast<double>(bytes) /
				(1024.0 * 1024.0);
			if (bytes > 0 && megabytes < 0.01)
				sizeOnDisk << L"<0.01 MB";
			else
				sizeOnDisk << std::fixed << std::setprecision(2)
					<< megabytes << L" MB";
			SizeOnDiskValueText->Text = ref new Platform::String(
				sizeOnDisk.str().c_str());
		}

		if (StorageVersionValueText != nullptr)
		{
			std::wstringstream storageVersion;
			storageVersion << data.getStorageVersion();
			StorageVersionValueText->Text = ref new Platform::String(
				storageVersion.str().c_str());
		}
		SetStatus(L"World information loaded.");
	}
	catch (...)
	{
		SetStatus(L"Could not read level.dat.");
	}
}

void WorldEditShellPage::OnChooseStartIconClick(
	Platform::Object^, Windows::UI::Xaml::RoutedEventArgs^)
{
	if (m_operationPending || m_pinOperationPending)
		return;
	ShowWorldTileIconPage();
}

void WorldEditShellPage::OnChangeFolderNameClick(
	Platform::Object^, Windows::UI::Xaml::RoutedEventArgs^)
{
	if (m_operationPending || m_worldId == nullptr || m_worldId->Length() == 0)
		return;

	m_operationPending = true;
	ShowSecondaryDialog(
		1,
		ref new Platform::String(L"Rename folder"),
		ref new Platform::String(L""),
		m_worldId);
}

void WorldEditShellPage::OnChangeWorldNameClick(
	Platform::Object^, Windows::UI::Xaml::RoutedEventArgs^)
{
	if (m_operationPending || m_worldId == nullptr || m_worldId->Length() == 0)
		return;

	m_operationPending = true;
	ShowSecondaryDialog(
		2,
		ref new Platform::String(L"Rename world"),
		ref new Platform::String(L""),
		WorldNameValueText == nullptr
			? ref new Platform::String(L"")
			: WorldNameValueText->Text);
}

void WorldEditShellPage::OnSecondaryDialogCancelClick(
	Platform::Object^, Windows::UI::Xaml::RoutedEventArgs^)
{
	if (!m_operationPending)
		return;
	HideSecondaryDialog();
	m_operationPending = false;
}

void WorldEditShellPage::OnSecondaryDialogPrimaryClick(
	Platform::Object^, Windows::UI::Xaml::RoutedEventArgs^)
{
	if (!m_operationPending || m_secondaryDialogKind == 0)
		return;

	const int kind = m_secondaryDialogKind;
	Platform::String^ requested = SecondaryDialogTextBox == nullptr
		? nullptr
		: SecondaryDialogTextBox->Text;
	HideSecondaryDialog();

	if (kind == 3)
	{
		Platform::String^ worldName = m_worldId;
		create_task(UwpStoragePaths::ensureWorldsFolderAsync())
			.then([worldName](StorageFolder^ root)
			{
				return create_task(root->GetFolderAsync(worldName));
			})
			.then([](StorageFolder^ folder)
			{
				return create_task(folder->DeleteAsync(StorageDeleteOption::Default));
			})
			.then([this](task<void> result)
			{
				try
				{
					result.get();
					m_operationPending = false;
					SetStatus(L"World deleted.");
					UwpLiveTileWorldsChanged();
					ClosePage();
				}
				catch (Platform::Exception^)
				{
					m_operationPending = false;
					SetStatus(L"Could not delete the world.");
				}
			}, task_continuation_context::use_current());
		return;
	}

	if (requested == nullptr || requested->Length() == 0)
	{
		m_operationPending = false;
		SetStatus(kind == 1
			? L"Folder name cannot be empty."
			: L"World name cannot be empty.");
		return;
	}

	if (kind == 1)
	{
		Platform::String^ oldName = m_worldId;
		if (requested == oldName)
		{
			m_operationPending = false;
			SetStatus(L"The folder name is unchanged.");
			return;
		}

		UwpStoragePaths::ensureWorldsFolderAsync()
			.then([oldName, requested](StorageFolder^ root)
			{
				return create_task(root->GetFolderAsync(oldName))
					.then([requested](StorageFolder^ folder)
					{
						return create_task(folder->RenameAsync(
							requested, NameCollisionOption::FailIfExists));
					});
			})
			.then([this, requested](task<void> renameResult)
			{
				try
				{
					renameResult.get();
					m_worldId = requested;
					if (FolderNameValueText != nullptr)
						FolderNameValueText->Text = requested;
					m_operationPending = false;
					LoadWorldData();
					SetStatus(L"Folder name changed.");
					UwpLiveTileWorldsChanged();
				}
				catch (Platform::Exception^)
				{
					m_operationPending = false;
					SetStatus(L"Could not change the folder name.");
				}
			}, task_continuation_context::use_current());
		return;
	}

	try
	{
		LevelData data;
		if (!ExternalFileLevelStorage::readLevelData(WorldDirectory(), data))
		{
			m_operationPending = false;
			SetStatus(L"Could not read level.dat.");
			return;
		}
		data.setLevelName(Utf8(requested));
		ExternalFileLevelStorage::saveLevelData(WorldDirectory(), data, nullptr);
		if (WorldNameValueText != nullptr)
			WorldNameValueText->Text = requested;
		m_operationPending = false;
		SetStatus(L"World name changed.");
		UwpLiveTileWorldsChanged();
	}
	catch (...)
	{
		m_operationPending = false;
		SetStatus(L"Could not change the world name.");
	}
}

void WorldEditShellPage::OnExportWorldClick(
	Platform::Object^, Windows::UI::Xaml::RoutedEventArgs^)
{
	if (m_operationPending || m_worldId == nullptr || m_worldId->Length() == 0)
		return;

	auto picker = ref new FolderPicker();
	picker->SuggestedStartLocation = PickerLocationId::Desktop;
	picker->FileTypeFilter->Append(L"*");

	m_operationPending = true;
	if (ExportProgressBar != nullptr)
		ExportProgressBar->Visibility =
			Windows::UI::Xaml::Visibility::Visible;
	SetStatus(L"Choose a destination folder...");

	Platform::String^ worldId = m_worldId;
	auto exportCancelled = std::make_shared<bool>(false);
	create_task(picker->PickSingleFolderAsync())
		.then([worldId, exportCancelled](StorageFolder^ destination) -> task<void>
		{
			if (destination == nullptr)
			{
				*exportCancelled = true;
				return task_from_result();
			}

			return UwpStoragePaths::ensureWorldsFolderAsync()
				.then([worldId, destination](StorageFolder^ root)
				{
					return create_task(root->GetFolderAsync(worldId))
						.then([worldId, destination](StorageFolder^ source) -> task<void>
						{
							const std::wstring archiveName =
								std::wstring(worldId->Data()) + L".zip";
							// Write the archive inside the app-owned temporary folder first.
							// A user-selected destination may be brokered by UWP, so its Path
							// is not guaranteed to be writable through stdio/fopen.
							return create_task(
								Windows::Storage::ApplicationData::Current->TemporaryFolder
									->CreateFileAsync(
										L"mcpe_world_export.zip",
										CreationCollisionOption::GenerateUniqueName))
								.then([source, destination, archiveName](StorageFile^ temporaryArchive) -> task<void>
								{
									auto writer = std::make_shared<ZipWriter>(
										temporaryArchive->Path->Data());
									if (!writer->valid())
										throw std::runtime_error("could not create ZIP file");

									return AddFolderToZip(source, writer, "")
										.then([writer, temporaryArchive, destination, archiveName](task<void> exportResult) -> task<void>
										{
											try
											{
												exportResult.get();
												if (!writer->finish())
													throw std::runtime_error(
														"could not finalize ZIP file");
											}
											catch (...)
											{
												writer->discard();
												throw;
											}

											// Only create the visible destination file after the
											// complete ZIP has been finalized successfully.
											return create_task(temporaryArchive->CopyAsync(
												destination,
												ref new Platform::String(archiveName.c_str()),
												NameCollisionOption::GenerateUniqueName))
												.then([temporaryArchive](StorageFile^)
												{
													return create_task(
														temporaryArchive->DeleteAsync());
												});
										});
								});
						});
				});
		})
		.then([this, exportCancelled](task<void> exportResult)
		{
			try
			{
				exportResult.get();
				m_operationPending = false;
				if (ExportProgressBar != nullptr)
					ExportProgressBar->Visibility =
						Windows::UI::Xaml::Visibility::Collapsed;
				SetStatus(*exportCancelled
					? L"Export cancelled."
					: L"World exported as a ZIP archive.");
			}
			catch (...)
			{
				m_operationPending = false;
				if (ExportProgressBar != nullptr)
					ExportProgressBar->Visibility =
						Windows::UI::Xaml::Visibility::Collapsed;
				SetStatus(L"Could not export the world as a ZIP archive.");
			}
		}, task_continuation_context::use_current());
}

void WorldEditShellPage::OnBackClick(
	Platform::Object^, Windows::UI::Xaml::RoutedEventArgs^)
{
	WorldEditShellDiagnostic(L"Back clicked");
	// MainPage subscribes before the page is made visible.  This is the same
	// C++/CX event-raising form used by this page; older UWP compilers do not
	// allow reading an event property for a null check.
	this->BackRequested(this, nullptr);
}

void WorldEditShellPage::OnDeleteWorldClick(
	Platform::Object^, Windows::UI::Xaml::RoutedEventArgs^)
{
	if (m_operationPending || m_worldId == nullptr || m_worldId->Length() == 0)
		return;

	m_operationPending = true;
	ShowSecondaryDialog(
		3,
		ref new Platform::String(L"Delete World?"),
		ref new Platform::String(
			L"This permanently deletes the selected world."),
		ref new Platform::String(L""));
}

void WorldEditShellPage::ClosePage()
{
	this->BackRequested(this, nullptr);
}
