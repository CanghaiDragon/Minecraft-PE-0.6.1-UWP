#include "pch.h"
#include "UwpZip.h"

#include <windows.h>
#include <windows.storage.h>
#include <windows.storage.streams.h>
#include <cstdio>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <vector>
#include <zlib.h>

using namespace concurrency;
using namespace Platform;
using namespace Windows::Storage;

namespace UwpZip
{
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

	bool DeflateZipData(const std::vector<unsigned char>& input,
		std::vector<unsigned char>& output)
	{
		if (input.size() > 0xffffffffULL)
			return false;

		z_stream stream = {};
		if (deflateInit2(&stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED,
			-MAX_WBITS, 8, Z_DEFAULT_STRATEGY) != Z_OK)
			return false;

		const uLong sourceSize = static_cast<uLong>(input.size());
		const uLong bound = deflateBound(&stream, sourceSize);
		if (bound > 0xffffffffUL)
		{
			deflateEnd(&stream);
			return false;
		}

		output.resize(static_cast<size_t>(bound));
		stream.next_in = input.empty()
			? Z_NULL
			: const_cast<Bytef*>(input.data());
		stream.avail_in = static_cast<uInt>(input.size());
		stream.next_out = output.empty() ? Z_NULL : output.data();
		stream.avail_out = static_cast<uInt>(output.size());

		const int result = deflate(&stream, Z_FINISH);
		const uLong compressedSize = stream.total_out;
		deflateEnd(&stream);
		if (result != Z_STREAM_END)
		{
			output.clear();
			return false;
		}

		output.resize(static_cast<size_t>(compressedSize));
		return true;
	}

	class ZipWriter
	{
		struct ZipEntry
		{
			std::string name;
			std::uint32_t crc = 0;
			std::uint32_t compressedSize = 0;
			std::uint32_t uncompressedSize = 0;
			std::uint32_t localHeaderOffset = 0;
		};

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
			std::vector<unsigned char> compressed;
			if (!DeflateZipData(data, compressed))
				return false;
			const uLongf compressedSize = static_cast<uLongf>(compressed.size());
			if (compressedSize > 0xffffffffUL || sourceSize > 0xffffffffUL)
				return false;

			const long offset = ftell(m_file);
			if (offset < 0 || static_cast<unsigned long long>(offset) > 0xffffffffULL)
				return false;
			if (!Write32(0x04034b50) || !Write16(20) || !Write16(0x0800) ||
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
					!Write16(0x0800) || !Write16(8) || !Write16(0) || !Write16(0) ||
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

task<void> CreateFromFolder(StorageFolder^ source,
	const std::wstring& archivePath)
{
	auto writer = std::make_shared<ZipWriter>(archivePath);
	if (!writer->valid())
		throw std::runtime_error("could not create ZIP file");

	return AddFolderToZip(source, writer, "")
		.then([writer](task<void> result) {
			try
			{
				result.get();
				if (!writer->finish())
					throw std::runtime_error("could not finalize ZIP file");
			}
			catch (...)
			{
				writer->discard();
				throw;
			}
		});
}
}
