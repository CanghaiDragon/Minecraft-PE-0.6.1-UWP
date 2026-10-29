#include "pch.h"
#include "WorldImportPage.xaml.h"
#include "../../../../src/platform/uwp/UwpOriginalClientBridge.h"
#include "../../../../src/platform/uwp/UwpStoragePaths.h"

#include <windows.storage.pickers.h>
#include <ppltasks.h>
#include <windows.h>
#include <cstdio>
#include <cwchar>
#include <string>
#include <vector>
#include <zlib.h>

using namespace MCPEUWP;
using namespace Platform;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::Storage;
using namespace Windows::Storage::Pickers;
using namespace concurrency;

namespace
{
	bool EnsureDirectory(const std::wstring& path)
	{
		if (path.empty() || CreateDirectoryW(path.c_str(), nullptr) ||
			GetLastError() == ERROR_ALREADY_EXISTS)
			return true;
		const size_t slash = path.find_last_of(L"\\/");
		return slash != std::wstring::npos && EnsureDirectory(path.substr(0, slash)) &&
			(CreateDirectoryW(path.c_str(), nullptr) || GetLastError() == ERROR_ALREADY_EXISTS);
	}

	std::uint16_t Read16(const std::vector<unsigned char>& d, size_t p)
	{ return static_cast<std::uint16_t>(d[p] | (d[p + 1] << 8)); }
	std::uint32_t Read32(const std::vector<unsigned char>& d, size_t p)
	{ return static_cast<std::uint32_t>(Read16(d, p) | (static_cast<std::uint32_t>(Read16(d, p + 2)) << 16)); }

	std::wstring NormalizeZipPath(const std::wstring& path)
	{
		std::wstring normalized = path;
		for (wchar_t& character : normalized)
		{
			if (character == L'\\')
				character = L'/';
		}
		while (normalized.size() >= 2 && normalized[0] == L'.' && normalized[1] == L'/')
			normalized.erase(0, 2);
		return normalized;
	}

	bool IsSafeRelativeZipPath(const std::wstring& path)
	{
		if (path.empty() || path[0] == L'/' || path.find(L':') != std::wstring::npos)
			return false;

		size_t start = 0;
		while (start < path.size())
		{
			const size_t slash = path.find(L'/', start);
			const size_t length = slash == std::wstring::npos
				? path.size() - start : slash - start;
			if (length == 2 && path.compare(start, length, L"..") == 0)
				return false;
			if (slash == std::wstring::npos)
				break;
			start = slash + 1;
		}
		return true;
	}

	bool GetRelativeZipPath(const std::wstring& archivePath,
		const std::wstring& worldRoot, std::wstring& relativePath)
	{
		const std::wstring normalizedPath = NormalizeZipPath(archivePath);
		if (worldRoot.empty())
		{
			relativePath = normalizedPath;
		}
		else
		{
			const std::wstring normalizedRoot = NormalizeZipPath(worldRoot);
			if (normalizedPath.size() < normalizedRoot.size() ||
				normalizedPath.compare(0, normalizedRoot.size(), normalizedRoot) != 0)
				return false;
			relativePath = normalizedPath.substr(normalizedRoot.size());
		}

		return IsSafeRelativeZipPath(relativePath);
	}

	bool InflateDeflate(const unsigned char* input, std::uint32_t inputSize,
		std::vector<unsigned char>& output, int windowBits)
	{
		z_stream s = {};
		s.next_in = const_cast<unsigned char*>(input);
		s.avail_in = inputSize;
		s.next_out = output.empty() ? Z_NULL : output.data();
		s.avail_out = static_cast<uInt>(output.size());
		if (inflateInit2(&s, windowBits) != Z_OK)
			return false;
		int result = Z_OK;
		do
		{
			const uInt oldIn = s.avail_in;
			const uInt oldOut = s.avail_out;
			result = inflate(&s, Z_NO_FLUSH);
			if (result == Z_BUF_ERROR && oldIn == s.avail_in && oldOut == s.avail_out)
				break;
		} while (result == Z_OK);
		inflateEnd(&s);
		return result == Z_STREAM_END;
	}

