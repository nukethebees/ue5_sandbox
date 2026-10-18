#pragma once

#include <filesystem>
#include <string>

namespace lispb::codegen {

auto output_path_key(std::filesystem::path const& path) -> std::string;

}
