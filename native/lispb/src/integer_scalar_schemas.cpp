#include <codegen/schema/integer_scalar_schemas.h>

#include <cassert>

namespace lispb::codegen {

auto IntegerScalarSchemas::index(DeclarationHandle id) const -> std::uint32_t {
    auto const row{Indexing::offset(id)};
    assert(row < name.size());
    return row;
}
auto IntegerScalarSchemas::append() -> DeclarationHandle {
    auto const index{static_cast<std::uint32_t>(name.size())};
    assert(index < 0x00fffffe);
    cpp_projection.push_back({});
    name.push_back({});
    signedness.push_back({});
    minimum_value.push_back({});
    maximum_value.push_back({});
    bit_width.push_back({});
    named_codes.push_back({});
    relationship.push_back({});
    return DeclarationHandle{(static_cast<std::uint32_t>(DeclarationKind::integer_scalar) << 24) |
                             (index + 1)};
}
auto IntegerScalarSchemas::clone(DeclarationHandle id) -> DeclarationHandle {
    auto const source_index{index(id)};
    auto const result{append()};
    auto const destination{index(result)};
    cpp_projection[destination] = cpp_projection[source_index];
    name[destination] = name[source_index];
    signedness[destination] = signedness[source_index];
    minimum_value[destination] = minimum_value[source_index];
    maximum_value[destination] = maximum_value[source_index];
    bit_width[destination] = bit_width[source_index];
    named_codes[destination] = named_codes[source_index];
    relationship[destination] = relationship[source_index];
    return result;
}
void IntegerScalarSchemas::clear() {
    cpp_projection.clear();
    name.clear();
    signedness.clear();
    minimum_value.clear();
    maximum_value.clear();
    bit_width.clear();
    named_codes.clear();
    relationship.clear();
}

}
