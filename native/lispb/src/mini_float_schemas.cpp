#include <codegen/schema/mini_float_schemas.h>

#include "schema_columns.h"

#include <cassert>

namespace lispb::codegen {

auto MiniFloatSchemas::index(DeclarationHandle id) const -> std::uint32_t {
    auto const row{Indexing::offset(id)};
    assert(row < name.size());
    return row;
}
auto MiniFloatSchemas::append() -> DeclarationHandle {
    auto const index{static_cast<std::uint32_t>(name.size())};
    assert(index < 0x00fffffe);
    cpp_projection.push_back({});
    name.push_back({});
    sign_bits.push_back({});
    exponent_bits.push_back({});
    significand_bits.push_back({});
    exponent_bias.push_back({});
    return DeclarationHandle{(static_cast<std::uint32_t>(DeclarationKind::mini_float) << 24) |
                             (index + 1)};
}
auto MiniFloatSchemas::clone(DeclarationHandle id) -> DeclarationHandle {
    return detail::clone_schema_row(
        *this, id, cpp_projection, name, sign_bits, exponent_bits, significand_bits, exponent_bias);
}
void MiniFloatSchemas::clear() {
    detail::clear_schema_columns(
        cpp_projection, name, sign_bits, exponent_bits, significand_bits, exponent_bias);
}

}