	bool LoadZipArchive(const std::wstring& archive, std::vector<unsigned char>& data,
		size_t& centralOffset, std::wstring& error)
	{
		error.clear();
		FILE* input = _wfopen(archive.c_str(), L"rb");
		if (!input) { error = L"cannot open selected ZIP"; return false; }
		if (fseek(input, 0, SEEK_END) != 0)
		{
			fclose(input);
			error = L"cannot seek selected ZIP";
			return false;
		}
		const long length = ftell(input);
		if (length < 0 || fseek(input, 0, SEEK_SET) != 0)
		{
			fclose(input);
			error = L"cannot read selected ZIP";
			return false;
		}

		data.assign(static_cast<size_t>(length), 0);
		const bool loaded = data.empty() || fread(data.data(), 1, data.size(), input) == data.size();
		fclose(input);
		if (!loaded) { error = L"cannot read ZIP file"; return false; }

		size_t eocd = data.size();
		const size_t searchStart = data.size() > 0xffff + 22 ? data.size() - (0xffff + 22) : 0;
		for (size_t candidate = data.size(); candidate >= searchStart + 4; --candidate)
		{
			if (Read32(data, candidate - 4) == 0x06054b50)
			{
				eocd = candidate - 4;
				break;
			}
		}
		if (eocd == data.size() || eocd + 22 > data.size())
		{
			error = L"ZIP end directory not found";
			return false;
		}

		centralOffset = static_cast<size_t>(Read32(data, eocd + 16));
		if (centralOffset >= data.size())
		{
			error = L"ZIP central directory is invalid";
			return false;
		}
		return true;
	}

	bool ReadZipEntry(const std::vector<unsigned char>& data, size_t centralEntry,
		std::wstring& name, std::vector<unsigned char>& result, bool& directory,
		size_t& nextEntry, std::wstring& error)
	{
		error.clear();
		if (centralEntry + 46 > data.size() || Read32(data, centralEntry) != 0x02014b50)
		{
			error = L"ZIP central directory entry is invalid";
			return false;
		}

		const std::uint16_t method = Read16(data, centralEntry + 10);
		const std::uint32_t compressedSize = Read32(data, centralEntry + 20);
		const std::uint32_t uncompressedSize = Read32(data, centralEntry + 24);
		const std::uint16_t nameLength = Read16(data, centralEntry + 28);
		const std::uint16_t extraLength = Read16(data, centralEntry + 30);
		const std::uint16_t commentLength = Read16(data, centralEntry + 32);
		const std::uint32_t localOffset = Read32(data, centralEntry + 42);
		nextEntry = centralEntry + 46ULL + nameLength + extraLength + commentLength;
		if (nextEntry > data.size() || localOffset + 30ULL > data.size())
		{
			error = L"ZIP entry header is truncated";
			return false;
		}
		if (Read32(data, localOffset) != 0x04034b50)
		{
			error = L"ZIP local entry header is invalid";
			return false;
		}

		const std::uint16_t localNameLength = Read16(data, localOffset + 26);
		const std::uint16_t localExtraLength = Read16(data, localOffset + 28);
		const size_t start = localOffset + 30ULL + localNameLength + localExtraLength;
		if (start > data.size() || compressedSize > data.size() - start)
		{
			error = L"ZIP entry data is truncated";
			return false;
		}

		const char* rawName = reinterpret_cast<const char*>(data.data() + centralEntry + 46);
		const int wideLength = MultiByteToWideChar(
			CP_UTF8, 0, rawName, static_cast<int>(nameLength), nullptr, 0);
		if (wideLength <= 0)
		{
			error = L"ZIP entry name is invalid";
			return false;
		}
		name.assign(wideLength, L'\0');
		MultiByteToWideChar(CP_UTF8, 0, rawName, static_cast<int>(nameLength),
			&name[0], wideLength);
		directory = !name.empty() && (name.back() == L'/' || name.back() == L'\\');
		result.clear();
		if (directory)
			return true;

		bool ok = true;
		if (method == 0)
			result.assign(data.begin() + start, data.begin() + start + compressedSize);
		else if (method == 8)
		{
			result.resize(static_cast<size_t>(uncompressedSize) + 1);
			// Standard ZIP uses raw Deflate.  The game's historical ZIP
			// exporter uses a zlib-wrapped Deflate stream, so accept both.
			ok = InflateDeflate(data.data() + start, compressedSize, result, -MAX_WBITS) ||
				InflateDeflate(data.data() + start, compressedSize, result, MAX_WBITS);
		}
		else
		{
			error = L"ZIP compression method is unsupported";
			return false;
		}
		if (!ok)
		{
			error = L"failed to decompress ZIP entry";
			return false;
		}
		if (method == 8)
			result.resize(uncompressedSize);
		if (result.size() != uncompressedSize)
		{
			error = L"ZIP entry size does not match its header";
			return false;
		}
		return true;
	}

