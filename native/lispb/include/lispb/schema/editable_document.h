#pragma once

#include <codegen/schema/declaration_handle.h>
#include <codegen/schema/declaration_kind.h>
#include <codegen/schema/manifest.h>
#include <codegen/schema/module_handle.h>
#include <codegen/sexpr/indices.h>
#include <codegen/sexpr/source_location.h>
#include <codegen/sexpr/source_span.h>
#include <lispb/schema/editable_document_state.h>
#include <lispb/schema/editor_declarations.h>
#include <lispb/schema/indices.h>
#include <lispb/schema/schema_edit_command.h>
#include <lispb/schema/schema_edit_error.h>
#include <lispb/schema/schema_source_update.h>
#include <lispb/schema/source_range.h>
#include <lispb/schema/type_graph.h>
#include <lispb/schema/type_identity.h>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace lispb {
struct CompilerStorage;
}

namespace lispb::schema {
class EditableSchemaDocument {
  public:
    ~EditableSchemaDocument();
    EditableSchemaDocument(EditableSchemaDocument&&) noexcept;
    auto operator=(EditableSchemaDocument&&) noexcept -> EditableSchemaDocument&;

    static auto from_manifest(std::unique_ptr<CompilerStorage> storage, codegen::Manifest manifest)
        -> EditableSchemaDocument;

    auto manifest() const -> codegen::Manifest const&;
    auto types() const -> TypeGraph const&;
    auto source_files() const -> std::span<codegen::sexpr::SourceFileId const>;
    auto module_source_file(std::size_t module_index) const
        -> std::optional<codegen::sexpr::SourceFileId>;
    auto registered_type_source(std::string_view name) const -> std::optional<SourceRange>;
    auto declarations() const -> EditorDeclarations const&;
    auto find_declaration(TypeIdentity const& identity) const -> std::optional<DeclarationId>;
    auto find_declaration(codegen::DeclarationHandle payload) const -> std::optional<DeclarationId>;
    auto storage() const -> CompilerStorage& { return *storage_; }
    auto source_span(SourceRange const& range) const -> codegen::sexpr::SourceSpan;
    auto source_location(SourceRange const& range) const -> codegen::sexpr::SourceLocation;
    auto declaration_handle(DeclarationId declaration,
                            std::optional<codegen::DeclarationKind> expected_kind = {}) const
        -> std::optional<codegen::DeclarationHandle>;
    auto unique_soa_generated_type_name(DeclarationId declaration, std::string const& base) const
        -> std::expected<std::string, SchemaEditError>;
    auto unique_soa_storage_owner_name(DeclarationId declaration, std::string const& base) const
        -> std::expected<std::string, SchemaEditError>;
    auto prepare_soa_duplicate(DeclarationId declaration) const
        -> std::expected<codegen::DeclarationHandle, SchemaEditError>;
    auto allocate_declaration_id() -> DeclarationId;

    auto create_declaration(DeclarationId declaration,
                            std::size_t module_index,
                            codegen::DeclarationHandle schema,
                            std::optional<std::size_t> insertion_index = {}) -> SchemaEditCommand;
    auto replace_declaration(DeclarationId declaration, codegen::DeclarationHandle schema)
        -> SchemaEditCommand;
    auto delete_declaration(DeclarationId declaration) -> SchemaEditCommand;
    auto create_module(codegen::sexpr::SourceFileId source_file, codegen::ModuleHandle schema)
        -> SchemaEditCommand;
    auto delete_module(std::size_t module_index) -> SchemaEditCommand;
    auto move_declaration(DeclarationId declaration,
                          std::size_t module_index,
                          std::optional<std::size_t> insertion_index = {},
                          bool restore_source_ownership = false) -> SchemaEditCommand;
    auto set_enumerator_display_name(DeclarationId declaration,
                                     std::string_view enumerator_name,
                                     std::optional<std::string> display_name) -> SchemaEditCommand;
    auto set_enumerator_name(DeclarationId declaration,
                             std::string_view current_name,
                             std::string_view new_name) -> SchemaEditCommand;
    auto rename_declaration(DeclarationId declaration, std::string_view new_name)
        -> SchemaEditCommand;

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

    explicit EditableSchemaDocument(std::unique_ptr<CompilerStorage> storage,
                                    EditableDocumentState state);

    auto execute(SchemaEditCommand command, bool record_inverse = true)
        -> std::expected<std::optional<SchemaEditCommand>, SchemaEditError>;
    auto command_declaration(SchemaEditCommand command) const -> std::optional<DeclarationId>;
    auto restore_module(std::size_t module_index,
                        codegen::ModuleHandle schema,
                        std::optional<SourceRange> source,
                        std::optional<codegen::sexpr::SourceFileId> pending_source_file,
                        std::span<DeclarationId const> declarations) -> SchemaEditCommand;

    std::unique_ptr<CompilerStorage> storage_;

    static auto read_state(CompilerStorage& storage,
                           std::filesystem::path const& types_path,
                           std::span<std::filesystem::path const> module_paths)
        -> EditableDocumentState;
    static void initialize_declarations(EditableDocumentState& state,
                                        std::vector<std::optional<SourceRange>> source_ranges);
    EditableDocumentState state_;
};

auto load_editable_schema_document(std::filesystem::path const& types_path,
                                   std::span<std::filesystem::path const> module_paths)
    -> EditableSchemaDocument;

} // namespace lispb::schema
