#include "packed_value_internal.h"

#include <algorithm>
#include <limits>
#include <string>

namespace codegen::detail {
namespace {

auto qualified_enum_name(EnumModuleSchema const& module, EnumSchema const& schema) -> std::string {
    return module.settings.namespace_name.has_value()
             ? *module.settings.namespace_name + "::" + schema.name
             : schema.name;
}

} // namespace

auto packed_unsigned_width(std::string_view const spelling) -> std::optional<int> {
    static std::map<std::string_view, int> const widths{
        {"uint8", 8},
        {"uint16", 16},
        {"uint32", 32},
        {"uint64", 64},
        {"std::uint8_t", 8},
        {"std::uint16_t", 16},
        {"std::uint32_t", 32},
        {"std::uint64_t", 64},
    };
    auto const found{widths.find(spelling)};
    return found == widths.end() ? std::nullopt : std::optional<int>{found->second};
}

auto enum_integer_type(std::string_view const spelling) -> std::optional<EnumIntegerType> {
    static std::map<std::string_view, EnumIntegerType> const types{
        {"int8", {7, false}},
        {"std::int8_t", {7, false}},
        {"int16", {15, false}},
        {"std::int16_t", {15, false}},
        {"int32", {31, false}},
        {"std::int32_t", {31, false}},
        {"int64", {63, false}},
        {"std::int64_t", {63, false}},
        {"uint8", {8, true}},
        {"std::uint8_t", {8, true}},
        {"uint16", {16, true}},
        {"std::uint16_t", {16, true}},
        {"uint32", {32, true}},
        {"std::uint32_t", {32, true}},
        {"uint64", {64, true}},
        {"std::uint64_t", {64, true}},
    };
    if (spelling == "int") {
        return EnumIntegerType{std::numeric_limits<int>::digits, false};
    }
    auto const found{types.find(spelling)};
    return found == types.end() ? std::nullopt : std::optional<EnumIntegerType>{found->second};
}

auto resolve_numeric_literal(std::string_view const source) -> std::optional<EnumNumericValue> {
    if (source.empty()) {
        return std::nullopt;
    }

    auto const is_negative{source.front() == '-'};
    auto const magnitude_source{is_negative ? source.substr(1) : source};
    if (magnitude_source.empty()) {
        return std::nullopt;
    }
    try {
        std::size_t parsed{};
        auto const magnitude{std::stoull(std::string{magnitude_source}, &parsed, 0)};
        if (parsed != magnitude_source.size()) {
            return std::nullopt;
        }
        return EnumNumericValue{magnitude, is_negative && magnitude != 0};
    } catch (std::invalid_argument const&) {
        return std::nullopt;
    } catch (std::out_of_range const&) {
        return std::nullopt;
    }
}

auto next_enum_value(EnumNumericValue const value) -> std::optional<EnumNumericValue> {
    if (value.is_negative) {
        return value.magnitude == 1 ? EnumNumericValue{0, false}
                                    : EnumNumericValue{value.magnitude - 1, true};
    }
    if (value.magnitude == std::numeric_limits<std::uint64_t>::max()) {
        return std::nullopt;
    }
    return EnumNumericValue{value.magnitude + 1, false};
}

auto resolve_enum_values(EnumSchema const& schema) -> std::vector<std::optional<EnumNumericValue>> {
    std::vector<std::optional<EnumNumericValue>> result;
    result.reserve(schema.values.size());
    std::optional<EnumNumericValue> next_value{EnumNumericValue{0, false}};
    for (auto const& value : schema.values) {
        auto resolved{value.initializer.has_value() ? resolve_numeric_literal(*value.initializer)
                                                    : next_value};
        result.push_back(resolved);
        next_value = resolved.has_value() ? next_enum_value(*resolved) : std::nullopt;
    }
    return result;
}

auto enum_value_fits(EnumNumericValue const value, EnumIntegerType const type) -> bool {
    auto const positive_max{type.value_bits == 64 ? std::numeric_limits<std::uint64_t>::max()
                                                  : (std::uint64_t{1} << type.value_bits) - 1};
    if (type.is_unsigned) {
        return !value.is_negative && value.magnitude <= positive_max;
    }
    return value.is_negative ? value.magnitude <= positive_max + 1
                             : value.magnitude <= positive_max;
}

auto enum_required_packed_bits(EnumSchema const& schema) -> std::optional<int> {
    auto const values{resolve_enum_values(schema)};
    std::uint64_t maximum_value{};
    for (std::size_t index{}; index < schema.values.size(); ++index) {
        auto const& value{schema.values[index]};
        if (!schema.count.has_value() || value.name != *schema.count) {
            if (!values[index].has_value() || values[index]->is_negative) {
                return std::nullopt;
            }
            maximum_value = std::max(maximum_value, values[index]->magnitude);
        }
    }

    int result{};
    while (maximum_value != 0) {
        ++result;
        maximum_value >>= 1;
    }
    return result;
}

auto find_packed_enum(TypeRef const& type,
                      std::map<std::string, CppType> const& types,
                      std::vector<ModuleSchema> const& modules) -> EnumSchema const* {
    auto const spelling{resolve_type(type, types).spelling};
    for (auto const& candidate : modules) {
        auto const* enum_module{std::get_if<EnumModuleSchema>(&candidate)};
        if (enum_module == nullptr) {
            continue;
        }
        for (auto const& schema : enum_module->enums) {
            if (qualified_enum_name(*enum_module, schema) == spelling) {
                return &schema;
            }
        }
    }
    return nullptr;
}

} // namespace codegen::detail
