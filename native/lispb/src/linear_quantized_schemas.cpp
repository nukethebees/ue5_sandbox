#include <codegen/schema/linear_quantized_schemas.h>

#include "schema_columns.h"

#include <cassert>

namespace lispb::codegen {

auto LinearQuantizedSchemas::index(DeclarationHandle id) const -> std::uint32_t {
    auto const row{Indexing::offset(id)};
    assert(row < name.size());
    return row;
}
auto LinearQuantizedSchemas::append() -> DeclarationHandle {
    auto const index{static_cast<std::uint32_t>(name.size())};
    assert(index < 0x00fffffe);
    cpp_projection.push_back({});
    name.push_back({});
    source.push_back({});
    bit_width.push_back({});
    reserved_codes.push_back({});
    clipping.push_back(QuantizationClipping::reject);
    return DeclarationHandle{(static_cast<std::uint32_t>(DeclarationKind::linear_quantized) << 24) |
                             (index + 1)};
}
auto LinearQuantizedSchemas::clone(DeclarationHandle id) -> DeclarationHandle {
    return detail::clone_schema_row(
        *this, id, cpp_projection, name, source, bit_width, reserved_codes, clipping);
}
void LinearQuantizedSchemas::clear() {
    detail::clear_schema_columns(cpp_projection, name, source, bit_width, reserved_codes, clipping);
}

}
