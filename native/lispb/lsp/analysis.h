#pragma once
#include "form_head.h"
#include "markup_kind.h"
#include "method.h"
#include "protocol.h"
#include "semantic_kind.h"

#include <lispb/compiler.h>
#include <lispb/project.h>

namespace lispb::lsp {
struct ClientCapabilities {
    bool hierarchical_symbols{};
    MarkupKind hover_format{MarkupKind::plaintext};
    MarkupKind completion_format{MarkupKind::plaintext};
};

struct Buffer {
    std::string uri;
    std::string text;
    std::int64_t version;
};
using WorkspaceFileId = Index<struct WorkspaceFileSlot, UINT32_MAX>;
using UnitId = Index<struct UnitSlot, UINT32_MAX>;
struct WorkspaceFile {
    fs::path path;
    std::optional<Buffer> buffer;
    UnitId active;
    bool published{};
};
struct SourcePosition {
    sexpr::SourceFileId file;
    std::uint32_t offset;
    auto operator<=>(SourcePosition const&) const = default;
};
struct TokenSegment {
    std::uint32_t line;
    std::uint32_t character;
    std::uint32_t length;
    SemanticKind kind;
    bool declaration;
};

using SymbolId = Index<struct SymbolSlot>;
struct Symbol {
    sexpr::FormId form;
    sexpr::FormId name;
    SymbolId parent;
    SemanticKind kind;
    schema::TypeId type{};
};
struct Document {
    auto parent_form(Symbol const& symbol) const -> sexpr::FormId {
        return symbol.parent.value ? symbols[symbol.parent.value - 1].form : sexpr::FormId{};
    }
    fs::path path;
    sexpr::SourceFileId file;
    sexpr::HandleRange roots;
    std::vector<Symbol> symbols;
    std::optional<std::vector<TokenSegment>> highlighting;
    std::vector<std::uint32_t> comment_lines;
};

class Analysis {
  public:
    Analysis(std::span<WorkspaceFile const> files, fs::path const& entry);
    Analysis(std::span<WorkspaceFile const> files, Project const& project, TargetId target);
    auto contains(fs::path const& path) const -> bool;
    auto project() const -> Project const* { return project_ ? &*project_ : nullptr; }
    auto diagnostic(DiagnosticId id) const -> Json;
    auto diagnostics() const -> auto const& { return diagnostics_; }
    auto query(fs::path const& path,
               Method method,
               Json const& params,
               ClientCapabilities capabilities) -> std::expected<Json, std::string>;
    auto root_head(fs::path const& path) -> std::optional<FormHead>;
  private:
    void overlay(std::span<WorkspaceFile const> files);
    void finish(std::vector<fs::path>& inputs, fs::path const& fallback);
    auto classify(Document const& doc) const -> std::vector<TokenSegment>;
    auto highlighting(Document& doc, Json const& params) -> Json;
    void source_error(fs::path const& path, std::string_view message);
    void
        index(Document& doc, sexpr::FormId form, SymbolId parent_symbol, sexpr::FormId parent_form);
    void link_types(schema::TypeGraph const& graph);
    auto document(sexpr::SourceFileId file) const -> Document const&;
    auto completion(Document const& doc,
                    std::span<sexpr::FormId const> context,
                    std::uint32_t offset,
                    MarkupKind format) -> Json;
    auto location(sexpr::SourceSpan span) const -> Json;
    auto reference_spans(sexpr::SourceSpan declaration, bool include_declaration) const
        -> std::vector<sexpr::SourceSpan>;
    auto rename(Document const& doc,
                sexpr::SourceSpan declaration,
                sexpr::FormId use,
                Method method,
                Json const& params) const -> std::expected<Json, std::string>;
    auto expression_hover(sexpr::FormId form, sexpr::FormId use, MarkupKind format) const -> Json;
    auto signature(Symbol const& symbol) const -> std::string_view;
    auto declaration_details(Document const& doc, Symbol const& symbol, MarkupKind format) const
        -> std::string;
    auto outline(Document const& doc, bool hierarchical) -> Json;
    auto hover(Document const& doc,
               sexpr::SourceSpan declaration,
               sexpr::FormId use,
               MarkupKind format) const -> Json;
    auto folds(Document const& doc) -> Json;
    auto lookup(Document const& doc, std::span<sexpr::FormId const> context)
        -> std::optional<sexpr::SourceSpan>;

    Compiler compiler_;
    sexpr::EditorFacts facts_;
    std::optional<Project> project_;
    fs::path root_;
    struct DeclarationLocation {
        SourcePosition start;
        SemanticKind kind;
    };
    std::vector<DeclarationLocation> declarations_;
    std::vector<Document> documents_;
    std::vector<std::uint32_t> document_indices_;
    struct Issue {
        fs::path path;
        DiagnosticId id;
    };
    std::vector<Issue> diagnostics_;
};
}
