#include <codegen/sexpr/sources.h>

#include <codegen/sexpr/indices.h>
#include <codegen/sexpr/source_location.h>
#include <codegen/sexpr/source_span.h>
#include <codegen/sexpr/syntax.h>
#include <lispb/diagnostics.h>
#include <lispb/indices.h>
#include <lispb/strings.h>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iterator>
#include <limits>
#include <ranges>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace lispb::codegen::sexpr {

Sources::Sources(lispb::Strings& strings, lispb::Diagnostics& diagnostics)
    : strings_{&strings}
    , diagnostics_{&diagnostics} {}

auto Sources::add(std::string_view const path, std::string_view const text) -> SourceFileId {
    assert(text.size() <= std::numeric_limits<std::uint32_t>::max());
    auto const id{SourceFileId{static_cast<std::uint32_t>(paths_.size())}};
    auto const lines_begin{static_cast<std::uint32_t>(line_offsets_.size())};
    line_offsets_.push_back(0);
    auto const count{text.size()};
    for (std::size_t index{}; index < count; ++index) {
        if (text[index] == '\n') {
            line_offsets_.push_back(static_cast<std::uint32_t>(index + 1));
        }
    }

    // Retain both arguments before buffer growth can invalidate a caller's source views.
    std::string const path_text{path};
    texts_.push_back(strings_->retain(text));
    paths_.push_back(lispb::string_id(strings_->intern_retained(path_text)));
    lines_.push_back({lines_begin, static_cast<std::uint32_t>(line_offsets_.size()) - lines_begin});
    kinds_.push_back(SourceFileKind::unspecified);
    states_.push_back(SourceFileState::current);
    includes_.push_back({});
    return id;
}

auto Sources::path(SourceFileId const file) const -> std::string_view {
    return strings_->view(path_id(file));
}

auto Sources::filesystem_path(SourceFileId const file) const -> std::filesystem::path {
    auto result{std::filesystem::path{path(file)}};
    return result.make_preferred();
}
auto Sources::kind(SourceFileId const file) const -> SourceFileKind {
    return kinds_[file.value];
}
void Sources::set_kind(SourceFileId const file, SourceFileKind const kind) {
    kinds_[file.value] = kind;
}
void Sources::set_includes(SourceFileId const file,
                           std::span<SourceFileId const> const targets,
                           std::span<SourceSpan const> const spans) {
    assert(targets.size() == spans.size());
    includes_[file.value] = {static_cast<std::uint32_t>(include_targets_.size()),
                             static_cast<std::uint32_t>(targets.size())};
    include_targets_.insert(include_targets_.end(), targets.begin(), targets.end());
    include_spans_.insert(include_spans_.end(), spans.begin(), spans.end());
}
auto Sources::include_targets(SourceFileId const file) const -> std::span<SourceFileId const> {
    auto const range{includes_[file.value]};
    return std::span{include_targets_}.subspan(range.offset, range.count);
}
auto Sources::include_spans(SourceFileId const file) const -> std::span<SourceSpan const> {
    auto const range{includes_[file.value]};
    return std::span{include_spans_}.subspan(range.offset, range.count);
}

auto Sources::load(std::filesystem::path const& path, std::string_view const display_path)
    -> SourceFileId {
    auto const absolute{std::filesystem::absolute(path).lexically_normal()};
    auto const path_text{absolute.generic_string()};
    std::string const displayed{display_path.empty() ? path_text : display_path};
    auto const key{strings_->intern_retained(path_text)};
    auto const found{loaded_files_.find(key)};
    if (found != loaded_files_.end() && states_[found->second.value] != SourceFileState::stale) {
        return found->second;
    }

    std::ifstream input{absolute, std::ios::binary};
    if (!input) {
        auto const file{add(displayed, {})};
        diagnostics_->fail(*this, {.file = file}, "Cannot open LispB source: " + path_text);
    }
    std::string const text{std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
    if (found != loaded_files_.end() && this->text(found->second) == text) {
        states_[found->second.value] = SourceFileState::current;
        return found->second;
    }
    auto const file{add(displayed, text)};
    loaded_files_.insert_or_assign(key, file);
    return file;
}

auto Sources::text(SourceFileId const file) const -> std::string_view {
    return strings_->view(text_id(file));
}

auto Sources::overlay(std::filesystem::path const& path, std::string_view const text)
    -> SourceFileId {
    auto const absolute{std::filesystem::absolute(path).lexically_normal().generic_string()};
    auto const file{add(absolute, text)};
    auto const key{strings_->find(absolute)};
    states_[file.value] = SourceFileState::overlay;
    loaded_files_.insert_or_assign(key, file);
    return file;
}

auto Sources::path_id(SourceFileId const file) const -> lispb::StringId {
    return paths_.at(file.value);
}

auto Sources::text_id(SourceFileId const file) const -> lispb::StringId {
    return texts_.at(file.value);
}

auto Sources::span(SourceFileId const file,
                   std::uint32_t const begin,
                   std::uint32_t const end) const -> SourceSpan {
    assert(begin <= end && end <= text(file).size());
    return {file, begin, end, {}};
}

auto Sources::location(SourceSpan const span) const -> SourceLocation {
    if (span.file.value == 0) {
        return {};
    }

    auto const range{lines_.at(span.file.value)};
    auto const offsets{std::span{line_offsets_}.subspan(range.offset, range.count)};
    auto const found{std::ranges::upper_bound(offsets, span.offset)};
    auto const line{static_cast<std::uint32_t>(found - offsets.begin())};
    return {line, span.offset - offsets[line - 1] + 1, path(span.file)};
}

auto Sources::add_expansion(lispb::IdentifierId const name,
                            SourceSpan const invocation,
                            SourceSpan const definition) -> ExpansionId {
    auto const id{ExpansionId{static_cast<std::uint32_t>(expansion_names_.size())}};
    expansion_names_.push_back(name);
    invocations_.push_back(invocation);
    definitions_.push_back(definition);
    return id;
}

auto Sources::expansion_text(SourceSpan const span) const -> std::string {
    std::string result;
    for (auto id{span.expansion}; id.value != 0; id = invocations_[id.value].expansion) {
        result += "\n  macro '";
        result += strings_->view(expansion_names_[id.value]);
        result += "' defined at ";
        result += format_location(*this, definitions_[id.value]);
        result += "\n  expanded at ";
        result += format_location(*this, invocations_[id.value]);
    }
    return result;
}

void Sources::set_path(SourceFileId const file, std::string_view const path) {
    paths_.at(file.value) = lispb::string_id(strings_->intern_retained(path));
}

void Sources::refresh() {
    for (auto const& [path, file] : loaded_files_) {
        if (states_[file.value] != SourceFileState::overlay) {
            states_[file.value] = SourceFileState::stale;
        }
    }
}

void Sources::clear_expansions() {
    expansion_names_.resize(1);
    invocations_.resize(1);
    definitions_.resize(1);
}
void Sources::clear() {
    paths_.resize(1);
    texts_.resize(1);
    lines_.resize(1);
    kinds_.resize(1);
    states_.resize(1);
    includes_.resize(1);
    include_targets_.clear();
    include_spans_.clear();
    line_offsets_.clear();
    loaded_files_.clear();
    clear_expansions();
}

}
