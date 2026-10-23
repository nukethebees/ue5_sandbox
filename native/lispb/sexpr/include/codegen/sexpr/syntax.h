#pragma once

#include <codegen/sexpr/handle_range.h>
#include <codegen/sexpr/indices.h>
#include <codegen/sexpr/source_span.h>
#include <codegen/sexpr/token_kind.h>
#include <lispb/indices.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>
namespace lispb {
class Strings;
}

namespace lispb::codegen::sexpr {

class Sources;

class Syntax {
  public:
    Syntax(Strings& strings, Sources& sources);

    [[nodiscard]] auto strings() const -> Strings& { return *strings_; }
    [[nodiscard]] auto sources() const -> Sources& { return *sources_; }
    [[nodiscard]] auto kind(TokenId id) const -> TokenKind;
    [[nodiscard]] auto text_id(TokenId id) const -> StringId;
    [[nodiscard]] auto identifier(TokenId id) const -> StringId;
    [[nodiscard]] auto text(TokenId id) const -> std::string_view;
    [[nodiscard]] auto tag(TokenId id) const -> std::string_view;
    [[nodiscard]] auto token_span(TokenId id) const -> SourceSpan;

    [[nodiscard]] auto token(FormId id) const -> TokenId;
    [[nodiscard]] auto kind(FormId id) const -> TokenKind;
    [[nodiscard]] auto text_id(FormId id) const -> StringId;
    [[nodiscard]] auto identifier(FormId id) const -> StringId;
    [[nodiscard]] auto text(FormId id) const -> std::string_view;
    [[nodiscard]] auto tag(FormId id) const -> std::string_view;
    [[nodiscard]] auto token_span(FormId id) const -> SourceSpan;
    [[nodiscard]] auto span(FormId id) const -> SourceSpan;
    [[nodiscard]] auto closing_span(FormId id) const -> SourceSpan;
    [[nodiscard]] auto head(FormId id) const -> std::string_view;
    [[nodiscard]] auto is_list(FormId id) const -> bool;
    // Returned spans are invalidated by appending child lists. Retain handles/ranges across edits.
    [[nodiscard]] auto forms(HandleRange range) const -> std::span<FormId const>;
    [[nodiscard]] auto children(FormId id) const -> std::span<FormId const>;
    [[nodiscard]] auto child(FormId id, std::size_t index) const -> FormId;
    [[nodiscard]] auto range(std::span<FormId const> ids) -> HandleRange;

    [[nodiscard]] auto add_token(TokenKind kind, StringId text, SourceSpan span, StringId tag = {})
        -> TokenId;
    [[nodiscard]] auto add_leaf(TokenId token, ExpansionId expansion = {}) -> FormId;
    [[nodiscard]] auto add_list(TokenId token, std::span<FormId const> children, SourceSpan closing)
        -> FormId;
    [[nodiscard]] auto token_count() const -> std::uint32_t;
    [[nodiscard]] auto form_count() const -> std::uint32_t;
    void add_comment(SourceSpan span);
    [[nodiscard]] auto comments() const -> std::span<SourceSpan const>;
    [[nodiscard]] auto cached_forms(SourceFileId file) const -> std::optional<HandleRange>;
    void cache_forms(SourceFileId file, HandleRange forms);
    void clear();
    void rebind(Strings& strings, Sources& sources);
  private:
    Strings* strings_;
    Sources* sources_;
    std::vector<TokenKind> token_kinds_;
    std::vector<StringId> token_texts_;
    std::vector<SourceSpan> token_spans_;
    std::vector<StringId> token_tags_;
    std::vector<TokenId> form_tokens_;
    std::vector<HandleRange> form_children_;
    std::vector<std::uint32_t> form_ends_;
    std::vector<ExpansionId> form_expansions_;
    std::vector<FormId> child_ids_;
    std::vector<std::optional<HandleRange>> file_forms_;
    std::vector<SourceSpan> comments_;
};

}
