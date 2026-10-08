#pragma once

#include <expected>
#include <filesystem>
#include <string>
#include <system_error>

namespace ioj {
enum class FileReadErrorCode { OpenFailed, ReadFailed };
struct FileReadError {
    FileReadErrorCode code{};
    std::filesystem::path source_path{};
    std::error_code system_error{};
};

// Read all bytes without newline or encoding conversion. An empty file succeeds.
[[nodiscard]] auto read_file(std::filesystem::path const& path)
    -> std::expected<std::string, FileReadError>;
// Convert the native filesystem encoding explicitly; never use the Windows ANSI code page.
[[nodiscard]] auto path_to_utf8(std::filesystem::path const& path) -> std::string;
}
