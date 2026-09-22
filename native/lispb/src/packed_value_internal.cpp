#include "packed_value_internal.h"

#include <lispb/schema/enum_domain.h>

#include <algorithm>

namespace codegen::detail {
namespace {

template <typename Schema>
auto find_normal(std::string const& spelling,
                 std::vector<ModuleSchema> const& modules,
                 std::string_view const module_name = {}) -> Schema const* {
    if (!module_name.empty()) {
        for (auto const& candidate : modules) {
            auto const* module{std::get_if<NormalModuleSchema>(&candidate)};
            if (module == nullptr || module->settings.name != module_name) {
                continue;
            }
            for (auto const& declaration : module->declarations) {
                auto const* schema{std::get_if<Schema>(&declaration)};
                if (schema != nullptr && schema->name == spelling) {
                    return schema;
                }
            }
        }
    }
    for (auto const& candidate : modules) {
        auto const* module{std::get_if<NormalModuleSchema>(&candidate)};
        if (module == nullptr) {
            continue;
        }
        for (auto const& declaration : module->declarations) {
            auto const* schema{std::get_if<Schema>(&declaration)};
            if (schema == nullptr) {
                continue;
            }
            auto const qualified{module->settings.namespace_name.has_value()
                                     ? *module->settings.namespace_name + "::" + schema->name
                                     : schema->name};
            if (qualified == spelling) {
                return schema;
            }
        }
    }
    return nullptr;
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

auto packed_signed_width(std::string_view const spelling) -> std::optional<int> {
    static std::map<std::string_view, int> const widths{
        {"int8", 8},
        {"int16", 16},
        {"int32", 32},
        {"int64", 64},
        {"std::int8_t", 8},
        {"std::int16_t", 16},
        {"std::int32_t", 32},
        {"std::int64_t", 64},
    };
    auto const found{widths.find(spelling)};
    return found == widths.end() ? std::nullopt : std::optional<int>{found->second};
}

auto find_packed_enum(TypeRef const& type,
                      std::map<std::string, CppType> const& types,
                      std::vector<ModuleSchema> const& modules) -> EnumSchema const* {
    return find_normal<EnumSchema>(resolve_type(type, types).spelling, modules);
}

auto find_integer_scalar(TypeRef const& type,
                         std::map<std::string, CppType> const& types,
                         std::vector<ModuleSchema> const& modules,
                         std::string_view const module_name) -> IntegerScalarSchema const* {
    return find_normal<IntegerScalarSchema>(
        resolve_type(type, types).spelling, modules, module_name);
}

auto find_linear_quantized(TypeRef const& type,
                           std::map<std::string, CppType> const& types,
                           std::vector<ModuleSchema> const& modules)
    -> LinearQuantizedSchema const* {
    return find_normal<LinearQuantizedSchema>(resolve_type(type, types).spelling, modules);
}

auto find_fixed_point(TypeRef const& type,
                      std::map<std::string, CppType> const& types,
                      std::vector<ModuleSchema> const& modules) -> FixedPointSchema const* {
    return find_normal<FixedPointSchema>(resolve_type(type, types).spelling, modules);
}

auto find_mini_float(TypeRef const& type,
                     std::map<std::string, CppType> const& types,
                     std::vector<ModuleSchema> const& modules) -> MiniFloatSchema const* {
    return find_normal<MiniFloatSchema>(resolve_type(type, types).spelling, modules);
}

auto derive_integer_scalar_width(IntegerScalarSchema const& scalar) -> std::optional<int> {
    if (scalar.bit_width.has_value()) {
        return static_cast<int>(*scalar.bit_width);
    }

    auto minimum_code{scalar.minimum_value};
    auto maximum_code{scalar.maximum_value};
    for (auto const& code : scalar.named_codes) {
        if (packed_integer_less(code.value, minimum_code)) {
            minimum_code = code.value;
        }
        if (packed_integer_less(maximum_code, code.value)) {
            maximum_code = code.value;
        }
    }
    auto const bits{minimum_packed_integer_bits(minimum_code, maximum_code, scalar.signedness)};
    return bits.has_value() ? std::optional<int>{static_cast<int>(*bits)} : std::nullopt;
}

auto derive_packed_field_width(PackedFieldSchema const& field,
                               std::map<std::string, CppType> const& types,
                               std::vector<ModuleSchema> const& modules) -> std::optional<int> {
    if (field.bits.has_value()) {
        return field.bits;
    }
    if (auto const* scalar{find_integer_scalar(field.type, types, modules)}) {
        return derive_integer_scalar_width(*scalar);
    }
    if (field.kind == PackedFieldKind::linear_quantized) {
        auto const* quantized{find_linear_quantized(field.type, types, modules)};
        return quantized != nullptr ? std::optional<int>{static_cast<int>(quantized->bit_width)}
                                    : std::nullopt;
    }
    if (field.kind == PackedFieldKind::fixed_point) {
        auto const* fixed{find_fixed_point(field.type, types, modules)};
        return fixed != nullptr ? std::optional<int>{static_cast<int>(fixed->total_bits)}
                                : std::nullopt;
    }
    if (field.kind == PackedFieldKind::mini_float) {
        auto const* mini{find_mini_float(field.type, types, modules)};
        return mini != nullptr
                 ? std::optional<int>{static_cast<int>(mini->sign_bits + mini->exponent_bits +
                                                       mini->significand_bits)}
                 : std::nullopt;
    }
    if (field.kind == PackedFieldKind::enumeration) {
        auto const* enumeration{find_packed_enum(field.type, types, modules)};
        if (enumeration == nullptr) {
            return std::nullopt;
        }
        if (enumeration->bit_width.has_value()) {
            return static_cast<int>(*enumeration->bit_width);
        }
        std::vector<lispb::schema::EnumDomainInput> values;
        values.reserve(enumeration->values.size());
        for (auto const& value : enumeration->values) {
            values.push_back({.name = value.name,
                              .explicit_value = value.initializer,
                              .reserved = value.sentinel || (enumeration->count.has_value() &&
                                                             value.name == *enumeration->count)});
        }
        auto const domain{lispb::schema::analyze_enum_domain(values, enumeration->signedness)};
        return domain.minimum_required_bits.has_value()
                 ? std::optional<int>{static_cast<int>(*domain.minimum_required_bits)}
                 : std::nullopt;
    }
    if (!field.minimum_value.has_value() || !field.maximum_value.has_value()) {
        return std::nullopt;
    }

    auto minimum_code{*field.minimum_value};
    auto maximum_code{*field.maximum_value};
    for (auto const& code : field.named_codes) {
        if (packed_integer_less(code.value, minimum_code)) {
            minimum_code = code.value;
        }
        if (packed_integer_less(maximum_code, code.value)) {
            maximum_code = code.value;
        }
    }
    auto const bits{minimum_packed_integer_bits(
        minimum_code, maximum_code, field.kind == PackedFieldKind::signed_integer)};
    return bits.has_value() ? std::optional<int>{static_cast<int>(*bits)} : std::nullopt;
}

} // namespace codegen::detail
