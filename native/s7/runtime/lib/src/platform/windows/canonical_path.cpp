#include "canonical_path.h"

#include "utf_encoding.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <utility>

namespace ioj::s7::detail {
namespace {
[[nodiscard]] auto strip_extended_path_prefix(std::wstring path) -> std::wstring {
    constexpr std::wstring_view unc_prefix{LR"(\\?\UNC\)"};
    constexpr std::wstring_view local_prefix{LR"(\\?\)"};
    if (path.starts_with(unc_prefix)) {
        path.erase(0, unc_prefix.size());
        path.insert(path.begin(), 2, L'\\');
    } else if (path.starts_with(local_prefix)) {
        path.erase(0, local_prefix.size());
    }
    return path;
}

}

[[nodiscard]] auto canonical_path(FileHandle const& handle, bool const require_directory)
    -> std::optional<CanonicalPath> {
    if (!handle) {
        return std::nullopt;
    }

    BY_HANDLE_FILE_INFORMATION file_information{};
    auto const has_information{GetFileInformationByHandle(handle.get(), &file_information) != 0};
    auto const is_directory{(file_information.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0};
    auto const required_size{GetFinalPathNameByHandleW(
        handle.get(), nullptr, 0, FILE_NAME_NORMALIZED | VOLUME_NAME_DOS)};
    if (!has_information || is_directory != require_directory || required_size == 0) {
        return std::nullopt;
    }

    std::wstring resolved(static_cast<std::size_t>(required_size), L'\0');
    auto const copied{GetFinalPathNameByHandleW(
        handle.get(), resolved.data(), required_size, FILE_NAME_NORMALIZED | VOLUME_NAME_DOS)};
    if (copied == 0 || copied >= required_size) {
        return std::nullopt;
    }
    resolved.resize(copied);
    resolved = strip_extended_path_prefix(std::move(resolved));

    auto narrow{wide_to_utf8(resolved)};
    auto const size{(static_cast<std::uint64_t>(file_information.nFileSizeHigh) << 32U) |
                    file_information.nFileSizeLow};
    if (!narrow.has_value() || size > std::numeric_limits<std::size_t>::max()) {
        return std::nullopt;
    }
    return CanonicalPath{
        .wide_path = std::move(resolved),
        .narrow_path = std::move(*narrow),
        .file_size = static_cast<std::size_t>(size),
    };
}

}
