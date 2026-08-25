#pragma once

#include <codegen/schema.h>

#include <filesystem>
#include <stdexcept>
#include <string>

namespace codegen {

class ManifestError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

auto load_manifest(std::filesystem::path const& path) -> Manifest;

} // namespace codegen
