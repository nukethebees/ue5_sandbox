#pragma once
#include <codegen/ast/template_parameter_kind.h>

#include <cstdint>
namespace lispb::codegen {
struct TemplateParameterId {
    std::uint32_t value{};
    auto kind() const -> TemplateParameterKind {
        return static_cast<TemplateParameterKind>(value >> 24);
    }
    auto index() const -> std::uint32_t { return value & 0x00ffffffU; }
    auto operator==(TemplateParameterId const&) const -> bool = default;
};
static_assert(sizeof(TemplateParameterId) == 4);
}
