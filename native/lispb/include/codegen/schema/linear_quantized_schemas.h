#pragma once

#include <codegen/schema/declaration_handle.h>
#include <codegen/schema/declaration_kind.h>
#include <codegen/schema/indices.h>
#include <codegen/schema/quantization_clipping.h>
#include <lispb/indices.h>

#include <cstdint>
#include <vector>

namespace lispb::codegen {

struct LinearQuantizedSchemas {
    [[nodiscard]] auto append() -> DeclarationHandle;
    [[nodiscard]] auto clone(DeclarationHandle id) -> DeclarationHandle;
    [[nodiscard]] auto index(DeclarationHandle id) const -> std::uint32_t;
    void clear();

    std::vector<StringId> name;
    std::vector<TypeRefId> source;
    std::vector<std::uint32_t> bit_width;
    std::vector<std::uint64_t> reserved_codes;
    std::vector<QuantizationClipping> clipping;
};

}
