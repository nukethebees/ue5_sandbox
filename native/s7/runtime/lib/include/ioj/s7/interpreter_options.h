#pragma once

#include <cstddef>
#include <optional>
#include <string>

namespace ioj::s7 {
struct InterpreterOptions {
    std::optional<std::string> script_library_root_utf8{};
    std::size_t max_loaded_file_bytes{1024 * 1024};
    std::size_t max_total_loaded_bytes{8 * 1024 * 1024};
    std::size_t max_loaded_files{64};
    std::size_t max_load_depth{32};
};
}
