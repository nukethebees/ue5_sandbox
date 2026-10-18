#pragma once

#include <ioj/layout/document_id.hpp>
#include <ioj/layout/document_io_state.hpp>
#include <ioj/layout/revision.hpp>
#include <ioj/layout/variants.hpp>

#include <codegen/schema/manifest.h>
#include <codegen/schema/schema_version.h>
#include <lispb/project.h>
#include <lispb/schema/source_comments.h>
#include <lispb/schema/type_graph.h>

#include <memory>

namespace ioj::layout {
class EditorSession;
class DocumentState {
  public:
    DocumentState(EditorSession& session, DocumentId id);
    auto storage() const -> lispb::CompilerStorage&;
    auto manifest() const -> lispb::codegen::Manifest;
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
    DocumentIoState io;
    std::map<lispb::codegen::DeclarationHandle, lispb::schema::DeclarationId> declaration_ids;
    lispb::schema::DeclarationId next_declaration_id{1};
    std::vector<lispb::codegen::ModuleHandle> baseline;
    std::optional<lispb::Project> project;
    std::optional<lispb::TargetId> target;
    std::filesystem::path project_path;
    std::map<lispb::codegen::ModuleHandle, lispb::codegen::sexpr::SourceFileId> source_files;
    std::vector<lispb::codegen::sexpr::SourceFileId> source_order;
    std::vector<lispb::codegen::sexpr::SourceFileId> baseline_sources;

    // Replay forward root splices; immutable schema versions share the compiler's SoA stores.
    std::vector<std::size_t> starts;
    std::vector<std::size_t> removed;
    std::vector<std::size_t> offsets;
    std::vector<std::size_t> counts;
    std::vector<lispb::codegen::ModuleHandle> versions;
    std::vector<std::shared_ptr<Variants const>> variant_versions;
    std::vector<std::shared_ptr<std::vector<lispb::codegen::sexpr::SourceFileId> const>>
        source_versions;
    std::size_t position{};
    Revision revision{};
    bool valid{};
};
}
