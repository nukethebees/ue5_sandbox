#pragma once

#include <codegen/schema.h>

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace codegen::detail {

struct EnumIntegerType {
    int value_bits;
    bool is_unsigned;
};

struct EnumNumericValue {
    std::uint64_t magnitude;
    bool is_negative;
};

auto packed_unsigned_width(std::string_view spelling) -> std::optional<int>;
auto enum_integer_type(std::string_view spelling) -> std::optional<EnumIntegerType>;
auto resolve_enum_values(EnumSchema const& schema) -> std::vector<std::optional<EnumNumericValue>>;
auto enum_value_fits(EnumNumericValue value, EnumIntegerType type) -> bool;
auto enum_required_packed_bits(EnumSchema const& schema) -> std::optional<int>;
auto find_packed_enum(TypeRef const& type,
                      std::map<std::string, CppType> const& types,
                      std::vector<ModuleSchema> const& modules) -> EnumSchema const*;

} // namespace codegen::detail
