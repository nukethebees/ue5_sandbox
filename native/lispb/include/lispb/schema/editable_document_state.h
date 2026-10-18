#pragma once

#include <codegen/schema/manifest.h>
#include <codegen/sexpr/indices.h>
#include <lispb/indices.h>
#include <lispb/schema/create_declaration_edits.h>
#include <lispb/schema/create_module_edits.h>
#include <lispb/schema/editor_declarations.h>
#include <lispb/schema/enumerator_display_name_edits.h>
#include <lispb/schema/enumerator_name_edits.h>
#include <lispb/schema/indices.h>
#include <lispb/schema/move_declaration_edits.h>
#include <lispb/schema/rename_declaration_edits.h>
#include <lispb/schema/replace_declaration_edits.h>
#include <lispb/schema/restore_module_edits.h>
#include <lispb/schema/schema_edit_history_entry.h>
#include <lispb/schema/source_range.h>
#include <lispb/schema/type_graph.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <vector>

namespace lispb::schema {

struct EditableDocumentState {
    explicit EditableDocumentState(codegen::Manifest manifest);
    CreateDeclarationEdits create_declaration_edits_;
    ReplaceDeclarationEdits replace_declaration_edits_;
    std::vector<DeclarationId> delete_declaration_edits_;
    CreateModuleEdits create_module_edits_;
    std::vector<std::size_t> delete_module_edits_;
    RestoreModuleEdits restore_module_edits_;
    MoveDeclarationEdits move_declaration_edits_;
    EnumeratorDisplayNameEdits enumerator_display_name_edits_;
    EnumeratorNameEdits enumerator_name_edits_;
    RenameDeclarationEdits rename_declaration_edits_;
    codegen::Manifest manifest_;
    TypeGraph types_;
    std::vector<codegen::sexpr::SourceFileId> source_files_;
    std::map<IdentifierId, SourceRange> registered_type_sources_;
    std::filesystem::path types_path_;
    std::vector<std::filesystem::path> module_paths_;
    std::vector<std::optional<SourceRange>> module_source_ranges_;
    std::map<std::size_t, codegen::sexpr::SourceFileId> pending_module_sources_;
    std::vector<SourceRange> deleted_module_source_ranges_;
    EditorDeclarations declarations_;
    std::map<DeclarationId, SourceRange> source_tombstones_;
    std::vector<SchemaEditHistoryEntry> history_;
    std::size_t history_position_{};
    std::optional<std::size_t> saved_history_position_{0};
    std::uint64_t revision_{};
};

} // namespace lispb::schema
