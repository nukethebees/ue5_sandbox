#pragma once

#include <codegen/source_loader.h>
#include <lispb/schema/schema_source_kind.h>

#include <filesystem>
#include <string>
#include <vector>

namespace lispb::schema {
struct SchemaSourceFile {
    std::filesystem::path path;
    std::string text;
    SchemaSourceKind kind{SchemaSourceKind::module};
    std::vector<codegen::RegistryInclude> includes;
};
}
