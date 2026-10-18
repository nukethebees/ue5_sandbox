#pragma once

#include <ioj/layout/document_source.hpp>
#include <ioj/layout/document_state.hpp>
#include <ioj/layout/revision.hpp>
#include <ioj/layout/transaction.hpp>
#include <ioj/layout/variants.hpp>
#include <ioj/layout/rendered_source.hpp>

#include <lispb/schema/document_query.h>
#include <lispb/schema/type_graph.h>

#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace ioj::layout {
class EditorSession;

// Borrow the creating session, which must outlive this handle. Moves preserve queries;
// queries and borrowed views expire when this document is edited or replayed.
class Document {
  public:
    ~Document();
    Document(Document&&) noexcept;
    auto operator=(Document&&) noexcept -> Document&;
    Document(Document const&) = delete;
    auto operator=(Document const&) -> Document& = delete;
    auto valid() const -> bool;
    auto id() const -> DocumentId;
    auto query() const -> lispb::schema::DocumentQuery;
    auto types() const -> lispb::schema::TypeGraph const&;
    auto variants() const -> Variants const&;
    auto analysis_settings() const -> AnalysisSettings const&;
    auto find_type(std::string_view module, std::string_view name) const
        -> std::optional<lispb::schema::TypeId>;
    auto find_declaration(lispb::schema::TypeIdentity const& identity) const
        -> std::optional<lispb::schema::DeclarationId>;
    auto declaration_type(lispb::schema::DeclarationId declaration) const
        -> std::optional<lispb::schema::TypeId>;
    auto session() const -> EditorSession&;
    auto transaction() const -> Transaction;
    auto commit(Transaction const& transaction) -> bool;
    auto undo() -> bool;
    auto redo() -> bool;
    auto can_undo() const -> bool;
    auto can_redo() const -> bool;
    auto revision() const -> Revision;
    auto project_path() const -> std::filesystem::path;
    auto target_name() const -> std::string_view;
    auto sources() const -> std::vector<RenderedSource>;
    auto declaration_source(std::string_view module, std::string_view name) const
        -> std::optional<std::string>;
    auto dirty() const -> bool;
    auto save() -> bool;
    auto save_as(std::filesystem::path const& project_path, std::string_view target) -> bool;
  private:
    friend class EditorSession;
    Document(EditorSession& session, DocumentId id);
    auto state() const -> DocumentState&;
    auto reconstruct(std::size_t position) -> lispb::codegen::Manifest;
    auto replay(std::size_t position) -> bool;
    EditorSession* session_{};
    DocumentId id_{};
};
}
