#pragma once

#include <codegen/schema.h>

#include <lispb/schema/type_graph.h>

#include <compare>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace lispb::schema {

struct DeclarationId {
    inline static constexpr std::uint64_t invalid_value{0};

    std::uint64_t value{invalid_value};

    [[nodiscard]] auto valid() const -> bool { return value != invalid_value; }
    auto operator<=>(DeclarationId const&) const = default;
};

struct SourceRange {
    std::size_t source_file_index{};
    std::size_t begin_offset{};
    std::size_t end_offset{};
    std::size_t line{1};
    std::size_t column{1};

    auto operator==(SourceRange const&) const -> bool = default;
};

struct SchemaSourceFile {
    std::filesystem::path path;
    std::string text;
};

struct DeclarationInfo {
    DeclarationId id;
    TypeIdentity identity;
    std::size_t module_index{};
    std::size_t declaration_index{};
    std::optional<SourceRange> source;
};

struct SetEnumeratorDisplayName {
    DeclarationId enum_declaration;
    std::string enumerator_name;
    std::optional<std::string> display_name;
};

struct SetEnumeratorName {
    DeclarationId enum_declaration;
    std::string current_name;
    std::string new_name;
};

struct CreateModule {
    std::size_t source_file_index{};
    codegen::ModuleSchema schema;
};

struct DeleteModule {
    std::size_t module_index{};
};

struct MoveDeclaration {
    DeclarationId declaration;
    std::size_t module_index{};
    std::optional<std::size_t> insertion_index;
    bool restore_source_ownership{};
};

struct CreateEnum {
    DeclarationId declaration;
    std::size_t module_index{};
    codegen::EnumSchema schema;
    std::optional<std::size_t> insertion_index;
};

struct ReplaceEnum {
    DeclarationId declaration;
    codegen::EnumSchema schema;
};

struct DeleteEnum {
    DeclarationId declaration;
};

struct CreatePackedValue {
    DeclarationId declaration;
    std::size_t module_index{};
    codegen::PackedValueSchema schema;
    std::optional<std::size_t> insertion_index;
};

struct ReplacePackedValue {
    DeclarationId declaration;
    codegen::PackedValueSchema schema;
};

struct DeletePackedValue {
    DeclarationId declaration;
};

struct CreateIntegerScalar {
    DeclarationId declaration;
    std::size_t module_index{};
    codegen::IntegerScalarSchema schema;
    std::optional<std::size_t> insertion_index;
};

struct ReplaceIntegerScalar {
    DeclarationId declaration;
    codegen::IntegerScalarSchema schema;
};

struct RenameDeclaration {
    DeclarationId declaration;
    std::string new_name;
};

struct DeleteIntegerScalar {
    DeclarationId declaration;
};

struct CreateLinearQuantized {
    DeclarationId declaration;
    std::size_t module_index{};
    codegen::LinearQuantizedSchema schema;
    std::optional<std::size_t> insertion_index;
};

struct ReplaceLinearQuantized {
    DeclarationId declaration;
    codegen::LinearQuantizedSchema schema;
};

struct DeleteLinearQuantized {
    DeclarationId declaration;
};

struct CreateIntegerVarint {
    DeclarationId declaration;
    std::size_t module_index{};
    codegen::IntegerVarintSchema schema;
    std::optional<std::size_t> insertion_index;
};

struct ReplaceIntegerVarint {
    DeclarationId declaration;
    codegen::IntegerVarintSchema schema;
};

struct DeleteIntegerVarint {
    DeclarationId declaration;
};

struct CreateFixedPoint {
    DeclarationId declaration;
    std::size_t module_index{};
    codegen::FixedPointSchema schema;
    std::optional<std::size_t> insertion_index;
};

struct ReplaceFixedPoint {
    DeclarationId declaration;
    codegen::FixedPointSchema schema;
};

struct DeleteFixedPoint {
    DeclarationId declaration;
};

struct CreateOptionalSentinel {
    DeclarationId declaration;
    std::size_t module_index{};
    codegen::OptionalSentinelSchema schema;
    std::optional<std::size_t> insertion_index;
};

struct ReplaceOptionalSentinel {
    DeclarationId declaration;
    codegen::OptionalSentinelSchema schema;
};

struct DeleteOptionalSentinel {
    DeclarationId declaration;
};

