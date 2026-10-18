#pragma once

#include <codegen/schema/declaration_handle.h>
#include <codegen/schema/declaration_kind.h>
#include <lispb/indices.h>

#include <cstdint>
#include <vector>

namespace lispb::codegen {

struct MiniFloatSchemas {
    [[nodiscard]] auto append() -> DeclarationHandle;
    [[nodiscard]] auto clone(DeclarationHandle id) -> DeclarationHandle;
    [[nodiscard]] auto index(DeclarationHandle id) const -> std::uint32_t;
    void clear();

    std::vector<IdentifierId> name;
    std::vector<std::uint32_t> sign_bits;
    std::vector<std::uint32_t> exponent_bits;
    std::vector<std::uint32_t> significand_bits;
    std::vector<std::int32_t> exponent_bias;
};

}