	enum class LevelDatCheck
	{
		Cancelled,
		Compatible,
		TooNew,
		Missing,
		Multiple,
		Invalid
	};

	struct LevelDatCheckResult
	{
		LevelDatCheck status = LevelDatCheck::Invalid;
		std::uint32_t formatVersion = 0;
		std::wstring worldRoot;
		std::wstring error;
	};

	LevelDatCheckResult CheckLevelDatVersion(const std::wstring& archive)
	{
		LevelDatCheckResult check;
		std::vector<unsigned char> data;
		size_t centralOffset = 0;
		if (!LoadZipArchive(archive, data, centralOffset, check.error))
			return check;

		size_t entry = centralOffset;
		size_t levelDatCount = 0;
		while (entry + 46 <= data.size() && Read32(data, entry) == 0x02014b50)
		{
			std::wstring name;
			std::vector<unsigned char> contents;
			bool directory = false;
			size_t nextEntry = entry;
			if (!ReadZipEntry(data, entry, name, contents, directory, nextEntry, check.error))
				return check;

			const size_t slash = name.find_last_of(L"\\/");
			const std::wstring leaf = slash == std::wstring::npos
				? name : name.substr(slash + 1);
			if (!directory && _wcsicmp(leaf.c_str(), L"level.dat") == 0)
			{
				if (contents.size() < sizeof(std::uint32_t))
				{
					check.error = L"level.dat is too small to contain a format version";
					return check;
				}
				++levelDatCount;
				if (levelDatCount == 1)
				{
					check.formatVersion = Read32(contents, 0);
					const std::wstring normalizedName = NormalizeZipPath(name);
					const size_t nameSlash = normalizedName.find_last_of(L'/');
					check.worldRoot = nameSlash == std::wstring::npos
						? std::wstring() : normalizedName.substr(0, nameSlash + 1);
				}
			}
			entry = nextEntry;
		}

		if (levelDatCount == 0)
		{
			check.status = LevelDatCheck::Missing;
			check.error = L"level.dat was not found in the selected ZIP";
		}
		else if (levelDatCount > 1)
		{
			check.status = LevelDatCheck::Multiple;
			check.error = L"The selected ZIP contains multiple level.dat files";
		}
		else
		{
			check.status = check.formatVersion <= 3
				? LevelDatCheck::Compatible : LevelDatCheck::TooNew;
		}
		return check;
	}

