#include "packed_value_internal.h"

#include <algorithm>
#include <limits>

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

auto enum_required_packed_bits(EnumSchema const& schema) -> int {
    std::uint64_t next_value{};
    std::uint64_t maximum_value{};
    for (auto const& value : schema.values) {
        auto const encoded_value{value.initializer.has_value() ? *value.initializer : next_value};
        if (!schema.count.has_value() || value.name != *schema.count) {
            maximum_value = std::max(maximum_value, encoded_value);
        }
        if (encoded_value != std::numeric_limits<std::uint64_t>::max()) {
            next_value = encoded_value + 1;
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
