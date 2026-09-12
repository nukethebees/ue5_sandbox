#pragma once

#include <codegen/schema.h>

#include <filesystem>

namespace codegen {

auto load_manifest(std::filesystem::path const& path) -> Manifest;

} // namespace codegen