	bool ExtractZip(const std::wstring& archive, const std::wstring& destination,
		const std::wstring& worldRoot,
		std::wstring& error)
	{
		error.clear();
		FILE* input = _wfopen(archive.c_str(), L"rb");
		if (!input) { error = L"cannot open selected ZIP"; return false; }
		if (!EnsureDirectory(destination)) { fclose(input); error = L"cannot create destination folder"; return false; }
		fseek(input, 0, SEEK_END); const long length = ftell(input); fseek(input, 0, SEEK_SET);
		std::vector<unsigned char> d(length > 0 ? static_cast<size_t>(length) : 0);
		const bool loaded = length >= 0 && (d.empty() || fread(d.data(), 1, d.size(), input) == d.size()); fclose(input);
		if (!loaded) { error = L"cannot read ZIP file"; return false; }
		size_t eocd = d.size();
		const size_t searchStart = d.size() > 0xffff + 22 ? d.size() - (0xffff + 22) : 0;
		for (size_t candidate = d.size(); candidate >= searchStart + 4; --candidate)
		{
			if (Read32(d, candidate - 4) == 0x06054b50)
			{
				eocd = candidate - 4;
				break;
			}
		}
		if (eocd == d.size() || eocd + 22 > d.size()) { error = L"ZIP end directory not found"; return false; }
		const std::uint32_t centralOffset = Read32(d, eocd + 16);
		size_t p = centralOffset;
		if (p + 46 > d.size() || Read32(d, p) != 0x02014b50) { error = L"ZIP central directory is invalid"; return false; }
		while (p + 46 <= d.size() && Read32(d, p) == 0x02014b50)
		{
			const std::uint16_t method = Read16(d, p + 10);
			const std::uint32_t cs = Read32(d, p + 20), us = Read32(d, p + 24);
			const std::uint16_t nl = Read16(d, p + 28), el = Read16(d, p + 30), comment = Read16(d, p + 32);
			const std::uint32_t localOffset = Read32(d, p + 42);
			if (p + 46ULL + nl + el + comment > d.size() || localOffset + 30 > d.size()) { error = L"ZIP entry header is truncated"; return false; }
			const std::uint16_t localName = Read16(d, localOffset + 26);
			const std::uint16_t localExtra = Read16(d, localOffset + 28);
			const size_t start = localOffset + 30ULL + localName + localExtra;
			if (start + cs > d.size()) { error = L"ZIP entry data is truncated"; return false; }
			std::string n(reinterpret_cast<const char*>(d.data() + p + 46), nl);
			int wc = MultiByteToWideChar(CP_UTF8, 0, n.data(), static_cast<int>(n.size()), nullptr, 0);
			if (wc <= 0) { error = L"ZIP entry name is invalid"; return false; }
			std::wstring archiveName(wc, L'\0');
			MultiByteToWideChar(CP_UTF8, 0, n.data(), static_cast<int>(n.size()), &archiveName[0], wc);

			const std::wstring normalizedName = NormalizeZipPath(archiveName);
			const std::wstring normalizedRoot = NormalizeZipPath(worldRoot);
			if (!normalizedRoot.empty() &&
				(normalizedName.size() < normalizedRoot.size() ||
					normalizedName.compare(0, normalizedRoot.size(), normalizedRoot) != 0))
			{
				// Ignore archive metadata and other files outside the directory
				// that contains level.dat.
				p += 46ULL + nl + el + comment;
				continue;
			}

			std::wstring rel;
			if (!GetRelativeZipPath(archiveName, worldRoot, rel))
			{
				// Ignore malformed or traversal-like entries instead of allowing
				// unrelated ZIP metadata to make an otherwise valid world fail.
				p += 46ULL + nl + el + comment;
				continue;
			}
			if (rel.empty())
			{
				p += 46ULL + nl + el + comment;
				continue;
			}
			const std::wstring out = destination + L"\\" + rel;
			bool ok = true;
			if (!rel.empty() && (rel.back() == L'/' || rel.back() == L'\\')) ok = EnsureDirectory(out);
			else
			{
				std::vector<unsigned char> result(us + (method == 8 ? 1 : 0));
				if (method == 0) result.assign(d.begin() + start, d.begin() + start + cs);
				else if (method == 8)
				{
					// Standard ZIP uses raw Deflate.  The game's historical ZIP
					// exporter uses a zlib-wrapped Deflate stream, so accept both.
					ok = InflateDeflate(d.data() + start, cs, result, -MAX_WBITS) ||
						InflateDeflate(d.data() + start, cs, result, MAX_WBITS);
				}
				else ok = false;
				if (ok && result.size() != us) result.resize(us);
				const size_t slash = out.find_last_of(L"\\/"); if (ok && slash != std::wstring::npos) ok = EnsureDirectory(out.substr(0, slash));
				FILE* f = ok ? _wfopen(out.c_str(), L"wb") : nullptr; if (!f) ok = false;
				if (ok) ok = fwrite(result.data(), 1, result.size(), f) == result.size(); if (f) fclose(f);
			}
			if (!ok)
			{
				error = L"failed to extract entry: ";
				error += archiveName;
				return false;
			}
			p += 46ULL + nl + el + comment;
		}
		return true;
	}
}

