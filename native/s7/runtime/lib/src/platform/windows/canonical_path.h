#pragma once

#include "file_handle.h"

#include <optional>
#include <string>

namespace ioj::s7::detail {
struct CanonicalPath {
    std::wstring wide_path;
    std::string narrow_path;
    std::size_t file_size{};
};
[[nodiscard]] auto canonical_path(FileHandle const& handle, bool require_directory)
    -> std::optional<CanonicalPath>;
}
