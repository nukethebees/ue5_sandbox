#pragma once

#include "syntax.h"

#include <filesystem>
#include <string>

namespace kernel_codegen::detail {

auto read_file(std::filesystem::path const& path) -> std::string;
auto load_manifest(std::filesystem::path const& path) -> Manifest;

}
