#pragma once

#include <ioj/layout/analysis_settings.hpp>
#include <ioj/layout/document_id.hpp>
#include <ioj/layout/document_io_state.hpp>
#include <ioj/layout/frozen_sources.hpp>
#include <ioj/layout/revision.hpp>
#include <ioj/layout/source_id.hpp>
#include <ioj/layout/transaction.hpp>
#include <ioj/layout/variants.hpp>

#include <codegen/schema/manifest.h>
#include <codegen/schema/schema_version.h>
#include <lispb/project.h>
#include <lispb/schema/source_comments.h>
#include <lispb/schema/type_graph.h>

#include <map>

namespace ioj::layout {
class EditorSession;
class DocumentState {
  public:
    DocumentState(EditorSession& owner, DocumentId identity);
    auto storage() const -> lispb::CompilerStorage&;
    auto manifest() const -> lispb::codegen::Manifest;
    void reset_workspace();
    void restore_baseline();
    auto register_declaration(lispb::codegen::DeclarationHandle declaration)
        -> lispb::schema::DeclarationId;
    void inherit_declaration(lispb::codegen::DeclarationHandle before,
                             lispb::codegen::DeclarationHandle after);
    void inherit_module(lispb::codegen::ModuleHandle before, lispb::codegen::ModuleHandle after);
    void edit_source(std::string_view operation,
                     std::string_view path,
                     std::string_view text,
                     lispb::codegen::Manifest& candidate);

    EditorSession* session;
    DocumentId id;
    std::vector<lispb::codegen::ModuleHandle> modules;
    std::map<lispb::IdentifierId, lispb::codegen::RegisteredTypeId> registered_types;
    lispb::schema::TypeGraph types;
    lispb::schema::SourceComments comments;
    Variants variants;
    AnalysisSettings analysis_settings;
    DocumentIoState io;
    std::map<lispb::codegen::DeclarationHandle, lispb::schema::DeclarationId> declaration_ids;
    lispb::schema::DeclarationId next_declaration_id{1};
    std::map<std::pair<InstructionId, std::uint32_t>, lispb::schema::DeclarationId> creation_ids;
    InstructionId next_instruction_id{1};
    InstructionId current_instruction_id{};
    std::uint32_t creation_slot{};
    std::uint32_t source_creation_slot{};
    VariantId next_variant_id{1};
    std::map<lispb::codegen::ModuleHandle, lispb::codegen::sexpr::SourceFileId> source_files;
    std::vector<lispb::codegen::sexpr::SourceFileId> source_order;
    std::map<lispb::codegen::sexpr::SourceFileId, SourceId> source_ids;
    std::map<lispb::codegen::sexpr::SourceFileId, lispb::codegen::sexpr::SourceFileId>
        render_origins;
    FrozenSources baseline_sources;
    Transaction history;
    std::vector<std::size_t> transaction_ends;
    std::size_t position{};
    Revision revision{};
    bool valid{};
    bool changed{};
};
}
