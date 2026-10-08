#include "file_handle.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <utility>

namespace ioj::s7::detail {
void FileHandleCloser::operator()(void* const handle) const {
    CloseHandle(handle);
}

[[nodiscard]] auto open_script_handle(std::wstring const& path, bool const directory)
    -> FileHandle {
    auto const handle{
        CreateFileW(path.c_str(),
                    directory ? FILE_READ_ATTRIBUTES : GENERIC_READ,
                    directory ? FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE
                              : FILE_SHARE_READ | FILE_SHARE_DELETE,
                    nullptr,
                    OPEN_EXISTING,
                    directory ? FILE_FLAG_BACKUP_SEMANTICS : FILE_FLAG_SEQUENTIAL_SCAN,
                    nullptr)};
    return FileHandle{handle == INVALID_HANDLE_VALUE ? nullptr : handle};
}

}
