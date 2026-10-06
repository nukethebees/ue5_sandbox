#pragma once

#include <cstddef>
#include <expected>
#include <memory>
#include <string>
#include <string_view>

namespace ml::s7::detail {
struct FileHandleCloser {
    void operator()(void* handle) const;
};
using FileHandle = std::unique_ptr<void, FileHandleCloser>;

struct ScriptFile {
    FileHandle handle;
    std::string narrow_path;
    std::size_t file_size{};
};

using ScriptFileResult = std::expected<ScriptFile, std::string>;
using ScriptSourceResult = std::expected<std::string, std::string>;

[[nodiscard]] auto path_key(std::string key) -> std::string;
// Open and validate the same object subsequently read by read_script_source.
// Keep the handle alive until capture finishes; writes are denied while open.
[[nodiscard]] auto open_script_file(std::string_view root_utf8, std::string_view requested_path)
    -> ScriptFileResult;
// Call once, after admission has reserved file_size bytes; errors retain no source.
[[nodiscard]] auto read_script_source(ScriptFile const& file) -> ScriptSourceResult;
}
