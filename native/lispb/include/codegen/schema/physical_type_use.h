#pragma once

#include <codegen/ast/indices.h>
#include <codegen/schema/physical_type_form.h>
#include <lispb/indices.h>

#include <string>

namespace lispb::codegen {

class CppTypes;

struct PhysicalTypeUse {
    PhysicalTypeForm form{PhysicalTypeForm::unsupported};
    StringId object_spelling;
    bool names_semantic_type{};
    bool cv_qualified{};

    auto contains_value() const -> bool {
        return form == PhysicalTypeForm::value && names_semantic_type;
    }
};

auto classify_physical_type_use(CppTypes& types, CppTypeId type) -> PhysicalTypeUse;
auto physical_type_diagnostic(CppTypes const& types, CppTypeId type) -> std::string;

} // namespace lispb::codegen
