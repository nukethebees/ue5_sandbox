#pragma once

#include <codegen/schema/module_handle.h>
#include <codegen/sexpr/indices.h>

#include <cstddef>
#include <vector>

namespace lispb::schema {

struct CreateModuleEdits {
    std::vector<codegen::sexpr::SourceFileId> source_files;
    std::vector<codegen::ModuleHandle> versions;
};

} // namespace lispb::schema
