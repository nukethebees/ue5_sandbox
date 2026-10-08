#pragma once

#include "file_handle.h"

#include <string>

namespace ioj::s7::detail {
struct ScriptFile {
    FileHandle handle;
    std::string narrow_path;
    std::size_t file_size{};
};
}
