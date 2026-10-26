#include <codegen/schema/linear_quantized_schemas.h>

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
    auto const source_index{index(id)};
    auto const result{append()};
    auto const destination{index(result)};
    cpp_projection[destination] = cpp_projection[source_index];
    name[destination] = name[source_index];
    source[destination] = source[source_index];
    bit_width[destination] = bit_width[source_index];
    reserved_codes[destination] = reserved_codes[source_index];
    clipping[destination] = clipping[source_index];
    return result;
}
void LinearQuantizedSchemas::clear() {
    cpp_projection.clear();
    name.clear();
    source.clear();
    bit_width.clear();
    reserved_codes.clear();
    clipping.clear();
}

}
