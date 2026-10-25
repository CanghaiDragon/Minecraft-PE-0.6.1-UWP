#include "pch.h"
#include "WorldEditPage.xaml.h"

#include "../../../../src/platform/uwp/UwpStoragePaths.h"
#include "../../../../src/world/level/LevelSettings.h"
#include "../../../../src/world/level/storage/ExternalFileLevelStorage.h"
#include "../../../../src/world/level/storage/LevelData.h"

#include <windows.h>
#include <windows.storage.h>
#include <windows.storage.pickers.h>
#include <windows.storage.streams.h>
#include <windows.ui.xaml.media.h>
#include <ppltasks.h>
#include <zlib.h>
#include <cstdio>
#include <cstdint>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <vector>

using namespace MCPEUWP;
using namespace Platform;
using namespace concurrency;
using namespace Windows::Foundation;
using namespace Windows::Storage;
using namespace Windows::Storage::Pickers;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;

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

	std::wstring WorldTypeName(int type)
	{
		switch (type)
		{
		case WorldType::Infinite: return L"Infinite";
		case WorldType::Sky: return L"Sky / Nether Reactor";
		default: return L"Old / Finite";
		}
	}

	std::wstring GameModeName(int type)
	{
		return type == GameType::Creative ? L"Creative" : L"Survival";
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
		MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
			static_cast<int>(value.size()), &result[0], required);
		return result;
	}

	task<void> CopyFolderContents(StorageFolder^ source, StorageFolder^ destination)
	{
		return create_task(source->GetFilesAsync())
			.then([destination](Windows::Foundation::Collections::IVectorView<StorageFile^>^ files)
			{
				task<void> copyFiles = create_task([]() {});
				for (StorageFile^ file : files)
				{
					copyFiles = copyFiles.then([destination, file]()
					{
						return create_task(file->CopyAsync(
							destination, file->Name,
							NameCollisionOption::ReplaceExisting))
							.then([](StorageFile^)
							{
							});
					});
				}
				return copyFiles;
			})
			.then([source]()
			{
				return create_task(source->GetFoldersAsync());
			})
			.then([source, destination](Windows::Foundation::Collections::IVectorView<StorageFolder^>^ folders)
			{
				task<void> copyFolders = create_task([]() {});
				for (StorageFolder^ folder : folders)
				{
					copyFolders = copyFolders.then([destination, folder]()
					{
						return create_task(destination->CreateFolderAsync(
							folder->Name, CreationCollisionOption::OpenIfExists));
					})
					.then([folder](StorageFolder^ child)
					{
						return CopyFolderContents(folder, child);
					});
				}
				return copyFolders;
			});
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
		explicit ZipWriter(const std::string& path)
			: m_path(path), m_file(fopen(path.c_str(), "wb"))
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
			remove(m_path.c_str());
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

		std::string m_path;
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
					reader->ReadBytes(
						Platform::ArrayReference<unsigned char>(
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

WorldEditPage::WorldEditPage()
{
	InitializeComponent();
	Loaded += ref new RoutedEventHandler(this, &WorldEditPage::OnLoaded);
}

void WorldEditPage::SetWorldId(String^ worldId)
{
	m_worldId = worldId == nullptr ? ref new String(L"") : worldId;
	if (FolderNameTextBox != nullptr)
		FolderNameTextBox->Text = m_worldId;
	// Do not touch the visual tree or synchronously read level.dat while the
	// Page is still being inserted into Frame.  Older UWP XAML versions can
	// surface an unhandled XAML exception during that transition.
	if (m_pageLoaded)
		LoadWorldData();
}

void WorldEditPage::OnLoaded(Object^, RoutedEventArgs^)
{
	m_pageLoaded = true;
	if (FolderNameTextBox != nullptr)
		FolderNameTextBox->Text = m_worldId;
	LoadWorldData();
}

std::string WorldEditPage::Utf8(String^ value) const
{
	return value == nullptr ? std::string() : WideToUtf8(value->Data());
}

std::string WorldEditPage::WorldDirectory() const
{
	const std::string root = WideToUtf8(UwpStoragePaths::worldsFolderPath()->Data());
	const std::string id = Utf8(m_worldId);
	return root.empty() || id.empty() ? std::string() : root + "\\" + id;
}

void WorldEditPage::SetStatus(const std::wstring& text)
{
	if (StatusText != nullptr)
		StatusText->Text = ref new String(text.c_str());
}

void WorldEditPage::ApplyWorldData(const LevelData& data)
{
	const std::wstring wideLevelName = Utf8ToWide(data.levelName);
	if (WorldNameTextBox != nullptr)
		WorldNameTextBox->Text = ref new String(wideLevelName.c_str());
	if (GameModeText != nullptr)
		GameModeText->Text = ref new String(GameModeName(data.getGameType()).c_str());
	if (WorldTypeText != nullptr)
		WorldTypeText->Text = ref new String(WorldTypeName(data.getWorldType()).c_str());

	std::wstringstream playTime;
	if (data.getTime() >= 0)
		playTime << (static_cast<long long>(data.getTime()) / 24000LL)
			<< L" in-game days";
	else
		playTime << L"Unavailable";
	if (PlayTimeText != nullptr)
		PlayTimeText->Text = ref new String(playTime.str().c_str());

	std::wstringstream size;
	if (data.getSizeOnDisk() > 0)
		size << data.getSizeOnDisk() << L" bytes";
	else
		size << L"Unavailable";
	if (StoredSizeText != nullptr)
		StoredSizeText->Text = ref new String(size.str().c_str());
	const std::wstring wideHeader = data.levelName.empty()
		? std::wstring(L"World Edit")
		: Utf8ToWide(data.levelName);
	if (HeaderText != nullptr)
		HeaderText->Text = ref new String(wideHeader.c_str());
}

void WorldEditPage::LoadWorldData()
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
		ApplyWorldData(data);
		SetStatus(L"World information loaded.");
	}
	catch (...)
	{
		SetStatus(L"Could not read level.dat.");
	}
}

void WorldEditPage::OnChangeFolderNameClick(Object^, RoutedEventArgs^)
{
	if (m_operationPending || m_worldId == nullptr)
		return;
	String^ requested = FolderNameTextBox->Text;
	if (requested == nullptr || requested->Length() == 0)
	{
		SetStatus(L"Folder name cannot be empty.");
		return;
	}
	if (requested == m_worldId)
	{
		SetStatus(L"The folder name is unchanged.");
		return;
	}

	m_operationPending = true;
	String^ oldName = m_worldId;
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
		.then([this, requested](task<void> result)
		{
			try
			{
				result.get();
				m_worldId = requested;
				FolderNameTextBox->Text = requested;
				m_operationPending = false;
				LoadWorldData();
				SetStatus(L"Folder name changed.");
			}
			catch (Exception^)
			{
				m_operationPending = false;
				SetStatus(L"Could not change the folder name.");
			}
		}, task_continuation_context::use_current());
}

void WorldEditPage::OnChangeWorldNameClick(Object^, RoutedEventArgs^)
{
	if (m_operationPending || m_worldId == nullptr)
		return;
	String^ requested = WorldNameTextBox->Text;
	if (requested == nullptr || requested->Length() == 0)
	{
		SetStatus(L"World name cannot be empty.");
		return;
	}

	LevelData data;
	if (!ExternalFileLevelStorage::readLevelData(WorldDirectory(), data))
	{
		SetStatus(L"Could not read level.dat.");
		return;
	}
	data.setLevelName(Utf8(requested));
	ExternalFileLevelStorage::saveLevelData(WorldDirectory(), data, nullptr);
	ApplyWorldData(data);
	SetStatus(L"World name changed.");
}

void WorldEditPage::OnDeleteWorldClick(Object^, RoutedEventArgs^)
{
	if (m_operationPending || m_worldId == nullptr)
		return;

	auto dialog = ref new ContentDialog();
	dialog->Title = "Delete World?";
	auto warning = ref new TextBlock();
	warning->Text = "This permanently deletes the selected world.";
	warning->TextWrapping = TextWrapping::Wrap;
	warning->FontFamily = ref new Windows::UI::Xaml::Media::FontFamily(
		L"ms-appx:///data/fonts/minecraft.ttf#Minecraft");
	dialog->Content = warning;
	dialog->PrimaryButtonText = "Delete";
	dialog->CloseButtonText = "Cancel";

	m_operationPending = true;
	String^ worldName = m_worldId;
	create_task(dialog->ShowAsync())
		.then([this, worldName](ContentDialogResult result) -> task<void>
		{
			if (result != ContentDialogResult::Primary)
			{
				m_operationPending = false;
				return task_from_result();
			}
			return UwpStoragePaths::ensureWorldsFolderAsync()
				.then([worldName](StorageFolder^ root)
				{
					return create_task(root->GetFolderAsync(worldName));
				})
				.then([](StorageFolder^ folder)
				{
					return create_task(folder->DeleteAsync(StorageDeleteOption::Default));
				});
		})
		.then([this](task<void> result)
		{
			try
			{
				result.get();
				m_operationPending = false;
				SetStatus(L"World deleted.");
				ClosePage();
			}
			catch (Exception^)
			{
				m_operationPending = false;
				SetStatus(L"Could not delete the world.");
			}
		}, task_continuation_context::use_current());
}

void WorldEditPage::OnExportWorldClick(Object^, RoutedEventArgs^)
{
	if (m_operationPending || m_worldId == nullptr)
		return;

	auto picker = ref new FolderPicker();
	picker->SuggestedStartLocation = PickerLocationId::Desktop;
	picker->FileTypeFilter->Append(L"*");
	m_operationPending = true;
	String^ worldName = m_worldId;
	create_task(picker->PickSingleFolderAsync())
		.then([this, worldName](StorageFolder^ destination) -> task<void>
		{
			if (destination == nullptr)
			{
				m_operationPending = false;
				return task_from_result();
			}

			return UwpStoragePaths::ensureWorldsFolderAsync()
				.then([worldName, destination](StorageFolder^ root)
				{
					return create_task(root->GetFolderAsync(worldName))
						.then([worldName, destination](StorageFolder^ source) -> task<void>
						{
							const std::wstring archiveName =
								std::wstring(worldName->Data()) + L".zip";
							return create_task(destination->CreateFileAsync(
								ref new String(archiveName.c_str()),
								CreationCollisionOption::GenerateUniqueName))
								.then([source](StorageFile^ archive) -> task<void>
								{
									auto writer = std::make_shared<ZipWriter>(
										WideToUtf8(archive->Path->Data()));
									if (!writer->valid())
										throw std::runtime_error("could not create ZIP file");
									return AddFolderToZip(source, writer, "")
										.then([writer]()
										{
											if (!writer->finish())
												throw std::runtime_error("could not finalize ZIP file");
										});
								});
						});
				});
		})
		.then([this](task<void> result)
		{
			try
			{
				result.get();
				m_operationPending = false;
				SetStatus(L"World exported as a ZIP archive.");
			}
			catch (...)
			{
				m_operationPending = false;
				SetStatus(L"Could not export the world as a ZIP archive.");
			}
		}, task_continuation_context::use_current());
}

void WorldEditPage::OnBackClick(Object^, RoutedEventArgs^)
{
	ClosePage();
}

void WorldEditPage::ClosePage()
{
	this->BackRequested(this, nullptr);
}