WorldImportPage::WorldImportPage()
{
	InitializeComponent();
}

void WorldImportPage::HandleEnterKey()
{
	if (ImportRejectDialogOverlay != nullptr &&
		ImportRejectDialogOverlay->Visibility ==
			Windows::UI::Xaml::Visibility::Visible)
		HideRejectDialog();
}

void WorldImportPage::HandleEscapeKey()
{
	if (ImportRejectDialogOverlay != nullptr &&
		ImportRejectDialogOverlay->Visibility ==
			Windows::UI::Xaml::Visibility::Visible)
	{
		HideRejectDialog();
		return;
	}
	OnBackClick(nullptr, nullptr);
}

void WorldImportPage::OnBackClick(Object^, RoutedEventArgs^)
{
	UwpOriginalClientRequestCloseWorldImport();
}

void WorldImportPage::ShowRejectDialog(String^ title, String^ message)
{
	if (SelectZipButton != nullptr)
		SelectZipButton->IsEnabled = false;
	if (ImportRejectDialogTitle != nullptr)
		ImportRejectDialogTitle->Text = title == nullptr
			? ref new String(L"") : title;
	if (ImportRejectDialogMessage != nullptr)
		ImportRejectDialogMessage->Text = message == nullptr
			? ref new String(L"") : message;
	if (ImportRejectDialogOverlay != nullptr)
	{
		AnimateRejectDialog(true);
		if (ImportRejectDialogOkButton != nullptr)
			ImportRejectDialogOkButton->Focus(
				Windows::UI::Xaml::FocusState::Programmatic);
	}
}

void WorldImportPage::HideRejectDialog()
{
	if (ImportRejectDialogOverlay != nullptr)
		AnimateRejectDialog(false);
}

