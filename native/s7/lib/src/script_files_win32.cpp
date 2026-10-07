#include "script_files_win32.h"
#if !defined(_WIN32)
#error The sandboxed s7 loader currently requires Windows.
#endif
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <limits>
#include <optional>
#include <utility>

namespace ioj::s7::detail {
struct CanonicalPath {
    std::wstring wide_path;
    std::string narrow_path;
    std::size_t file_size{};
};

void FileHandleCloser::operator()(void* const handle) const {
    CloseHandle(handle);
}

[[nodiscard]] auto open_file(std::wstring const& path, bool const directory) -> FileHandle {
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

[[nodiscard]] auto utf8_to_wide(std::string_view const value) -> std::optional<std::wstring> {
    if (value.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        return std::nullopt;
    }
    auto const input_size{static_cast<int>(value.size())};
    auto const output_size{
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), input_size, nullptr, 0)};
    if (output_size <= 0) {
        return std::nullopt;
    }

    std::wstring result(static_cast<std::size_t>(output_size), L'\0');
    if (MultiByteToWideChar(
            CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), input_size, result.data(), output_size) !=
        output_size) {
        return std::nullopt;
    }
    return result;
}

[[nodiscard]] auto wide_to_utf8(std::wstring_view const value) -> std::optional<std::string> {
    if (value.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        return std::nullopt;
    }
    auto const input_size{static_cast<int>(value.size())};
    auto const output_size{WideCharToMultiByte(
        CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), input_size, nullptr, 0, nullptr, nullptr)};
    if (output_size <= 0) {
        return std::nullopt;
    }

    std::string result(static_cast<std::size_t>(output_size), '\0');
    if (WideCharToMultiByte(CP_UTF8,
                            WC_ERR_INVALID_CHARS,
                            value.data(),
                            input_size,
                            result.data(),
                            output_size,
                            nullptr,
                            nullptr) != output_size) {
        return std::nullopt;
    }
    return result;
}

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

[[nodiscard]] auto path_key(std::string key) -> std::string {
    std::ranges::transform(key, key.begin(), [](unsigned char const character) {
        return static_cast<char>(std::tolower(character));
    });
    return key;
}

[[nodiscard]] auto is_within_root(std::wstring_view const path, std::wstring_view const root)
    -> bool {
    if (path.size() <= root.size() || path[root.size()] != L'\\') {
        return false;
    }
    return CompareStringOrdinal(path.data(),
                                static_cast<int>(root.size()),
                                root.data(),
                                static_cast<int>(root.size()),
                                true) == CSTR_EQUAL;
}
auto open_script_file(std::string_view const root_utf8, std::string_view const requested_path)
    -> ScriptFileResult {
    if (requested_path.empty() || requested_path.front() == '/' || requested_path.front() == '\\' ||
        requested_path.contains(':') || requested_path.contains('\0')) {
        return std::unexpected("load-script requires a relative .scm path.");
    }
    std::size_t component_start{};
    auto const path_size{requested_path.size()};
    for (std::size_t index{}; index <= path_size; ++index) {
        if (index != path_size && requested_path[index] != '/' && requested_path[index] != '\\') {
            continue;
        }

        auto const component{requested_path.substr(component_start, index - component_start)};
        if (component.empty() || component == "." || component == "..") {
            return std::unexpected("load-script paths cannot contain '.' or '..'.");
        }
        component_start = index + 1;
    }
    if (!requested_path.ends_with(".scm")) {
        return std::unexpected("load-script only accepts .scm files.");
    }

    auto const root_source{utf8_to_wide(root_utf8)};
    if (!root_source.has_value()) {
        return std::unexpected("The configured script library root is unavailable.");
    }
    auto const root_handle{open_file(*root_source, true)};
    auto const root{canonical_path(root_handle, true)};
    if (!root.has_value()) {
        return std::unexpected("The configured script library root is unavailable.");
    }

    auto requested_path_wide{utf8_to_wide(requested_path)};
    if (!requested_path_wide.has_value()) {
        return std::unexpected("The requested script library file is unavailable.");
    }
    std::ranges::replace(*requested_path_wide, L'/', L'\\');
    auto handle{open_file(root->wide_path + L'\\' + *requested_path_wide, false)};
    auto candidate{canonical_path(handle, false)};
    if (!candidate.has_value() || !is_within_root(candidate->wide_path, root->wide_path)) {
        return std::unexpected("The requested script library file is unavailable.");
    }

    return ScriptFileResult{
        std::in_place, std::move(handle), std::move(candidate->narrow_path), candidate->file_size};
}

auto read_script_source(ScriptFile const& file) -> ScriptSourceResult {
    std::string source(file.file_size, '\0');
    std::size_t offset{};
    while (offset < file.file_size) {
        auto const count{
            static_cast<DWORD>(std::min(file.file_size - offset, std::size_t{1024 * 1024}))};
        DWORD received{};
        if (!ReadFile(file.handle.get(), source.data() + offset, count, &received, nullptr) ||
            received == 0) {
            return ScriptSourceResult{std::unexpect,
                                      "The requested script library file could not be read."};
        }
        offset += received;
    }
    return source;
}
}
