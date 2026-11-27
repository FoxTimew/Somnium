// Keeps interrupted writes invisible by using an exclusive temporary beside the destination.
// Only platform contains native durability and replacement operations.
#include "source_files_atomic.hpp"
#include <algorithm>
#include <limits>
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace somnium::platform::detail {
namespace {
Error failure(std::string_view operation) {
#if defined(_WIN32)
    const std::error_code error(static_cast<int>(GetLastError()), std::system_category());
#else
    const std::error_code error(errno, std::generic_category());
#endif
    return {ErrorCode::Io, std::string(operation) + ": " + error.message()};
}
struct Temporary {
    std::filesystem::path path;
    bool owned = false;
    ~Temporary() {
        if (owned) {
            std::error_code ignored;
            std::filesystem::remove(path, ignored);
        }
    }
};
} // namespace
Result<void> atomicReplace(const std::filesystem::path& target, std::string_view contents,
                           std::uint64_t sequence, const std::function<Result<void>()>& validate) {
#if defined(_WIN32)
    const auto process = GetCurrentProcessId();
    HANDLE file = INVALID_HANDLE_VALUE;
#else
    const auto process = getpid();
    int file = -1;
#endif
    Temporary temporary;
    for (unsigned attempt = 0; attempt != 64; ++attempt) {
        temporary.path = target;
        temporary.path += ".somnium-" + std::to_string(process) + "-" + std::to_string(sequence) +
                          "-" + std::to_string(attempt) + ".tmp";
#if defined(_WIN32)
        file = CreateFileW(temporary.path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file != INVALID_HANDLE_VALUE)
            break;
        if (GetLastError() != ERROR_FILE_EXISTS && GetLastError() != ERROR_ALREADY_EXISTS)
            return tl::unexpected(failure("Create source temporary"));
#else
        file = open(temporary.path.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0666);
        if (file != -1)
            break;
        if (errno != EEXIST)
            return tl::unexpected(failure("Create source temporary"));
#endif
    }
#if defined(_WIN32)
    if (file == INVALID_HANDLE_VALUE)
#else
    if (file == -1)
#endif
        return tl::unexpected(Error{ErrorCode::Io, "Source temporary name collision limit"});
    temporary.owned = true;
    std::size_t offset = 0;
    while (offset != contents.size()) {
#if defined(_WIN32)
        DWORD written = 0;
        const auto count = static_cast<DWORD>(
            std::min<std::size_t>(contents.size() - offset, std::numeric_limits<DWORD>::max()));
        if (!WriteFile(file, contents.data() + offset, count, &written, nullptr) || written == 0) {
            const auto error = failure("Write source temporary");
            CloseHandle(file);
            return tl::unexpected(error);
        }
#else
        const auto count =
            std::min<std::size_t>(contents.size() - offset, std::numeric_limits<ssize_t>::max());
        const auto written = write(file, contents.data() + offset, count);
        if (written < 0 && errno == EINTR)
            continue;
        if (written <= 0) {
            const auto error = failure("Write source temporary");
            close(file);
            return tl::unexpected(error);
        }
#endif
        offset += static_cast<std::size_t>(written);
    }
#if defined(_WIN32)
    if (!FlushFileBuffers(file)) {
        const auto error = failure("Flush source temporary");
        CloseHandle(file);
        return tl::unexpected(error);
    }
    if (!CloseHandle(file))
        return tl::unexpected(failure("Close source temporary"));
#else
    if (fsync(file) != 0) {
        const auto error = failure("Flush source temporary");
        close(file);
        return tl::unexpected(error);
    }
    if (close(file) != 0)
        return tl::unexpected(failure("Close source temporary"));
#endif
    auto checked = validate();
    if (!checked)
        return checked;
#if defined(_WIN32)
    if (!MoveFileExW(temporary.path.c_str(), target.c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        return tl::unexpected(failure("Publish source"));
#else
    std::error_code error;
    std::filesystem::rename(temporary.path, target, error);
    if (error)
        return tl::unexpected(Error{ErrorCode::Io, "Publish source: " + error.message()});
#endif
    temporary.owned = false;
#if !defined(_WIN32)
    const auto directory = open(target.parent_path().c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (directory == -1)
        return tl::unexpected(failure("Source published; cannot open directory for durability"));
    if (fsync(directory) != 0) {
        const auto error = failure("Source published; directory durability uncertain");
        close(directory);
        return tl::unexpected(error);
    }
    if (close(directory) != 0)
        return tl::unexpected(failure("Source published; cannot close directory"));
#endif
    return {};
}
} // namespace somnium::platform::detail
