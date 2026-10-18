#include <codegen/schema/integer_scalar_schemas.h>

#include <cassert>

namespace lispb::codegen {

auto IntegerScalarSchemas::index(DeclarationHandle id) const -> std::uint32_t {
    assert(id.kind() == DeclarationKind::integer_scalar);
    auto const row{id.index()};
    assert(row < name.size());
    return row;
}
auto IntegerScalarSchemas::append() -> DeclarationHandle {
    auto const index{static_cast<std::uint32_t>(name.size())};
    assert(index < 0x00fffffe);
    name.push_back({});
    signedness.push_back({});
    minimum_value.push_back({});
    maximum_value.push_back({});
    bit_width.push_back({});
    named_codes.push_back({});
    relationship.push_back({});
    cpp_emission.push_back(IntegerScalarCppEmission::none);
    cpp_type.push_back({});
    return DeclarationHandle{(static_cast<std::uint32_t>(DeclarationKind::integer_scalar) << 24) |
                             (index + 1)};
}
auto IntegerScalarSchemas::clone(DeclarationHandle id) -> DeclarationHandle {
    auto const source_index{index(id)};
    auto const result{append()};
    auto const destination{index(result)};
    name[destination] = name[source_index];
    signedness[destination] = signedness[source_index];
    minimum_value[destination] = minimum_value[source_index];
    maximum_value[destination] = maximum_value[source_index];
    bit_width[destination] = bit_width[source_index];
    named_codes[destination] = named_codes[source_index];
    relationship[destination] = relationship[source_index];
    cpp_emission[destination] = cpp_emission[source_index];
    cpp_type[destination] = cpp_type[source_index];
    return result;
}
void IntegerScalarSchemas::clear() {
    name.clear();
    signedness.clear();
    minimum_value.clear();
    maximum_value.clear();
    bit_width.clear();
    named_codes.clear();
    relationship.clear();
    cpp_emission.clear();
    cpp_type.clear();
}

}