struct CreateOptionalPresenceBit {
    DeclarationId declaration;
    std::size_t module_index{};
    codegen::OptionalPresenceBitSchema schema;
    std::optional<std::size_t> insertion_index;
};

struct ReplaceOptionalPresenceBit {
    DeclarationId declaration;
    codegen::OptionalPresenceBitSchema schema;
};

struct DeleteOptionalPresenceBit {
    DeclarationId declaration;
};

struct CreateMiniFloat {
    DeclarationId declaration;
    std::size_t module_index{};
    codegen::MiniFloatSchema schema;
    std::optional<std::size_t> insertion_index;
};

struct ReplaceMiniFloat {
    DeclarationId declaration;
    codegen::MiniFloatSchema schema;
};

struct DeleteMiniFloat {
    DeclarationId declaration;
};

struct CreateRecord {
    DeclarationId declaration;
    std::size_t module_index{};
    codegen::RecordSchema schema;
    std::optional<std::size_t> insertion_index;
};

struct ReplaceRecord {
    DeclarationId declaration;
    codegen::RecordSchema schema;
};

struct DeleteRecord {
    DeclarationId declaration;
};

struct CreateUnion {
    DeclarationId declaration;
    std::size_t module_index{};
    codegen::UnionSchema schema;
    std::optional<std::size_t> insertion_index;
};

struct ReplaceUnion {
    DeclarationId declaration;
    codegen::UnionSchema schema;
};

struct DeleteUnion {
    DeclarationId declaration;
};

struct CreateTaggedUnion {
    DeclarationId declaration;
    std::size_t module_index{};
    codegen::TaggedUnionSchema schema;
    std::optional<std::size_t> insertion_index;
};

struct ReplaceTaggedUnion {
    DeclarationId declaration;
    codegen::TaggedUnionSchema schema;
};

struct DeleteTaggedUnion {
    DeclarationId declaration;
};

struct CreateSoa {
    DeclarationId declaration;
    std::size_t module_index{};
    codegen::SoaSchema schema;
    std::optional<std::size_t> insertion_index;
};

struct ReplaceSoa {
    DeclarationId declaration;
    codegen::SoaSchema schema;
};

struct DeleteSoa {
    DeclarationId declaration;
};

using SchemaEditCommand = std::variant<SetEnumeratorDisplayName,
                                       SetEnumeratorName,
                                       CreateModule,
                                       DeleteModule,
                                       MoveDeclaration,
                                       CreateEnum,
                                       ReplaceEnum,
                                       DeleteEnum,
                                       CreatePackedValue,
                                       ReplacePackedValue,
                                       DeletePackedValue,
                                       CreateIntegerScalar,
                                       ReplaceIntegerScalar,
                                       RenameDeclaration,
                                       DeleteIntegerScalar,
                                       CreateLinearQuantized,
                                       ReplaceLinearQuantized,
                                       DeleteLinearQuantized,
                                       CreateIntegerVarint,
                                       ReplaceIntegerVarint,
                                       DeleteIntegerVarint,
                                       CreateFixedPoint,
                                       ReplaceFixedPoint,
                                       DeleteFixedPoint,
                                       CreateOptionalSentinel,
                                       ReplaceOptionalSentinel,
                                       DeleteOptionalSentinel,
                                       CreateOptionalPresenceBit,
                                       ReplaceOptionalPresenceBit,
                                       DeleteOptionalPresenceBit,
                                       CreateMiniFloat,
                                       ReplaceMiniFloat,
                                       DeleteMiniFloat,
                                       CreateRecord,
                                       ReplaceRecord,
                                       DeleteRecord,
                                       CreateUnion,
                                       ReplaceUnion,
                                       DeleteUnion,
                                       CreateTaggedUnion,
                                       ReplaceTaggedUnion,
                                       DeleteTaggedUnion,
                                       CreateSoa,
                                       ReplaceSoa,
                                       DeleteSoa>;

struct SchemaEditError {
    std::string message;
};

struct SchemaSourceUpdate {
    std::filesystem::path path;
    std::string original;
    std::string updated;
};

class EditableSchemaDocument {
  public:
    static auto from_manifest(codegen::Manifest manifest) -> EditableSchemaDocument;

