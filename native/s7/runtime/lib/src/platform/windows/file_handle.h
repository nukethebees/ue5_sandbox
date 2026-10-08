#pragma once
#include "../../script_file_handle.h"

#include <string>
namespace ioj::s7::detail {
[[nodiscard]] auto open_script_handle(std::wstring const& path, bool directory) -> FileHandle;
}
