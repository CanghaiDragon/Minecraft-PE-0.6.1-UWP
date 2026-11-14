// LevelDB environment for the UWP target.
//
// LevelDB's upstream Windows environment uses desktop-only ANSI Win32 file
// APIs (CreateFileA, MoveFileA, memory mappings, and file locks).  Those APIs
// are not available to the UWP API family.  The converter only needs the
// ordinary LevelDB file contract, so this implementation uses C++17
// filesystem/stream primitives and the existing UWP sandbox path supplied by
// the caller.

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdarg>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <limits>
#include <memory>
#include <mutex>
#include <queue>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "leveldb/env.h"
#include "leveldb/slice.h"

namespace leveldb {

namespace {

namespace fs = std::filesystem;

constexpr std::size_t kWritableFileBufferSize = 65536;

fs::path UwpPath(const std::string& value) {
  // LevelDB receives UTF-8 paths from the converter.  u8path keeps them
  // independent of the process ANSI code page.
  return fs::u8path(value);
}

std::string UwpPathString(const fs::path& value) {
  return value.u8string();
}

Status OpenError(const std::string& filename) {
  std::error_code error;
  if (!fs::exists(UwpPath(filename), error))
    return Status::NotFound(filename, "file not found");
  return Status::IOError(filename, "could not open file");
}

Status FilesystemError(const std::string& filename, const char* operation) {
  return Status::IOError(filename, operation);
}

class UwpSequentialFile final : public SequentialFile {
 public:
  UwpSequentialFile(std::string filename, std::ifstream file)
      : filename_(std::move(filename)), file_(std::move(file)) {}

  Status Read(std::size_t n, Slice* result, char* scratch) override {
    file_.read(scratch, static_cast<std::streamsize>(n));
    const std::streamsize count = file_.gcount();
    if (file_.bad()) {
      *result = Slice(scratch, 0);
      return FilesystemError(filename_, "read failed");
    }
    *result = Slice(scratch, static_cast<std::size_t>(count));
    return Status::OK();
  }

  Status Skip(std::uint64_t n) override {
    if (n > static_cast<std::uint64_t>(std::numeric_limits<std::streamoff>::max()))
      return FilesystemError(filename_, "skip failed");

    file_.clear();
    file_.seekg(static_cast<std::streamoff>(n), std::ios::cur);
    if (!file_) {
      // LevelDB permits skipping beyond EOF; position at EOF in that case.
      file_.clear();
      file_.seekg(0, std::ios::end);
      if (!file_)
        return FilesystemError(filename_, "skip failed");
    }
    return Status::OK();
  }

 private:
  const std::string filename_;
  std::ifstream file_;
};

class UwpRandomAccessFile final : public RandomAccessFile {
 public:
  explicit UwpRandomAccessFile(std::string filename)
      : filename_(std::move(filename)) {}

  Status Read(std::uint64_t offset, std::size_t n, Slice* result,
              char* scratch) const override {
    std::ifstream file(UwpPath(filename_), std::ios::binary);
    if (!file) {
      *result = Slice(scratch, 0);
      return OpenError(filename_);
    }
    if (offset > static_cast<std::uint64_t>(
                    std::numeric_limits<std::streamoff>::max())) {
      *result = Slice(scratch, 0);
      return FilesystemError(filename_, "offset is too large");
    }

    file.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
    if (!file) {
      // Reading beyond EOF is a successful zero-byte read.
      *result = Slice(scratch, 0);
      return Status::OK();
    }
    file.read(scratch, static_cast<std::streamsize>(n));
    const std::streamsize count = file.gcount();
    if (file.bad()) {
      *result = Slice(scratch, 0);
      return FilesystemError(filename_, "read failed");
    }
    *result = Slice(scratch, static_cast<std::size_t>(count));
    return Status::OK();
  }

 private:
  const std::string filename_;
};

class UwpWritableFile final : public WritableFile {
 public:
  UwpWritableFile(std::string filename, std::ofstream file)
      : filename_(std::move(filename)), file_(std::move(file)) {}

  ~UwpWritableFile() override {
    if (!closed_)
      Close();
  }

  Status Append(const Slice& data) override {
    if (closed_)
      return FilesystemError(filename_, "append after close");

    std::size_t remaining = data.size();
    const char* source = data.data();
    const std::size_t buffered = std::min(
        remaining, kWritableFileBufferSize - bufferSize_);
    std::memcpy(buffer_ + bufferSize_, source, buffered);
    bufferSize_ += buffered;
    source += buffered;
    remaining -= buffered;

    if (remaining == 0)
      return Status::OK();

    Status status = FlushBuffer();
    if (!status.ok())
      return status;

    if (remaining < kWritableFileBufferSize) {
      std::memcpy(buffer_, source, remaining);
      bufferSize_ = remaining;
      return Status::OK();
    }
    return WriteUnbuffered(source, remaining);
  }

