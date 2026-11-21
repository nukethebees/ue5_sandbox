#pragma once

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace ioj::s7 {
struct ModuleInput {
    std::filesystem::path path;
    std::optional<std::string_view> draft{};
    bool imported{};
};
struct InterpreterOptions {
    std::optional<std::string> script_library_root_utf8{};
    std::size_t max_loaded_file_bytes{1024 * 1024};
    std::size_t max_total_loaded_bytes{8 * 1024 * 1024};
    std::size_t max_loaded_files{1024};
    std::size_t max_load_depth{32};
    // Borrow for the interpreter lifetime. A completed import sets imported, even without a draft.
    ModuleInput* module_input{};
};
}
