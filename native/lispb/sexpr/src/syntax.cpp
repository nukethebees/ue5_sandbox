#include <codegen/sexpr/syntax.h>

#include <codegen/sexpr/handle_range.h>
#include <codegen/sexpr/indices.h>
#include <codegen/sexpr/source_error.h>
#include <codegen/sexpr/source_span.h>
#include <codegen/sexpr/sources.h>
#include <codegen/sexpr/token_kind.h>
#include <lispb/indices.h>
#include <lispb/strings.h>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>

namespace lispb::codegen::sexpr {

Syntax::Syntax(lispb::Strings& strings, Sources& sources)
    : strings_{&strings}
    , sources_{&sources} {
    clear();
}

auto Syntax::kind(TokenId const id) const -> TokenKind {
    assert(id.value > 0 && id.value < token_kinds_.size());
    return token_kinds_[id.value];
}
auto Syntax::text_id(TokenId const id) const -> StringId {
    return token_texts_[id.value];
}
auto Syntax::identifier(TokenId const id) const -> IdentifierId {
    auto const token_kind{kind(id)};
    return token_kind == TokenKind::atom || token_kind == TokenKind::keyword
             ? IdentifierId{text_id(id).value}
             : IdentifierId{};
}
auto Syntax::text(TokenId const id) const -> std::string_view {
    return strings_->view(text_id(id));
}
auto Syntax::tag(TokenId const id) const -> std::string_view {
    return strings_->view(token_tags_[id.value]);
}
auto Syntax::token_span(TokenId const id) const -> SourceSpan {
    return token_spans_[id.value];
}
auto Syntax::token(FormId const id) const -> TokenId {
    assert(id.value > 0 && id.value < form_tokens_.size());
    return form_tokens_[id.value];
}
auto Syntax::kind(FormId const id) const -> TokenKind {
    return kind(token(id));
}
auto Syntax::text_id(FormId const id) const -> StringId {
    return text_id(token(id));
}
auto Syntax::identifier(FormId const id) const -> IdentifierId {
    return identifier(token(id));
}
auto Syntax::text(FormId const id) const -> std::string_view {
    return text(token(id));
}
auto Syntax::tag(FormId const id) const -> std::string_view {
    return tag(token(id));
}
auto Syntax::token_span(FormId const id) const -> SourceSpan {
    auto result{token_span(token(id))};
    if (form_expansions_[id.value].value != 0) {
        result.expansion = form_expansions_[id.value];
    }
    return result;
}
auto Syntax::span(FormId const id) const -> SourceSpan {
    auto result{token_span(id)};
    result.end_offset = form_ends_[id.value];
    return result;
}
auto Syntax::closing_span(FormId const id) const -> SourceSpan {
    assert(is_list(id));
    auto result{span(id)};
    result.offset = result.end_offset - 1;
    return result;
}
auto Syntax::head(FormId const id) const -> std::string_view {
    auto const list{children(id)};
    return is_list(id) && !list.empty() && kind(list.front()) == TokenKind::atom
             ? text(list.front())
             : std::string_view{};
}
auto Syntax::is_list(FormId const id) const -> bool {
    return kind(id) == TokenKind::left_parenthesis;
}
auto Syntax::forms(HandleRange const range) const -> std::span<FormId const> {
    return std::span{child_ids_}.subspan(range.offset, range.count);
}
auto Syntax::children(FormId const id) const -> std::span<FormId const> {
    return forms(form_children_[id.value]);
}
auto Syntax::child(FormId const id, std::size_t const index) const -> FormId {
    return children(id)[index];
}
auto Syntax::range(std::span<FormId const> const ids) -> HandleRange {
    assert(child_ids_.size() + ids.size() <= std::numeric_limits<std::uint32_t>::max());
    auto const range{HandleRange{static_cast<std::uint32_t>(child_ids_.size()),
                                 static_cast<std::uint32_t>(ids.size())}};
    child_ids_.insert(child_ids_.end(), ids.begin(), ids.end());
    return range;
}

auto Syntax::add_token(TokenKind const kind,
                       lispb::StringId const text,
                       SourceSpan const span,
                       lispb::IdentifierId const tag) -> TokenId {
    auto const id{TokenId{token_count()}};
    auto const stored_text{kind == TokenKind::atom || kind == TokenKind::keyword
                               ? lispb::string_id(strings_->intern(text))
                               : text};
    token_kinds_.push_back(kind);
    token_texts_.push_back(stored_text);
    token_spans_.push_back(span);
    token_tags_.push_back(tag);
    return id;
}

auto Syntax::add_leaf(TokenId const token, ExpansionId const expansion) -> FormId {
    auto const id{FormId{form_count()}};
    form_tokens_.push_back(token);
    form_children_.push_back({});
    form_ends_.push_back(token_spans_[token.value].end_offset);
    form_expansions_.push_back(expansion);
    return id;
}

auto Syntax::add_list(TokenId const token,
                      std::span<FormId const> const children,
                      SourceSpan const closing) -> FormId {
    assert(token_kinds_[token.value] == TokenKind::left_parenthesis);
    auto const child_range{range(children)};
    auto const id{FormId{form_count()}};
    form_tokens_.push_back(token);
    form_children_.push_back(child_range);
    form_ends_.push_back(closing.end_offset);
    form_expansions_.push_back(closing.expansion);
    return id;
}

auto Syntax::token_count() const -> std::uint32_t {
    return static_cast<std::uint32_t>(token_kinds_.size());
}

auto Syntax::form_count() const -> std::uint32_t {
    return static_cast<std::uint32_t>(form_tokens_.size());
}

void Syntax::clear() {
    token_kinds_.assign(1, TokenKind::end);
    token_texts_.assign(1, {});
    token_spans_.assign(1, {});
    token_tags_.assign(1, {});
    form_tokens_.assign(1, {});
    form_children_.assign(1, {});
    form_ends_.assign(1, 0);
    form_expansions_.assign(1, {});
    child_ids_.clear();
    file_forms_.clear();
}

auto Syntax::cached_forms(SourceFileId const file) const -> std::optional<HandleRange> {
    if (file.value < file_forms_.size() && file_forms_[file.value].has_value()) {
        return file_forms_[file.value];
    }
    return std::nullopt;
}

void Syntax::cache_forms(SourceFileId const file, HandleRange const forms) {
    if (file_forms_.size() <= file.value) {
        file_forms_.resize(static_cast<std::size_t>(file.value) + 1);
    }
    file_forms_[file.value] = forms;
}

void Syntax::rebind(lispb::Strings& strings, Sources& sources) {
    strings_ = &strings;
    sources_ = &sources;
}

}
