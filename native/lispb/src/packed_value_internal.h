#pragma once

#include <codegen/schema.h>

#include <optional>
#include <string_view>

namespace codegen::detail {

auto packed_unsigned_width(std::string_view spelling) -> std::optional<int>;
auto find_packed_enum(TypeRef const& type,
                      std::map<std::string, CppType> const& types,
                      std::vector<ModuleSchema> const& modules) -> EnumSchema const*;

} // namespace codegen::detail
