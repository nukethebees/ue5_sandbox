#include <codegen/schema/fixed_point_schemas.h>

#include "schema_columns.h"

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
    cpp_projection.push_back({});
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
    return detail::clone_schema_row(*this,
                                    id,
                                    cpp_projection,
                                    name,
                                    signedness,
                                    total_bits,
                                    fractional_bits,
                                    rounding,
                                    minimum_value,
                                    maximum_value);
}
void FixedPointSchemas::clear() {
    detail::clear_schema_columns(cpp_projection,
                                 name,
                                 signedness,
                                 total_bits,
                                 fractional_bits,
                                 rounding,
                                 minimum_value,
                                 maximum_value);
}

}
