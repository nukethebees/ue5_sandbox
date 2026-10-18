#pragma once

#include <codegen/schema/declaration_handle.h>
#include <codegen/schema/declaration_kind.h>
#include <codegen/schema/fixed_point_rounding.h>
#include <lispb/indices.h>

#include <cstdint>
#include <optional>
#include <vector>

namespace lispb::codegen {

struct FixedPointSchemas {
    [[nodiscard]] auto append() -> DeclarationHandle;
    [[nodiscard]] auto clone(DeclarationHandle id) -> DeclarationHandle;
    [[nodiscard]] auto index(DeclarationHandle id) const -> std::uint32_t;
    void clear();

    std::vector<IdentifierId> name;
    std::vector<std::uint8_t> signedness;
    std::vector<std::uint32_t> total_bits;
    std::vector<std::uint32_t> fractional_bits;
    std::vector<FixedPointRounding> rounding;
    std::vector<std::optional<StringId>> minimum_value;
    std::vector<std::optional<StringId>> maximum_value;
};

}