  Status Close() override {
    if (closed_)
      return Status::OK();

    Status status = FlushBuffer();
    file_.close();
    if (!file_ && status.ok())
      status = FilesystemError(filename_, "close failed");
    closed_ = true;
    return status;
  }

  Status Flush() override {
    Status status = FlushBuffer();
    if (!status.ok())
      return status;
    file_.flush();
    return file_ ? Status::OK() : FilesystemError(filename_, "flush failed");
  }

  Status Sync() override {
    // UWP does not expose the desktop FlushFileBuffers contract.  The stream
    // flush is the portable durability boundary available to this target.
    return Flush();
  }

 private:
  Status FlushBuffer() {
    if (bufferSize_ == 0)
      return Status::OK();
    Status status = WriteUnbuffered(buffer_, bufferSize_);
    if (status.ok())
      bufferSize_ = 0;
    return status;
  }

  Status WriteUnbuffered(const char* data, std::size_t size) {
    while (size > 0) {
      const std::size_t chunk = std::min(
          size, static_cast<std::size_t>(std::numeric_limits<std::streamsize>::max()));
      file_.write(data, static_cast<std::streamsize>(chunk));
      if (!file_)
        return FilesystemError(filename_, "write failed");
      data += chunk;
      size -= chunk;
    }
    return Status::OK();
  }

  const std::string filename_;
  std::ofstream file_;
  char buffer_[kWritableFileBufferSize]{};
  std::size_t bufferSize_ = 0;
  bool closed_ = false;
};

class UwpFileLock final : public FileLock {
 public:
  UwpFileLock(std::string filename, std::unique_ptr<std::fstream> file)
      : filename_(std::move(filename)), file_(std::move(file)) {}

  const std::string& filename() const { return filename_; }

 private:
  const std::string filename_;
  std::unique_ptr<std::fstream> file_;
};

std::mutex g_lockMutex;
std::set<std::string> g_lockedFiles;

class UwpLogger final : public Logger {
 public:
  explicit UwpLogger(std::ofstream file) : file_(std::move(file)) {}

  void Logv(const char* format, std::va_list arguments) override {
    std::vector<char> buffer(512);
    while (true) {
      std::va_list copy;
      va_copy(copy, arguments);
      const int written = std::vsnprintf(
          buffer.data(), buffer.size(), format, copy);
      va_end(copy);
      if (written < 0)
        return;
      if (static_cast<std::size_t>(written) < buffer.size())
        break;
      buffer.resize(static_cast<std::size_t>(written) + 1);
    }

    const auto now = std::chrono::system_clock::now();
    const std::time_t seconds = std::chrono::system_clock::to_time_t(now);
    std::tm localTime{};
    if (const std::tm* converted = std::localtime(&seconds))
      localTime = *converted;

    char timestamp[64]{};
    std::strftime(timestamp, sizeof(timestamp),
                  "%Y/%m/%d-%H:%M:%S", &localTime);
    std::lock_guard<std::mutex> guard(mutex_);
    file_ << timestamp << " " << buffer.data() << "\n";
    file_.flush();
  }

 private:
  std::ofstream file_;
  std::mutex mutex_;
};

class UwpEnv final : public Env {
 public:
  UwpEnv() = default;
  ~UwpEnv() override = default;

  Status NewSequentialFile(const std::string& filename,
                           SequentialFile** result) override {
    *result = nullptr;
    std::ifstream file(UwpPath(filename), std::ios::binary);
    if (!file)
      return OpenError(filename);
    *result = new UwpSequentialFile(filename, std::move(file));
    return Status::OK();
  }

  Status NewRandomAccessFile(const std::string& filename,
                             RandomAccessFile** result) override {
    *result = nullptr;
    std::ifstream file(UwpPath(filename), std::ios::binary);
    if (!file)
      return OpenError(filename);
    *result = new UwpRandomAccessFile(filename);
    return Status::OK();
  }

  Status NewWritableFile(const std::string& filename,
                         WritableFile** result) override {
    *result = nullptr;
    std::ofstream file(UwpPath(filename),
                       std::ios::binary | std::ios::trunc);
    if (!file)
      return FilesystemError(filename, "could not create file");
    *result = new UwpWritableFile(filename, std::move(file));
    return Status::OK();
  }

  Status NewAppendableFile(const std::string& filename,
                           WritableFile** result) override {
    *result = nullptr;
    std::ofstream file(UwpPath(filename),
                       std::ios::binary | std::ios::app);
    if (!file)
      return FilesystemError(filename, "could not open appendable file");
    *result = new UwpWritableFile(filename, std::move(file));
    return Status::OK();
  }

  bool FileExists(const std::string& filename) override {
    std::error_code error;
    return fs::exists(UwpPath(filename), error);
  }

