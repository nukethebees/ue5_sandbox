#pragma once

#include <codegen/schema/module_schema.h>

#include <cstddef>

namespace lispb::schema {
struct CreateModule {
    std::size_t source_file_index{};
    codegen::ModuleSchema schema;
};
}
