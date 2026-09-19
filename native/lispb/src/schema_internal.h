#pragma once

#include <filesystem>
#include <string>

namespace codegen::detail {

auto output_path_key(std::filesystem::path const& path) -> std::string;

} // namespace codegen::detail
