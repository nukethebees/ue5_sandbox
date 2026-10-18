#pragma once

#include <ioj/layout/document_source.hpp>
#include <ioj/layout/revision.hpp>
#include <ioj/layout/transaction.hpp>
#include <ioj/layout/variants.hpp>

#include <lispb/schema/document_query.h>
#include <lispb/schema/type_graph.h>

#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace lispb {
class Diagnostics;
}
namespace ioj::layout {
class DocumentState;

// Queries and source views borrow this document until the next edit or source rendering.
class Document {
  public:
    static auto from_source(std::string_view source, std::string_view registry = {}) -> Document;
    static auto from_sources(std::span<DocumentSource const> sources,
                             std::string_view registry = {}) -> Document;
    static auto load(std::filesystem::path const& project_path, std::string_view target)
        -> Document;
    ~Document();
    Document(Document&&) noexcept;
    auto operator=(Document&&) noexcept -> Document&;
    Document(Document const&) = delete;
    auto operator=(Document const&) -> Document& = delete;
    auto valid() const -> bool;
    auto query() const -> lispb::schema::DocumentQuery;
    auto types() const -> lispb::schema::TypeGraph const&;
    auto variants() const -> Variants const&;
    auto find_type(std::string_view module, std::string_view name) const
        -> std::optional<lispb::schema::TypeId>;
    auto find_declaration(lispb::schema::TypeIdentity const& identity) const
        -> std::optional<lispb::schema::DeclarationId>;
    auto declaration_type(lispb::schema::DeclarationId declaration) const
        -> std::optional<lispb::schema::TypeId>;
    auto diagnostics() const -> lispb::Diagnostics const&;
    auto format_diagnostic(lispb::DiagnosticId diagnostic) const -> std::string;
    auto last_diagnostic_begin() const -> std::size_t;
    void record_error(std::string_view message);
    auto transaction() const -> Transaction;
    auto commit(Transaction const& transaction) -> bool;
    auto undo() -> bool;
    auto redo() -> bool;
    auto can_undo() const -> bool;
    auto can_redo() const -> bool;
    auto revision() const -> Revision;
    auto project_path() const -> std::filesystem::path const&;
    auto target_name() const -> std::string_view;
    auto sources() const -> std::vector<DocumentSource>;
    auto declaration_source(std::string_view module, std::string_view name) const
        -> std::optional<std::string>;
    auto dirty() const -> bool;
    auto save() -> bool;
    auto save_as(std::filesystem::path const& project_path, std::string_view target) -> bool;
  private:
    Document();
    auto replay(std::size_t position) -> bool;
    // Keep the compiler's mutually referencing stores at stable addresses when Document moves.
    std::unique_ptr<DocumentState> state_;
};
}
