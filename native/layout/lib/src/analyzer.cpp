#include "analyzer_internal.hpp"

namespace ioj::layout {

auto physical_type_spelling(lispb::schema::TypeGraph const& types, lispb::schema::TypeId const type)
    -> std::optional<std::string> {
    std::vector<lispb::schema::TypeId> visited;
    auto current{type};
    while (current.valid() && std::ranges::find(visited, current) == visited.end()) {
        visited.push_back(current);
        auto const& node{types.type(current)};
        if (auto const* external{std::get_if<lispb::schema::ExternalType>(&node.definition)}) {
            return codegen::native_spelling(external->cpp_type.spelling);
        }
        if (auto const* enumeration{std::get_if<lispb::schema::EnumType>(&node.definition)}) {
            if (enumeration->underlying_type.has_value()) {
                if (!enumeration->underlying_type->physical.contains_value()) {
                    return enumeration->underlying_type->cpp_type.spelling;
                }
                current = enumeration->underlying_type->type;
                continue;
            }
            return derived_enum_backing_type(*enumeration);
        }
        if (auto const* packed{std::get_if<lispb::schema::PackedType>(&node.definition)}) {
            if (!packed->storage_type.physical.contains_value()) {
                return packed->storage_type.cpp_type.spelling;
            }
            current = packed->storage_type.type;
            continue;
        }
        if (auto const* scalar{std::get_if<lispb::schema::IntegerScalarType>(&node.definition)};
            scalar != nullptr && scalar->cpp_representation.has_value()) {
            if (!scalar->cpp_representation->physical.contains_value()) {
                return scalar->cpp_representation->cpp_type.spelling;
            }
            current = scalar->cpp_representation->type;
            continue;
        }
        return std::nullopt;
    }
    return std::nullopt;
}

auto physical_type_spelling(lispb::schema::TypeGraph const& types,
                            lispb::schema::ResolvedTypeRef const& use)
    -> std::optional<std::string> {
    return use.physical.contains_value() ? physical_type_spelling(types, use.type)
                                         : std::optional{use.cpp_type.spelling};
}

auto numeric_delta(std::optional<std::uint64_t> const baseline,
                   std::optional<std::uint64_t> const variant) -> std::optional<NumericDelta> {
    if (!baseline.has_value() || !variant.has_value()) {
        return std::nullopt;
    }
    if (*baseline == *variant) {
        return NumericDelta{.direction = NumericDeltaDirection::unchanged,
                            .magnitude = 0,
                            .percentage =
                                *baseline == 0 ? std::nullopt : std::optional<double>{0.0}};
    }
    auto const increased{*variant > *baseline};
    auto const magnitude{increased ? *variant - *baseline : *baseline - *variant};
    return NumericDelta{.direction = increased ? NumericDeltaDirection::increased
                                               : NumericDeltaDirection::decreased,
                        .magnitude = magnitude,
                        .percentage = *baseline == 0 ? std::nullopt
                                                     : std::optional<double>{
                                                           static_cast<double>(magnitude) * 100.0 /
                                                           static_cast<double>(*baseline)}};
}

} // namespace ioj::layout
