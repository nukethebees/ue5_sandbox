#include <codegen/schema/registered_types.h>

#include <codegen/schema/declarations.h>
#include <codegen/schema/external_scalar_kind.h>

#include <algorithm>
#include <cassert>
#include <cstddef>

namespace lispb::codegen {

auto RegisteredTypes::append(CppTypeId const type, ExternalScalarId const scalar)
    -> RegisteredTypeId {
    auto const id{RegisteredTypeId{static_cast<std::uint32_t>(cpp_type.size()) + 1}};
    cpp_type.push_back(type);
    semantics.push_back(scalar);
    return id;
}
auto RegisteredTypes::index(RegisteredTypeId const id) const -> std::uint32_t {
    assert(id.value > 0 && id.value <= cpp_type.size());
    return id.value - 1;
}
void RegisteredTypes::clear() {
    cpp_type.clear();
    semantics.clear();
    integer_signedness.clear();
    integer_minimums.clear();
    integer_maximums.clear();
    integer_widths.clear();
    integer_codes.clear();
}

auto RegisteredTypes::add_integer(bool const signedness,
                                  PackedIntegerValue const minimum,
                                  PackedIntegerValue const maximum,
                                  std::uint8_t const width,
                                  SchemaRange<PackedNamedCodeId> const codes) -> ExternalScalarId {
    auto const count{integer_signedness.size()};
    for (std::size_t index{}; index < count; ++index) {
        auto const range{integer_codes[index]};
        if (integer_signedness[index] == signedness && integer_minimums[index] == minimum &&
            integer_maximums[index] == maximum && integer_widths[index] == width &&
            std::ranges::equal(declarations.items(range),
                               declarations.items(codes),
                               [&](PackedNamedCodeId const left, PackedNamedCodeId const right) {
                                   auto const& values{declarations.packed_named_codes};
                                   auto const lhs{values.index(left)};
                                   auto const rhs{values.index(right)};
                                   return values.name[lhs] == values.name[rhs] &&
                                          values.value[lhs] == values.value[rhs] &&
                                          values.sentinel[lhs] == values.sentinel[rhs];
                               })) {
            return {static_cast<std::uint32_t>(ExternalScalarKind::integer) << 24 |
                    static_cast<std::uint32_t>(index)};
        }
    }

    assert(count < 0x01000000);
    auto const id{ExternalScalarId{static_cast<std::uint32_t>(ExternalScalarKind::integer) << 24 |
                                   static_cast<std::uint32_t>(count)}};
    integer_signedness.push_back(signedness);
    integer_minimums.push_back(minimum);
    integer_maximums.push_back(maximum);
    integer_widths.push_back(width);
    integer_codes.push_back(codes);
    return id;
}
auto RegisteredTypes::add_floating(FloatingPointFormat const format) -> ExternalScalarId {
    return {static_cast<std::uint32_t>(ExternalScalarKind::floating) << 24 |
            static_cast<std::uint32_t>(format)};
}
auto RegisteredTypes::integer_index(ExternalScalarId const scalar) const -> std::uint32_t {
    assert(scalar.kind() == ExternalScalarKind::integer);
    auto const index{scalar.index()};
    assert(index < integer_signedness.size());
    return index;
}

auto RegisteredTypes::floating(ExternalScalarId const scalar) -> FloatingPointFormat {
    assert(scalar.kind() == ExternalScalarKind::floating);
    return static_cast<FloatingPointFormat>(scalar.index());
}

}
