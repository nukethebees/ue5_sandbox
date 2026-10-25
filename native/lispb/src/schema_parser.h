#pragma once

#include <codegen/schema/indices.h>
#include <codegen/sexpr/parser.h>
#include <lispb/indices.h>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace lispb::codegen::sexpr {
class Fields;
}

namespace lispb::codegen {

class Declarations;
class RegisteredTypes;
struct TypeRegistry;
struct DeclarationHandle;
struct ModuleHandle;
struct PackedSegmentId;
struct PackedIntegerValue;
struct ExternalScalarId;
enum class Access : std::uint8_t;
enum class RecordKind : std::uint8_t;
enum class SchemaProperty : std::uint8_t;
enum class NativeEnumHelper : std::uint8_t;
enum class PackedBitOrder : std::uint8_t;
enum class PackedByteOrder : std::uint8_t;
enum class SemanticRelationUnit : std::uint8_t;
enum class SettingDevice : std::uint8_t;

class SchemaParser {
  public:
    explicit SchemaParser(sexpr::Syntax& syntax);
    auto parse_module(Declarations& declarations, sexpr::FormId const form) -> ModuleHandle;
    auto parse_type_definition(sexpr::FormId const form, TypeRegistry& registry)
        -> std::pair<lispb::StringId, RegisteredTypeId>;
  private:
    auto optional_text_id(sexpr::Fields const& fields, SchemaProperty const key)
        -> std::optional<lispb::StringId>;
    auto parse_export_specifier(sexpr::Fields const& fields) -> lispb::StringId;
    auto optional_identifier(sexpr::Fields const& fields, SchemaProperty const key)
        -> std::optional<lispb::StringId>;
    auto text_id_list(sexpr::FormId const form, std::string_view const purpose)
        -> std::vector<lispb::StringId>;
    auto identifier_list(sexpr::FormId const form, std::string_view const purpose)
        -> std::vector<lispb::StringId>;
    auto text_id_list_or(sexpr::Fields const& fields, SchemaProperty const key)
        -> std::vector<lispb::StringId>;
    auto identifier_list_or(sexpr::Fields const& fields, SchemaProperty const key)
        -> std::vector<lispb::StringId>;
    auto unsigned_integer(sexpr::FormId const form, std::string_view const purpose)
        -> std::uint64_t;
    auto packed_integer(sexpr::FormId const form, std::string_view const purpose)
        -> PackedIntegerValue;
    auto optional_text(sexpr::Fields const& fields, SchemaProperty const key)
        -> std::optional<std::string>;
    auto cpp_lines_or(sexpr::Fields const& fields, SchemaProperty const key)
        -> std::vector<lispb::StringId>;
    auto boolean_or(sexpr::Fields const& fields,
                    SchemaProperty const key,
                    bool const fallback = false) -> bool;
    auto parse_type_ref(Declarations& declarations, sexpr::FormId const form) -> TypeRefId;
    auto type_ref_list(Declarations& declarations,
                       sexpr::FormId const form,
                       std::string_view const purpose) -> std::vector<TypeRefId>;
    auto parse_parameter(Declarations& declarations, sexpr::FormId const form) -> ParameterId;
    auto parse_function(Declarations& declarations,
                        sexpr::FormId const form,
                        bool soa_owner = false) -> FunctionSchemaId;
    auto parse_member(Declarations& declarations, sexpr::FormId const form) -> SoaMemberId;
    auto parse_soa(Declarations& declarations, sexpr::FormId const form) -> DeclarationHandle;
    auto parse_module_settings(Declarations& declarations, sexpr::Fields const& fields)
        -> ModuleSettingsId;
    auto parse_native_enum_helper(sexpr::FormId const form) -> NativeEnumHelper;
    auto parse_enum_unreal_projection(Declarations& declarations, sexpr::FormId const form)
        -> EnumUnrealProjectionId;
    auto parse_semantic_relation_unit(sexpr::FormId const form) -> SemanticRelationUnit;
    auto parse_packed_byte_order(sexpr::FormId const form) -> PackedByteOrder;
    auto parse_packed_bit_order(sexpr::FormId const form) -> PackedBitOrder;
    auto parse_packed_field(Declarations& declarations, sexpr::FormId const form)
        -> PackedSegmentId;
    auto parse_packed_reserved_bits(Declarations& declarations, sexpr::FormId const form)
        -> PackedSegmentId;
    auto parse_packed_value(Declarations& declarations, sexpr::FormId const form)
        -> DeclarationHandle;
    auto parse_enum(Declarations& declarations, sexpr::FormId const form) -> DeclarationHandle;
    auto parse_table(Declarations& declarations, sexpr::FormId const form) -> DeclarationHandle;
    auto parse_facade_target_storage(sexpr::FormId const form) -> bool;
    auto parse_facade_access(sexpr::Fields const& fields, SchemaProperty const key) -> Access;
    auto parse_facade_friend_kind(sexpr::Fields const& fields) -> RecordKind;
    auto parse_facade(Declarations& declarations, sexpr::FormId const form) -> DeclarationHandle;
    auto parse_setting_device(sexpr::FormId const form) -> SettingDevice;
    auto parse_control(Declarations& declarations, sexpr::FormId const form) -> SettingControlId;
    auto parse_setting(Declarations& declarations, sexpr::FormId const form) -> SettingId;
    auto parse_record_member(Declarations& declarations, sexpr::FormId const form)
        -> RecordMemberId;
    auto parse_record(Declarations& declarations, sexpr::FormId const form) -> DeclarationHandle;
    auto parse_union_alternative(Declarations& declarations, sexpr::FormId const form)
        -> UnionAlternativeId;
    auto parse_union(Declarations& declarations, sexpr::FormId const form) -> DeclarationHandle;
    auto parse_tagged_union_alternative(Declarations& declarations, sexpr::FormId const form)
        -> TaggedUnionAlternativeId;
    auto parse_tagged_union(Declarations& declarations, sexpr::FormId const form)
        -> DeclarationHandle;
    auto parse_integer_scalar(Declarations& declarations, sexpr::FormId const form)
        -> DeclarationHandle;
    auto parse_linear_quantized(Declarations& declarations, sexpr::FormId const form)
        -> DeclarationHandle;
    auto parse_integer_varint(Declarations& declarations, sexpr::FormId const form)
        -> DeclarationHandle;
    auto parse_fixed_point(Declarations& declarations, sexpr::FormId const form)
        -> DeclarationHandle;
    auto parse_mini_float(Declarations& declarations, sexpr::FormId const form)
        -> DeclarationHandle;
    auto parse_optional_sentinel(Declarations& declarations, sexpr::FormId const form)
        -> DeclarationHandle;
    auto parse_optional_presence_bit(Declarations& declarations, sexpr::FormId const form)
        -> DeclarationHandle;
    auto parse_normal_declaration(Declarations& declarations, sexpr::FormId const form)
        -> DeclarationHandle;
    auto parse_external_integer(RegisteredTypes& registered, sexpr::FormId const form)
        -> ExternalScalarId;
    sexpr::Syntax& syntax_;
    sexpr::Parser parser_;
};

}
