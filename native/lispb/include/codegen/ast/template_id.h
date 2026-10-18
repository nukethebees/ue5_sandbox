#pragma once
#include <codegen/ast/template_kind.h>

#include <cstdint>
namespace lispb::codegen {
struct TemplateId {
    std::uint32_t value{};
    auto kind() const -> TemplateKind { return static_cast<TemplateKind>(value >> 24); }
    auto index() const -> std::uint32_t { return value & 0x00ffffffU; }
    explicit operator bool() const { return value != 0; }
    auto operator==(TemplateId const&) const -> bool = default;
};
static_assert(sizeof(TemplateId) == 4);
}