    auto manifest() const -> codegen::Manifest const&;
    auto types() const -> TypeGraph const&;
    auto source_files() const -> std::span<SchemaSourceFile const>;
    auto declarations() const -> std::span<DeclarationInfo const>;
    auto declaration(DeclarationId id) const -> DeclarationInfo const*;
    auto find_declaration(TypeIdentity const& identity) const -> std::optional<DeclarationId>;
    auto enum_schema(DeclarationId declaration) const -> codegen::EnumSchema const*;
    auto packed_value_schema(DeclarationId declaration) const -> codegen::PackedValueSchema const*;
    auto integer_scalar_schema(DeclarationId declaration) const
        -> codegen::IntegerScalarSchema const*;
    auto linear_quantized_schema(DeclarationId declaration) const
        -> codegen::LinearQuantizedSchema const*;
    auto integer_varint_schema(DeclarationId declaration) const
        -> codegen::IntegerVarintSchema const*;
    auto fixed_point_schema(DeclarationId declaration) const -> codegen::FixedPointSchema const*;
    auto optional_sentinel_schema(DeclarationId declaration) const
        -> codegen::OptionalSentinelSchema const*;
    auto optional_presence_bit_schema(DeclarationId declaration) const
        -> codegen::OptionalPresenceBitSchema const*;
    auto mini_float_schema(DeclarationId declaration) const -> codegen::MiniFloatSchema const*;
    auto record_schema(DeclarationId declaration) const -> codegen::RecordSchema const*;
    auto union_schema(DeclarationId declaration) const -> codegen::UnionSchema const*;
    auto tagged_union_schema(DeclarationId declaration) const -> codegen::TaggedUnionSchema const*;
    auto soa_schema(DeclarationId declaration) const -> codegen::SoaSchema const*;
    auto unique_soa_generated_type_name(DeclarationId declaration, std::string const& base) const
        -> std::expected<std::string, SchemaEditError>;
    auto unique_soa_storage_owner_name(DeclarationId declaration, std::string const& base) const
        -> std::expected<std::string, SchemaEditError>;
    auto prepare_soa_duplicate(DeclarationId declaration) const
        -> std::expected<codegen::SoaSchema, SchemaEditError>;
    auto allocate_declaration_id() -> DeclarationId;

    auto apply(SchemaEditCommand command) -> std::expected<bool, SchemaEditError>;
    auto undo() -> std::expected<bool, SchemaEditError>;
    auto redo() -> std::expected<bool, SchemaEditError>;
    auto can_undo() const -> bool;
    auto can_redo() const -> bool;
    auto dirty() const -> bool;
    auto revision() const -> std::uint64_t;
    void mark_saved();
    auto preview_source_updates() const
        -> std::expected<std::vector<SchemaSourceUpdate>, SchemaEditError>;
    auto save() -> std::expected<std::vector<std::filesystem::path>, SchemaEditError>;
  private:
    friend auto load_editable_schema_document(std::filesystem::path const& types_path,
                                              std::span<std::filesystem::path const> module_paths)
        -> EditableSchemaDocument;

    struct HistoryEntry {
        SchemaEditCommand forward;
        SchemaEditCommand inverse;
    };

    explicit EditableSchemaDocument(codegen::Manifest manifest,
                                    std::vector<SchemaSourceFile> source_files,
                                    std::filesystem::path types_path = {},
                                    std::vector<std::filesystem::path> module_paths = {});

    void initialize_declarations(std::vector<std::optional<SourceRange>> source_ranges);
    auto execute(SchemaEditCommand const& command)
        -> std::expected<std::optional<SchemaEditCommand>, SchemaEditError>;

    codegen::Manifest manifest_;
    TypeGraph types_;
    std::vector<SchemaSourceFile> source_files_;
    std::filesystem::path types_path_;
    std::vector<std::filesystem::path> module_paths_;
    std::vector<std::optional<SourceRange>> module_source_ranges_;
    std::map<std::size_t, std::size_t> pending_module_sources_;
    std::vector<DeclarationInfo> declarations_;
    std::map<DeclarationId, SourceRange> source_tombstones_;
    std::vector<HistoryEntry> history_;
    std::size_t history_position_{};
    std::optional<std::size_t> saved_history_position_{0};
    std::uint64_t next_declaration_id_{1};
    std::uint64_t revision_{};
};

auto load_editable_schema_document(std::filesystem::path const& types_path,
                                   std::span<std::filesystem::path const> module_paths)
    -> EditableSchemaDocument;

} // namespace lispb::schema
