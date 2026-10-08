#include "../../script_files.h"

#include "canonical_path.h"
#include "utf_encoding.h"
#include <ioj/ascii.h>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <utility>

namespace ioj::s7::detail {
[[nodiscard]] auto path_key(std::string key) -> std::string {
    std::ranges::transform(key, key.begin(), ioj::to_ascii_lower);
    return key;
}

namespace {
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
    auto const root_handle{open_script_handle(*root_source, true)};
    auto const root{canonical_path(root_handle, true)};
    if (!root.has_value()) {
        return std::unexpected("The configured script library root is unavailable.");
    }

    auto requested_path_wide{utf8_to_wide(requested_path)};
    if (!requested_path_wide.has_value()) {
        return std::unexpected("The requested script library file is unavailable.");
    }
    std::ranges::replace(*requested_path_wide, L'/', L'\\');
    auto handle{open_script_handle(root->wide_path + L'\\' + *requested_path_wide, false)};
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
