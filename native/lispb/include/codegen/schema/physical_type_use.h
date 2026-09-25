#pragma once

#include <codegen/ast/cpp_type.h>

#include <string>
#include <string_view>

namespace codegen {

enum class PhysicalTypeForm {
    value,
    object_pointer,
    lvalue_reference,
    rvalue_reference,
    unsupported
};

struct PhysicalTypeUse {
    PhysicalTypeForm form{PhysicalTypeForm::unsupported};
    std::string object_spelling;
    std::string diagnostic;
    bool names_semantic_type{};
    bool cv_qualified{};

    auto contains_value() const -> bool {
        return form == PhysicalTypeForm::value && names_semantic_type;
    }
};

struct ResolvedCppTypeUse {
    CppType cpp_type;
    PhysicalTypeUse physical;
};

auto classify_physical_type_use(std::string_view spelling) -> PhysicalTypeUse;

} // namespace codegen
