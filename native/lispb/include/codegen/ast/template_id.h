#pragma once
#include <codegen/ast/template_kind.h>

#include <cstdint>
namespace lispb::codegen {
struct TemplateId {
    inline static constexpr std::uint32_t index_mask{0x00ffffffu};
    inline static constexpr std::uint32_t kind_shift{24};

    std::uint32_t value{};
    auto kind() const -> TemplateKind { return static_cast<TemplateKind>(value >> kind_shift); }
    auto index() const -> std::uint32_t { return value & index_mask; }
    explicit operator bool() const { return value != 0; }
    auto operator==(TemplateId const&) const -> bool = default;
};
static_assert(sizeof(TemplateId) == 4);
}