void WorldImportPage::AnimateRejectDialog(bool entering)
{
	if (ImportRejectDialogOverlay == nullptr)
		return;

	if (m_rejectDialogTransition != nullptr)
	{
		m_rejectDialogTransition->Stop();
		if (m_rejectDialogTransitionCompletedToken.Value != 0)
		{
			m_rejectDialogTransition->Completed -=
				m_rejectDialogTransitionCompletedToken;
			m_rejectDialogTransitionCompletedToken.Value = 0;
		}
		m_rejectDialogTransition = nullptr;
	}

	if (entering)
	{
		ImportRejectDialogOverlay->Visibility =
			Windows::UI::Xaml::Visibility::Visible;
		ImportRejectDialogOverlay->Opacity = 0.0;
	}

	auto transition = ref new Windows::UI::Xaml::Media::Animation::Storyboard();
	auto fade = ref new Windows::UI::Xaml::Media::Animation::DoubleAnimation();
	fade->From = entering ? 0.0 : ImportRejectDialogOverlay->Opacity;
	fade->To = entering ? 1.0 : 0.0;
	Windows::Foundation::TimeSpan duration;
	duration.Duration = entering ? 2200000 : 1800000;
	fade->Duration = duration;
	Windows::UI::Xaml::Media::Animation::Storyboard::SetTarget(
		fade, ImportRejectDialogOverlay);
	Windows::UI::Xaml::Media::Animation::Storyboard::SetTargetProperty(
		fade, ref new Platform::String(L"Opacity"));
	transition->Children->Append(fade);

	m_rejectDialogTransition = transition;
	m_rejectDialogTransitionCompletedToken = transition->Completed +=
		ref new Windows::Foundation::EventHandler<Platform::Object^>(
			[this, transition, entering](Platform::Object^, Platform::Object^)
		{
				if (m_rejectDialogTransition != transition)
					return;

				if (!entering && ImportRejectDialogOverlay != nullptr)
				{
					ImportRejectDialogOverlay->Visibility =
						Windows::UI::Xaml::Visibility::Collapsed;
					ImportRejectDialogOverlay->Opacity = 0.0;
					if (SelectZipButton != nullptr)
						SelectZipButton->IsEnabled = true;
				}
				else if (entering && ImportRejectDialogOverlay != nullptr)
				{
					ImportRejectDialogOverlay->Opacity = 1.0;
				}

				if (m_rejectDialogTransitionCompletedToken.Value != 0)
				{
					transition->Completed -=
						m_rejectDialogTransitionCompletedToken;
					m_rejectDialogTransitionCompletedToken.Value = 0;
				}
				m_rejectDialogTransition = nullptr;
			});
	transition->Begin();
}

void WorldImportPage::OnRejectDialogOkClick(Object^, RoutedEventArgs^)
{
	HideRejectDialog();
}

void WorldImportPage::ShowTooNewVersionDialog(unsigned int formatVersion)
{
	const std::wstring message =
		L"This world uses format version " + std::to_wstring(formatVersion) +
		L". Minecraft PE 0.6.1 only supports format versions up to 3.";
	ShowRejectDialog(
		ref new String(L"World Cannot Be Imported"),
		ref new String(message.c_str()));
}

void WorldImportPage::ShowMultipleLevelDatDialog()
{
	ShowRejectDialog(
		ref new String(L"World Cannot Be Imported"),
		ref new String(
			L"The selected ZIP contains multiple level.dat files. Please select one world archive at a time."));
}

void WorldImportPage::ShowInvalidArchiveDialog(String^ error)
{
	std::wstring message = L"The selected ZIP does not contain a valid Minecraft PE world.";
	if (error != nullptr && error->Length() > 0)
	{
		message += L"\n";
		message += error->Data();
	}
	ShowRejectDialog(
		ref new String(L"World Cannot Be Imported"),
		ref new String(message.c_str()));
}

void WorldImportPage::ShowImportSuccess(unsigned int formatVersion, String^ worldRoot)
{
	std::wstring details = L"Archive: ";
	if (m_importArchiveName != nullptr)
		details += m_importArchiveName->Data();
	details += L"\nFormat version: ";
	details += std::to_wstring(formatVersion);
	details += L"\nImported to: minecraftWorlds\\";
	if (m_importWorldName != nullptr)
		details += m_importWorldName->Data();
	if (worldRoot != nullptr && worldRoot->Length() > 0)
	{
		details += L"\nlevel.dat source folder: ";
		details += worldRoot->Data();
	}

	if (StatusText != nullptr)
		StatusText->Text = ref new String(L"");
	if (ImportSuccessDetails != nullptr)
		ImportSuccessDetails->Text = ref new String(details.c_str());
	if (ImportSuccessPanel != nullptr)
		ImportSuccessPanel->Visibility =
			Windows::UI::Xaml::Visibility::Visible;
}

