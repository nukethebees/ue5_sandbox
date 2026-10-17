#pragma once

#include <codegen/ast/cpp_type.h>
#include <codegen/schema/physical_type_use.h>
#include <lispb/schema/type_id.h>

namespace lispb::schema {
struct ResolvedTypeRef {
    TypeId type;
    codegen::CppType cpp_type;
    codegen::PhysicalTypeUse physical{};
};
}
