#pragma once

#include <codegen/schema.h>

namespace codegen::detail {

struct ResolvedMember {
    std::string name;
    SoaMemberKind kind;
    CppType element_type;
    CppType container_type;
    CppType view_type;
    CppType const_view_type;
};

} // namespace codegen::detail
