#include <codegen/schema/linear_quantized_schemas.h>

#include "schema_columns.h"

#include <cassert>

namespace lispb::codegen {

auto LinearQuantizedSchemas::index(DeclarationHandle id) const -> std::uint32_t {
    auto const row{Indexing::offset(id)};
    assert(row < name.size());
    return row;
}
auto LinearQuantizedSchemas::append(StringId const name_value,
                                    TypeRefId const source_value,
                                    BitWidth const bits,
                                    std::uint64_t const reserved,
                                    QuantizationClipping const clipping_value,
                                    std::optional<CppProjection> const projection)
    -> DeclarationHandle {
    auto const index{static_cast<std::uint32_t>(name.size())};
    assert(index < 0x00fffffe);
    cpp_projection.push_back(projection);
    name.push_back(name_value);
    source.push_back(source_value);
    bit_width.push_back(bits);
    reserved_codes.push_back(reserved);
    clipping.push_back(clipping_value);
    return DeclarationHandle{(static_cast<std::uint32_t>(DeclarationKind::linear_quantized) << 24) |
                             (index + 1)};
}
auto LinearQuantizedSchemas::clone(DeclarationHandle id) -> DeclarationHandle {
    return append(
        name[id], source[id], bit_width[id], reserved_codes[id], clipping[id], cpp_projection[id]);
}
void LinearQuantizedSchemas::clear() {
    detail::clear_schema_columns(cpp_projection, name, source, bit_width, reserved_codes, clipping);
}

}
