#pragma once

#include <codegen/ast/indices.h>
#include <codegen/schema/physical_type_use.h>

namespace lispb::codegen {

struct ResolvedCppTypeUse {
    CppTypeId cpp_type;
    PhysicalTypeUse physical;
};

} // namespace lispb::codegen
