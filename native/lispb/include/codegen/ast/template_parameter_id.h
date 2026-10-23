#pragma once
#include <codegen/ast/template_parameter_kind.h>

#include <cstdint>
namespace lispb::codegen {
struct TemplateParameterId {
    inline static constexpr std::uint32_t index_mask{0x00ffffffu};
    inline static constexpr std::uint32_t kind_shift{24};

    std::uint32_t value{};
    auto kind() const -> TemplateParameterKind {
        return static_cast<TemplateParameterKind>(value >> kind_shift);
    }
    auto index() const -> std::uint32_t { return value & index_mask; }
    auto operator==(TemplateParameterId const&) const -> bool = default;
};
static_assert(sizeof(TemplateParameterId) == 4);
}
