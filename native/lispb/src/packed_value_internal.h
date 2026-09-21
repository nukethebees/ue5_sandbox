#pragma once

#include <codegen/schema.h>

#include <optional>
#include <string_view>

namespace codegen::detail {

auto packed_unsigned_width(std::string_view spelling) -> std::optional<int>;
auto packed_signed_width(std::string_view spelling) -> std::optional<int>;
auto find_packed_enum(TypeRef const& type,
                      std::map<std::string, CppType> const& types,
                      std::vector<ModuleSchema> const& modules) -> EnumSchema const*;
auto find_integer_scalar(TypeRef const& type,
                         std::map<std::string, CppType> const& types,
                         std::vector<ModuleSchema> const& modules) -> IntegerScalarSchema const*;
auto find_linear_quantized(TypeRef const& type,
                           std::map<std::string, CppType> const& types,
                           std::vector<ModuleSchema> const& modules)
    -> LinearQuantizedSchema const*;
auto find_fixed_point(TypeRef const& type,
                      std::map<std::string, CppType> const& types,
                      std::vector<ModuleSchema> const& modules) -> FixedPointSchema const*;
auto find_mini_float(TypeRef const& type,
                     std::map<std::string, CppType> const& types,
                     std::vector<ModuleSchema> const& modules) -> MiniFloatSchema const*;
auto derive_integer_scalar_width(IntegerScalarSchema const& scalar) -> std::optional<int>;
auto derive_packed_field_width(PackedFieldSchema const& field,
                               std::map<std::string, CppType> const& types,
                               std::vector<ModuleSchema> const& modules) -> std::optional<int>;

} // namespace codegen::detail
