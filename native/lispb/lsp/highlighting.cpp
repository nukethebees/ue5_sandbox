#include "analysis.h"

#include <algorithm>
#include <charconv>

namespace lispb::lsp {
static auto atom_kind(std::string_view atom, bool head) -> std::optional<SemanticKind> {
    if (atom.starts_with('@')) {
        return SemanticKind::type;
    }
    if (atom.starts_with('$')) {
        return SemanticKind::parameter;
    }
    if (atom == "true" || atom == "false") {
        return SemanticKind::keyword;
    }
    auto number{atom};
    if (number.starts_with('+')) {
        number.remove_prefix(1);
    }
    if (!number.empty()) {
        double value{};
        auto const end{number.data() + number.size()};
        auto const result{std::from_chars(number.data(), end, value)};
        if (result.ec == std::errc{} && result.ptr == end) {
            return SemanticKind::number;
        }
    }
    if (head) {
        if (!atom.empty() && atom.find_first_not_of("+-*/<>=!&|%") == atom.npos) {
            return SemanticKind::operation;
        }
        return SemanticKind::keyword;
    }
    return {};
}

auto Analysis::classify(Document const& doc) const -> std::vector<TokenSegment> {
    auto& storage{compiler_.storage()};
    auto& syntax{storage.syntax};
    struct ClassifiedSpan {
        sexpr::SourceSpan span;
        SemanticKind kind;
        bool declaration{};
    };
    std::vector<ClassifiedSpan> spans;
    auto declaration_kind = [&](SourcePosition pos) -> std::optional<SemanticKind> {
        auto const it{
            std::ranges::lower_bound(declarations_, pos, {}, &DeclarationLocation::start)};
        if (it != declarations_.end() && it->start == pos) {
            return it->kind;
        }
        return {};
    };
    auto visit = [&](auto&& self, sexpr::FormId form, bool head) -> void {
        if (syntax.is_list(form)) {
            auto const children{syntax.children(form)};
            for (auto child : children) {
                self(self, child, child == children.front());
            }
            return;
        }
        auto const span{syntax.token_span(form)};
        SourcePosition const pos{span.file, span.offset};
        auto kind{declaration_kind(pos)};
        bool const declaration{kind.has_value()};
        if (!kind) {
            auto const it{
                std::ranges::lower_bound(facts_.references, pos, {}, [](auto const& link) {
                    return SourcePosition{link.use.file, link.use.offset};
                })};
            if (it != facts_.references.end() && it->use.file == pos.file &&
                it->use.offset == pos.offset) {
                kind = declaration_kind({it->declaration.file, it->declaration.offset});
            }
        }
        if (!kind) {
            switch (syntax.kind(form)) {
                case sexpr::TokenKind::string:
                case sexpr::TokenKind::raw_literal:
                    kind = SemanticKind::string;
                    break;
                case sexpr::TokenKind::keyword:
                    kind = SemanticKind::keyword;
                    break;
                case sexpr::TokenKind::atom:
                    kind = atom_kind(syntax.text(form), head);
                    break;
                default:
                    break;
            }
        }
        if (kind) {
            spans.push_back({span, *kind, declaration});
        }
    };
    for (auto root : syntax.forms(doc.roots)) {
        visit(visit, root, false);
    }
    for (auto span : syntax.comments()) {
        if (span.file == doc.file) {
            spans.push_back({span, SemanticKind::comment});
        }
    }
    std::ranges::sort(spans, {}, [](auto const& item) { return item.span.offset; });

    auto const text{storage.sources.text(doc.file)};
    auto const lines{storage.sources.line_offsets(doc.file)};
    std::vector<TokenSegment> segments;
    std::uint32_t line{};
    std::uint32_t cursor{};
    std::uint32_t column{};
    auto const line_count{lines.size()};
    for (auto const& item : spans) {
        auto begin{item.span.offset};
        // The strict parse and editor recovery can observe the same comment.
        if (begin < cursor) {
            continue;
        }
        while (begin < item.span.end_offset) {
            while (line + 1 < line_count && lines[line + 1] <= begin) {
                cursor = lines[++line];
                column = 0;
            }
            auto const end{line + 1 < line_count ? std::min(lines[line + 1], item.span.end_offset)
                                                 : item.span.end_offset};
            auto content_end{end};
            while (content_end > begin &&
                   (text[content_end - 1] == '\r' || text[content_end - 1] == '\n')) {
                --content_end;
            }
            column += utf16_length(text.substr(cursor, begin - cursor));
            auto const length{utf16_length(text.substr(begin, content_end - begin))};
            if (length) {
                segments.push_back({line, column, length, item.kind, item.declaration});
            }
            column += length;
            cursor = end;
            begin = end;
        }
    }
    return segments;
}

auto Analysis::highlighting(Document& doc, Json const& params) -> Json {
    if (!doc.highlighting) {
        doc.highlighting.emplace(classify(doc));
    }
    auto const& segments{*doc.highlighting};
    auto first{segments.begin()};
    auto last{segments.end()};
    if (auto bounds{params.find("range")}; bounds != params.end()) {
        auto const& start{bounds->at("start")};
        auto const& end{bounds->at("end")};
        auto const start_line{start.at("line").get<std::uint32_t>()};
        auto const start_column{start.at("character").get<std::uint32_t>()};
        auto const end_line{end.at("line").get<std::uint32_t>()};
        auto const end_column{end.at("character").get<std::uint32_t>()};
        if (start_line == end_line && start_column == end_column) {
            return {{"data", Json::array()}};
        }
        first = std::partition_point(first, last, [&](auto const& token) {
            return token.line < start_line ||
                   (token.line == start_line && token.character + token.length <= start_column);
        });
        last = std::partition_point(first, last, [&](auto const& token) {
            return token.line < end_line ||
                   (token.line == end_line && token.character < end_column);
        });
    }
    std::vector<std::uint32_t> data;
    data.reserve(static_cast<size_t>(last - first) * 5);
    std::uint32_t previous_line{};
    std::uint32_t previous_column{};
    for (auto const& token : std::ranges::subrange(first, last)) {
        data.insert(
            data.end(),
            {token.line - previous_line,
             token.line == previous_line ? token.character - previous_column : token.character,
             token.length,
             static_cast<std::uint32_t>(token.kind),
             token.declaration ? 1u : 0u});
        previous_line = token.line;
        previous_column = token.character;
    }
    return {{"data", std::move(data)}};
}
}