  Status GetChildren(const std::string& directory,
                     std::vector<std::string>* result) override {
    result->clear();
    std::error_code error;
    const fs::path path = UwpPath(directory);
    if (!fs::exists(path, error))
      return Status::NotFound(directory, "directory not found");

    for (fs::directory_iterator iterator(path, error), end;
         iterator != end && !error; iterator.increment(error)) {
      result->push_back(UwpPathString(iterator->path().filename()));
    }
    return error ? Status::IOError(directory, "directory enumeration failed")
                 : Status::OK();
  }

  Status RemoveFile(const std::string& filename) override {
    std::error_code error;
    if (!fs::remove(UwpPath(filename), error)) {
      if (!fs::exists(UwpPath(filename), error))
        return Status::NotFound(filename, "file not found");
      return Status::IOError(filename, "remove failed");
    }
    return Status::OK();
  }

  Status CreateDir(const std::string& dirname) override {
    std::error_code error;
    if (fs::create_directory(UwpPath(dirname), error) ||
        (fs::exists(UwpPath(dirname), error) &&
         fs::is_directory(UwpPath(dirname), error)))
      return Status::OK();
    return Status::IOError(dirname, "create directory failed");
  }

  Status RemoveDir(const std::string& dirname) override {
    std::error_code error;
    if (fs::remove(UwpPath(dirname), error))
      return Status::OK();
    return Status::IOError(dirname, "remove directory failed");
  }

  Status GetFileSize(const std::string& filename,
                     std::uint64_t* size) override {
    std::error_code error;
    const std::uintmax_t fileSize = fs::file_size(UwpPath(filename), error);
    if (error) {
      *size = 0;
      return Status::IOError(filename, "get file size failed");
    }
    *size = static_cast<std::uint64_t>(fileSize);
    return Status::OK();
  }

  Status RenameFile(const std::string& from,
                    const std::string& to) override {
    std::error_code error;
    // LevelDB uses rename to publish a new MANIFEST.  std::filesystem follows
    // the UWP stream behavior but does not replace an existing target, so
    // remove the old target first.
    fs::remove(UwpPath(to), error);
    error.clear();
    fs::rename(UwpPath(from), UwpPath(to), error);
    return error ? Status::IOError(from, "rename failed") : Status::OK();
  }

  Status LockFile(const std::string& filename, FileLock** lock) override {
    *lock = nullptr;
    std::lock_guard<std::mutex> guard(g_lockMutex);
    if (g_lockedFiles.find(filename) != g_lockedFiles.end())
      return Status::IOError("lock " + filename, "already held by process");

    if (!FileExists(filename)) {
      std::ofstream create(UwpPath(filename), std::ios::binary);
      if (!create)
        return Status::IOError(filename, "create lock file failed");
    }
    auto file = std::make_unique<std::fstream>(
        UwpPath(filename), std::ios::in | std::ios::out | std::ios::binary);
    if (!file || !*file)
      return Status::IOError(filename, "open lock file failed");

    g_lockedFiles.insert(filename);
    *lock = new UwpFileLock(filename, std::move(file));
    return Status::OK();
  }

  Status UnlockFile(FileLock* lock) override {
    auto* uwpLock = static_cast<UwpFileLock*>(lock);
    {
      std::lock_guard<std::mutex> guard(g_lockMutex);
      g_lockedFiles.erase(uwpLock->filename());
    }
    delete uwpLock;
    return Status::OK();
  }

  void Schedule(void (*function)(void* arg), void* arg) override {
    std::thread([function, arg]() { function(arg); }).detach();
  }

  void StartThread(void (*function)(void* arg), void* arg) override {
    std::thread(function, arg).detach();
  }

  Status GetTestDirectory(std::string* result) override {
    const char* configured = std::getenv("TEST_TMPDIR");
    fs::path path;
    if (configured != nullptr && configured[0] != '\0') {
      path = UwpPath(configured);
    } else {
      std::ostringstream name;
      name << "leveldbtest-" << std::hash<std::thread::id>{}(
          std::this_thread::get_id());
      std::error_code error;
      path = fs::temp_directory_path(error) / name.str();
      if (error)
        path = fs::current_path() / name.str();
    }

    std::error_code error;
    fs::create_directories(path, error);
    if (error)
      return Status::IOError(UwpPathString(path), "create test directory failed");
    *result = UwpPathString(path);
    return Status::OK();
  }

  Status NewLogger(const std::string& filename, Logger** result) override {
    *result = nullptr;
    std::ofstream file(UwpPath(filename), std::ios::app | std::ios::binary);
    if (!file)
      return FilesystemError(filename, "could not create log file");
    *result = new UwpLogger(std::move(file));
    return Status::OK();
  }

  std::uint64_t NowMicros() override {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());
  }

  void SleepForMicroseconds(int micros) override {
    std::this_thread::sleep_for(std::chrono::microseconds(micros));
  }
};

} // namespace

Env* Env::Default() {
  static UwpEnv env;
  return &env;
}

} // namespace leveldb
