#pragma once

#include <codegen/schema.h>

#include <filesystem>
#include <span>

namespace codegen {

auto load_manifest(std::filesystem::path const& path) -> Manifest;
auto load_sources(std::filesystem::path const& types,
                  std::span<std::filesystem::path const> modules) -> Manifest;

} // namespace codegen
