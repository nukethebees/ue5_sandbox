#include <lispb/schema/linear_quantized_types.h>

#include <cassert>

namespace lispb::schema {

auto LinearQuantizedTypes::append() -> TypeDefinitionId {
    auto const row{static_cast<std::uint32_t>(source.size())};
    source.push_back({});
    bit_width.push_back({});
    reserved_codes.push_back({});
    clipping.push_back(codegen::QuantizationClipping::reject);
    return TypeDefinitionId{
        (static_cast<std::uint32_t>(TypeDefinitionKind::linear_quantized) << 24) | row};
}
auto LinearQuantizedTypes::index(TypeDefinitionId const id) const -> std::uint32_t {
    assert(id.kind() == TypeDefinitionKind::linear_quantized);
    auto const row{id.index()};
    assert(row < source.size());
    return row;
}

} // namespace lispb::schema
