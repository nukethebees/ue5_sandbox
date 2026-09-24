#pragma once

#include <codegen/ast/cpp_type.h>
#include <codegen/schema/packed_field_schema.h>

#include <map>
#include <span>
#include <string_view>
#include <variant>

namespace codegen {

struct ExternalIntegerSchema {
    bool signedness{};
    PackedIntegerValue minimum_value;
    PackedIntegerValue maximum_value;
    std::uint32_t bit_width{};
    std::vector<PackedNamedCodeSchema> named_codes;
    auto operator==(ExternalIntegerSchema const&) const -> bool = default;
};

enum class FloatingPointFormat { ieee754_binary16, ieee754_binary32, ieee754_binary64 };

struct FloatingPointFormatInfo {
    std::uint32_t exponent_bits;
    std::uint32_t fraction_bits;
    std::int32_t exponent_bias;
};

constexpr auto floating_point_format_info(FloatingPointFormat const format)
    -> FloatingPointFormatInfo {
    switch (format) {
        case FloatingPointFormat::ieee754_binary16:
            return {5, 10, 15};
        case FloatingPointFormat::ieee754_binary32:
            return {8, 23, 127};
        case FloatingPointFormat::ieee754_binary64:
            return {11, 52, 1023};
    }
    return {};
}

constexpr auto floating_point_format_name(FloatingPointFormat const format) -> std::string_view {
    switch (format) {
        case FloatingPointFormat::ieee754_binary16:
            return "ieee754-binary16";
        case FloatingPointFormat::ieee754_binary32:
            return "ieee754-binary32";
        case FloatingPointFormat::ieee754_binary64:
            return "ieee754-binary64";
    }
    return "unknown";
}

using ExternalScalarSchema =
    std::variant<std::monostate, ExternalIntegerSchema, FloatingPointFormat>;

struct RegisteredTypeSchema {
    CppType cpp_type;
    ExternalScalarSchema semantics;
};

using TypeRegistry = std::map<std::string, RegisteredTypeSchema>;

struct IntegerDomainView {
    bool signedness{};
    PackedIntegerValue minimum_value;
    PackedIntegerValue maximum_value;
    std::optional<std::uint32_t> bit_width;
    std::span<PackedNamedCodeSchema const> named_codes;
};

struct IntegerScalarSchema;
auto integer_domain(IntegerScalarSchema const& scalar) -> IntegerDomainView;
auto integer_domain(ExternalIntegerSchema const& scalar) -> IntegerDomainView;

auto external_scalar_schema(TypeRegistry const& types, std::string const& spelling)
    -> ExternalScalarSchema const*;

} // namespace codegen
