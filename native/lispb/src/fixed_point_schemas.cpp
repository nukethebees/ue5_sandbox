#include <codegen/schema/fixed_point_schemas.h>

#include <cassert>

namespace lispb::codegen {

auto FixedPointSchemas::index(DeclarationHandle id) const -> std::uint32_t {
    auto const row{Indexing::offset(id)};
    assert(row < name.size());
    return row;
}
auto FixedPointSchemas::append() -> DeclarationHandle {
    auto const index{static_cast<std::uint32_t>(name.size())};
    assert(index < 0x00fffffe);
    name.push_back({});
    signedness.push_back({});
    total_bits.push_back({});
    fractional_bits.push_back({});
    rounding.push_back(FixedPointRounding::nearest_even);
    minimum_value.push_back(std::nullopt);
    maximum_value.push_back(std::nullopt);
    return DeclarationHandle{(static_cast<std::uint32_t>(DeclarationKind::fixed_point) << 24) |
                             (index + 1)};
}
auto FixedPointSchemas::clone(DeclarationHandle id) -> DeclarationHandle {
    auto const source_index{index(id)};
    auto const result{append()};
    auto const destination{index(result)};
    name[destination] = name[source_index];
    signedness[destination] = signedness[source_index];
    total_bits[destination] = total_bits[source_index];
    fractional_bits[destination] = fractional_bits[source_index];
    rounding[destination] = rounding[source_index];
    minimum_value[destination] = minimum_value[source_index];
    maximum_value[destination] = maximum_value[source_index];
    return result;
}
void FixedPointSchemas::clear() {
    name.clear();
    signedness.clear();
    total_bits.clear();
    fractional_bits.clear();
    rounding.clear();
    minimum_value.clear();
    maximum_value.clear();
}

}
