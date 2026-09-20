#pragma once

#include <codegen/schema.h>

#include <optional>
#include <string_view>

namespace codegen::detail {

struct EnumIntegerType {
    int value_bits;
    bool is_unsigned;
};

auto packed_unsigned_width(std::string_view spelling) -> std::optional<int>;
auto enum_integer_type(std::string_view spelling) -> std::optional<EnumIntegerType>;
auto enum_required_packed_bits(EnumSchema const& schema) -> int;
auto find_packed_enum(TypeRef const& type,
                      std::map<std::string, CppType> const& types,
                      std::vector<ModuleSchema> const& modules) -> EnumSchema const*;

} // namespace codegen::detail
