#pragma once

#include <codegen/schema.h>

#include <optional>
#include <string_view>

namespace codegen::detail {

auto packed_unsigned_width(std::string_view spelling) -> std::optional<int>;
auto packed_signed_width(std::string_view spelling) -> std::optional<int>;

// Requires a validated layout. An incomplete explicit default set has no default value.
auto packed_default_value(PackedValueSchema const& schema,
                          TypeRegistry const& types,
                          std::vector<ModuleSchema> const& modules) -> std::optional<std::uint64_t>;
auto find_packed_enum(TypeRef const& type,
                      TypeRegistry const& types,
                      std::vector<ModuleSchema> const& modules) -> EnumSchema const*;
auto find_integer_domain(TypeRef const& type,
                         TypeRegistry const& types,
                         std::vector<ModuleSchema> const& modules,
                         std::string_view module_name = {}) -> std::optional<IntegerDomainView>;
auto find_linear_quantized(TypeRef const& type,
                           TypeRegistry const& types,
                           std::vector<ModuleSchema> const& modules)
    -> LinearQuantizedSchema const*;
auto find_fixed_point(TypeRef const& type,
                      TypeRegistry const& types,
                      std::vector<ModuleSchema> const& modules) -> FixedPointSchema const*;
auto find_mini_float(TypeRef const& type,
                     TypeRegistry const& types,
                     std::vector<ModuleSchema> const& modules) -> MiniFloatSchema const*;
auto derive_integer_scalar_width(IntegerDomainView const& scalar) -> std::optional<int>;
auto find_packed_integer_domain(PackedFieldSchema const& field,
                                TypeRegistry const& types,
                                std::vector<ModuleSchema> const& modules,
                                std::string_view module_name = {})
    -> std::optional<IntegerDomainView>;
auto derive_packed_field_width(PackedFieldSchema const& field,
                               TypeRegistry const& types,
                               std::vector<ModuleSchema> const& modules) -> std::optional<int>;

} // namespace codegen::detail
