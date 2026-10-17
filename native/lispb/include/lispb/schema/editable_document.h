#pragma once

#include <codegen/schema/enum_schema.h>
#include <codegen/schema/fixed_point_schema.h>
#include <codegen/schema/integer_scalar_schema.h>
#include <codegen/schema/integer_varint_schema.h>
#include <codegen/schema/linear_quantized_schema.h>
#include <codegen/schema/manifest.h>
#include <codegen/schema/mini_float_schema.h>
#include <codegen/schema/optional_presence_bit_schema.h>
#include <codegen/schema/optional_sentinel_schema.h>
#include <codegen/schema/packed_value_schema.h>
#include <codegen/schema/record_schema.h>
#include <codegen/schema/soa_schema.h>
#include <codegen/schema/tagged_union_schema.h>
#include <codegen/schema/union_schema.h>
#include <lispb/schema/declaration_id.h>
#include <lispb/schema/declaration_info.h>
#include <lispb/schema/schema_edit_command.h>
#include <lispb/schema/schema_edit_error.h>
#include <lispb/schema/schema_source_file.h>
#include <lispb/schema/schema_source_update.h>
#include <lispb/schema/source_range.h>
#include <lispb/schema/type_graph.h>
#include <lispb/schema/type_identity.h>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <functional>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace lispb::schema {
class EditableSchemaDocument {
  public:
    static auto from_manifest(codegen::Manifest manifest) -> EditableSchemaDocument;

    auto manifest() const -> codegen::Manifest const&;
    auto types() const -> TypeGraph const&;
    auto source_files() const -> std::span<SchemaSourceFile const>;
    auto module_source_file(std::size_t module_index) const -> std::optional<std::size_t>;
    auto registered_type_source(std::string_view name) const -> std::optional<SourceRange>;
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
    // Multi-file publication rolls back failures but is not crash-atomic. External writers can
    // still change a source between content verification and publication or rollback.
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
    std::map<std::string, SourceRange, std::less<>> registered_type_sources_;
    std::filesystem::path types_path_;
    std::vector<std::filesystem::path> module_paths_;
    std::vector<std::optional<SourceRange>> module_source_ranges_;
    std::map<std::size_t, std::size_t> pending_module_sources_;
    std::vector<SourceRange> deleted_module_source_ranges_;
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

namespace detail {

using SaveReplaceFile = void (*)(std::filesystem::path const&, std::filesystem::path const&);
using SaveReplacementHook = void (*)(std::filesystem::path const&,
                                     std::filesystem::path const&,
                                     SaveReplaceFile);

auto set_save_replacement_hook_for_testing(SaveReplacementHook hook) noexcept
    -> SaveReplacementHook;

} // namespace detail

} // namespace lispb::schema
