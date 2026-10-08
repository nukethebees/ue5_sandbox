#pragma once

#include <memory>
#include <string>

namespace ioj::s7::detail {
struct FileHandleCloser {
    void operator()(void* handle) const;
};
using FileHandle = std::unique_ptr<void, FileHandleCloser>;
// Script handles deny writes until capture completes; directory handles permit metadata lookup.
[[nodiscard]] auto open_script_handle(std::wstring const& path, bool directory) -> FileHandle;
}
