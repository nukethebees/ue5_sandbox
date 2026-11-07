#pragma once

#include <codegen/schema/cpp_projection.h>
#include <codegen/schema/declaration_handle.h>
#include <codegen/schema/declaration_kind.h>
#include <lispb/indexed_column.h>
#include <lispb/indices.h>

#include <cstdint>

namespace lispb::codegen {

struct MiniFloatSchemas {
    using Indexing = TaggedIndex<DeclarationHandle, DeclarationKind::mini_float>;
    template <typename T>
    using Column = IndexedColumn<T, Indexing>;

    [[nodiscard]] auto append() -> DeclarationHandle;
    [[nodiscard]] auto clone(DeclarationHandle id) -> DeclarationHandle;
    [[nodiscard]] auto index(DeclarationHandle id) const -> std::uint32_t;
    void clear();

    Column<StringId> name;
    Column<std::optional<CppProjection>> cpp_projection;
    Column<std::uint32_t> sign_bits;
    Column<std::uint32_t> exponent_bits;
    Column<std::uint32_t> significand_bits;
    Column<std::int32_t> exponent_bias;
};

}