void WorldImportPage::OnSelectZipClick(Object^, RoutedEventArgs^)
{
	if (StatusText != nullptr)
		StatusText->Text = ref new String(L"Select a ZIP file...");
	if (ImportSuccessPanel != nullptr)
		ImportSuccessPanel->Visibility =
			Windows::UI::Xaml::Visibility::Collapsed;

	auto picker = ref new FileOpenPicker();
	picker->SuggestedStartLocation = PickerLocationId::Downloads;
	picker->FileTypeFilter->Append(L".zip");

	create_task(picker->PickSingleFileAsync())
	.then([this](StorageFile^ file) -> task<StorageFile^>
	{
		if (file == nullptr)
		{
			StatusText->Text = ref new String(L"Import cancelled.");
			return task_from_result<StorageFile^>(nullptr);
		}

		m_importWorldName = file->DisplayName;
		m_importArchiveName = file->Name;
		StatusText->Text = ref new String(L"Checking world format...");
		return create_task(file->CopyAsync(
			ApplicationData::Current->TemporaryFolder,
			L"mcpe_import.zip",
			NameCollisionOption::GenerateUniqueName));
	}, task_continuation_context::use_current())
	.then([this](StorageFile^ temporaryZip) -> task<LevelDatCheckResult>
	{
		if (temporaryZip == nullptr)
		{
			LevelDatCheckResult cancelled;
			cancelled.status = LevelDatCheck::Cancelled;
			return task_from_result(cancelled);
		}

		m_importTemporaryFile = temporaryZip;
		const std::wstring archivePath = temporaryZip->Path->Data();
		return create_task([archivePath]()
		{
			return CheckLevelDatVersion(archivePath);
		});
	})
	.then([this](LevelDatCheckResult check) -> task<void>
	{
		if (check.status == LevelDatCheck::Cancelled)
			return task_from_result();
		if (check.status == LevelDatCheck::Multiple)
		{
			StatusText->Text = ref new String(L"Import rejected: multiple level.dat files were found.");
			ShowMultipleLevelDatDialog();
			return task_from_result();
		}
		if (check.status == LevelDatCheck::TooNew)
		{
			StatusText->Text = ref new String(L"This world cannot be imported by Minecraft PE 0.6.1.");
			ShowTooNewVersionDialog(check.formatVersion);
			return task_from_result();
		}
		if (check.status != LevelDatCheck::Compatible)
		{
			String^ error = ref new String(
				check.error.empty() ? L"The ZIP does not contain a readable level.dat." : check.error.c_str());
			StatusText->Text = error;
			ShowInvalidArchiveDialog(error);
			return task_from_result();
		}

		StatusText->Text = ref new String(L"Extracting ZIP archive...");
		const std::wstring worldRoot = check.worldRoot;
		const unsigned int formatVersion = check.formatVersion;
		return UwpStoragePaths::ensureWorldsFolderAsync()
			.then([this, worldRoot, formatVersion](StorageFolder^ worlds) -> task<void>
			{
				String^ name = m_importWorldName;
				return create_task(worlds->CreateFolderAsync(
					name, CreationCollisionOption::GenerateUniqueName))
					.then([this, worldRoot, formatVersion](StorageFolder^ destination) -> task<void>
					{
						const std::wstring destinationPath =
							std::wstring(destination->Path->Data()) + L"\\";
						std::wstring extractionError;
						const bool extracted = ExtractZip(
							m_importTemporaryFile->Path->Data(), destinationPath,
							worldRoot, extractionError);
						if (!extracted)
						{
							StatusText->Text = ref new String(extractionError.c_str());
							return task_from_result();
						}
						ShowImportSuccess(formatVersion,
							ref new String(worldRoot.c_str()));
						return task_from_result();
					});
			});
	}, task_continuation_context::use_current())
	.then([this](task<void> result)
		{
			try { result.get(); }
			catch (...)
			{
				StatusText->Text = ref new String(L"Could not import ZIP.");
			}
		}, task_continuation_context::use_current());
}
