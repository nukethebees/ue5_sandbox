#include <lispb/schema/source_comments.h>

#include <codegen/schema/declaration_schema.h>
#include <codegen/schema/declarations.h>
#include <codegen/sexpr/reader.h>
#include <codegen/sexpr/syntax.h>
#include <codegen/sexpr/token_kind.h>
#include <lispb/compiler_storage.h>

#include <algorithm>
#include <cassert>
#include <functional>
#include <optional>
#include <ranges>
#include <utility>

namespace lispb::schema {
namespace {

using codegen::sexpr::FormId;
using codegen::sexpr::SourceFileId;
using codegen::sexpr::Syntax;
using codegen::sexpr::TokenKind;

auto named_key(Syntax const& syntax, FormId const form) -> std::string {
    auto const children{syntax.children(form)};
    if (!syntax.is_list(form) || children.size() < 2 || syntax.is_list(children[1])) {
        return {};
    }
    return std::string{syntax.head(form)} + '\0' + std::string{syntax.text(children[1])};
}

auto module_name(codegen::Declarations const& declarations, codegen::ModuleHandle const module)
    -> std::string_view {
    auto const settings{declarations.module_settings(module)};
    auto const index{declarations.module_settings_store.index(settings)};
    return declarations.strings().view(declarations.module_settings_store.name[index]);
}

auto module_declarations(codegen::Declarations const& declarations,
                         codegen::ModuleHandle const module)
    -> std::span<codegen::DeclarationHandle const> {
    if (module.kind() != codegen::ModuleKind::normal) {
        return {};
    }
    return declarations.items(
        declarations.normal_modules.declarations[declarations.normal_modules.index(module)]);
}

auto indentation(std::string_view const text, std::size_t const offset) -> std::string {
    auto const newline{text.rfind('\n', offset == 0 ? 0 : offset - 1)};
    auto const begin{newline == std::string_view::npos ? 0 : newline + 1};
    auto end{begin};
    while (end < text.size() && (text[end] == ' ' || text[end] == '\t')) {
        ++end;
    }
    return std::string{text.substr(begin, end - begin)};
}

auto same_line(std::string_view const text, std::size_t const begin, std::size_t const end)
    -> bool {
    return text.substr(begin, end - begin).find('\n') == std::string_view::npos;
}

} // namespace

void SourceComments::capture(CompilerStorage const& storage,
                             std::span<codegen::ModuleHandle const> const modules,
                             SourceFileId const file) {
    auto const& syntax{storage.syntax};
    auto const form_count{syntax.form_count()};
    for (auto const module : modules) {
        if (modules_.contains(module)) {
            continue;
        }
        auto const name{module_name(storage.declarations, module)};
        for (std::uint32_t index{1}; index < form_count; ++index) {
            auto const form{FormId{index}};
            if (file.value != 0 && syntax.span(form).file != file) {
                continue;
            }
            auto const head{syntax.head(form)};
            if ((head != "module" && head != "settings-module" && head != "umbrella-module") ||
                syntax.children(form).size() < 2 || syntax.text(syntax.child(form, 1)) != name) {
                continue;
            }
            modules_.emplace(module, form);
            for (auto const declaration : module_declarations(storage.declarations, module)) {
                auto const declaration_name{storage.strings.view(
                    codegen::declaration_name(storage.declarations, declaration))};
                for (auto const child : syntax.children(form)) {
                    if (syntax.is_list(child) && syntax.children(child).size() >= 2 &&
                        syntax.text(syntax.child(child, 1)) == declaration_name) {
                        declarations_.emplace(declaration, child);
                        break;
                    }
                }
            }
            break;
        }
    }

    auto const previous_files{captured_files_};
    for (auto const span : syntax.comments()) {
        if (file.value != 0 && span.file != file) {
            continue;
        }
        if (previous_files.contains(span.file)) {
            continue;
        }
        captured_files_.insert(span.file);
        auto const source{storage.sources.text(span.file)};
        auto const roots{syntax.cached_forms(span.file)};
        if (!roots) {
            continue;
        }
        auto children{syntax.forms(*roots)};
        FormId parent{};

        // Descend to the smallest containing list; lexer trivia never overlaps a leaf.
        for (;;) {
            auto const containing{std::ranges::find_if(children, [&](FormId const candidate) {
                auto const bounds{syntax.span(candidate)};
                return syntax.is_list(candidate) && bounds.offset <= span.offset &&
                       bounds.end_offset >= span.end_offset;
            })};
            if (containing == children.end()) {
                break;
            }
            parent = *containing;
            children = syntax.children(parent);
        }

        FormId previous{};
        FormId next{};
        FormId property_parent{};
        bool previous_is_property{};
        for (std::size_t index{}; index < children.size(); ++index) {
            auto const child{children[index]};
            if (syntax.span(child).end_offset <= span.offset) {
                previous = child;
                previous_is_property =
                    syntax.kind(child) == TokenKind::keyword ||
                    (index > 0 && syntax.kind(children[index - 1]) == TokenKind::keyword);
            } else if (syntax.span(child).offset >= span.end_offset) {
                next = child;
                break;
            }
        }

        FormId owner{};
        SourceCommentPlacement placement{};
        if (previous.value != 0 &&
            same_line(source, syntax.span(previous).end_offset, span.offset)) {
            owner = previous;
            placement = SourceCommentPlacement::trailing;
            property_parent = previous_is_property ? parent : FormId{};
        } else if (parent.value == 0 && previous.value == 0) {
            placement = SourceCommentPlacement::file_header;
        } else if (next.value != 0) {
            owner = next;
            placement = SourceCommentPlacement::leading;
            property_parent = syntax.kind(next) == TokenKind::keyword ? parent : FormId{};
        } else {
            owner = parent;
            placement = parent.value == 0 ? SourceCommentPlacement::file_trailer
                                          : SourceCommentPlacement::before_close;
        }
        owners_.push_back(owner);
        property_parents_.push_back(property_parent);
        placements_.push_back(placement);
        spans_.push_back(span);
    }
    for (auto const& [declaration, form] : declarations_) {
        auto const declaration_span{syntax.span(form)};
        for (auto const owner : owners_) {
            if (owner.value == 0) {
                continue;
            }
            auto const owner_span{syntax.span(owner)};
            if (owner_span.file == declaration_span.file &&
                owner_span.offset >= declaration_span.offset &&
                owner_span.end_offset <= declaration_span.end_offset) {
                authored_comments_.insert(declaration);
                break;
            }
        }
    }
}

void SourceComments::inherit(codegen::DeclarationHandle const before,
                             codegen::DeclarationHandle const after) {
    if (authored_comments_.contains(after)) {
        return;
    }
    if (auto const found{declarations_.find(before)}; found != declarations_.end()) {
        declarations_[after] = found->second;
    }
    if (auto const found{renamed_children_.find(before)}; found != renamed_children_.end()) {
        renamed_children_[after] = found->second;
    }
}

void SourceComments::inherit(codegen::ModuleHandle const before,
                             codegen::ModuleHandle const after) {
    if (auto const found{modules_.find(before)}; found != modules_.end()) {
        modules_[after] = found->second;
    }
}

void SourceComments::rename_child(codegen::DeclarationHandle const declaration,
                                  std::string_view const head,
                                  std::string_view const before,
                                  std::string_view const after) {
    auto& names{renamed_children_[declaration]};
    auto const before_key{std::string{head} + '\0' + std::string{before}};
    auto const found{names.find(before_key)};
    auto const original{found == names.end() ? std::string{before} : found->second};
    names[before_key] = {};
    names[std::string{head} + '\0' + std::string{after}] = original;
}

void SourceComments::remove_child(codegen::DeclarationHandle const declaration,
                                  std::string_view const head,
                                  std::string_view const name) {
    renamed_children_[declaration][std::string{head} + '\0' + std::string{name}] = {};
}

auto SourceComments::apply(CompilerStorage const& storage,
                           std::span<codegen::ModuleHandle const> const modules,
                           SourceFileId const file,
                           std::string canonical) const -> std::string {
    auto const original{file.value == 0 ? std::string_view{} : storage.sources.text(file)};
    auto const newline{original.find("\r\n") == std::string_view::npos ? "\n" : "\r\n"};
    if (newline == std::string_view{"\r\n"}) {
        std::string converted;
        for (auto const character : canonical) {
            if (character == '\n') {
                converted += '\r';
            }
            converted += character;
        }
        canonical = std::move(converted);
    }

    CompilerStorage rendered;
    auto const rendered_file{rendered.sources.add("<canonical>", canonical)};
    auto const rendered_roots{codegen::sexpr::read_forms(rendered.syntax, rendered_file)};
    auto const roots{rendered.syntax.forms(rendered_roots)};
    assert(roots.size() == modules.size());
    auto const& source_syntax{storage.syntax};
    auto const& target_syntax{rendered.syntax};
    std::map<FormId, FormId> mapped;
    std::map<std::pair<std::size_t, std::size_t>, std::string> replacements;

    auto map_forms = [&](auto&& self,
                         FormId const source,
                         FormId const target,
                         std::map<std::string, std::string> const* const renamed) -> void {
        mapped[source] = target;
        if (source_syntax.kind(source) == TokenKind::raw_literal && target_syntax.is_list(target) &&
            target_syntax.children(target).size() == 1 &&
            target_syntax.kind(target_syntax.child(target, 0)) == TokenKind::string &&
            target_syntax.text(target_syntax.child(target, 0)) == source_syntax.text(source)) {
            auto const source_span{source_syntax.span(source)};
            auto const target_span{target_syntax.span(target)};
            replacements[{target_span.offset, target_span.end_offset}] =
                storage.sources.text(source_span.file)
                    .substr(source_span.offset, source_span.end_offset - source_span.offset);
            return;
        }
        if (!source_syntax.is_list(source) || !target_syntax.is_list(target)) {
            return;
        }

        auto const source_children{source_syntax.children(source)};
        auto const target_children{target_syntax.children(target)};
        std::set<FormId> used;
        for (std::size_t target_index{}; target_index < target_children.size(); ++target_index) {
            auto const target_child{target_children[target_index]};
            std::optional<FormId> source_child;
            if (target_syntax.kind(target_child) == TokenKind::keyword) {
                auto const property{target_syntax.text(target_child)};
                auto const found{std::ranges::find_if(source_children, [&](FormId const child) {
                    return source_syntax.kind(child) == TokenKind::keyword &&
                           source_syntax.text(child) == property;
                })};
                if (found != source_children.end()) {
                    self(self, *found, target_child, renamed);
                    assert(std::next(found) != source_children.end());
                    assert(target_index + 1 < target_children.size());
                    self(self, *std::next(found), target_children[target_index + 1], renamed);
                    used.insert(*found);
                    used.insert(*std::next(found));
                }
                ++target_index;
                continue;
            }
            if (target_syntax.is_list(target_child)) {
                if (source_syntax.head(source) == "module" &&
                    target_syntax.head(target_child) != "array-allocator") {
                    continue;
                }
                auto key{named_key(target_syntax, target_child)};
                if (renamed != nullptr) {
                    if (auto const found{renamed->find(key)}; found != renamed->end()) {
                        key = std::string{target_syntax.head(target_child)} + '\0' + found->second;
                    }
                }
                auto const found{std::ranges::find_if(source_children, [&](FormId const child) {
                    return !used.contains(child) && source_syntax.is_list(child) &&
                           named_key(source_syntax, child) == key;
                })};
                if (found != source_children.end()) {
                    source_child = *found;
                }
            } else if (target_index < source_children.size() &&
                       !source_syntax.is_list(source_children[target_index]) &&
                       source_syntax.kind(source_children[target_index]) != TokenKind::keyword) {
                source_child = source_children[target_index];
            }
            if (source_child && !used.contains(*source_child)) {
                self(self, *source_child, target_child, renamed);
                used.insert(*source_child);
            }
        }
    };

    for (std::size_t index{}; index < modules.size(); ++index) {
        auto const module{modules[index]};
        auto const target{roots[index]};
        if (auto const found{modules_.find(module)}; found != modules_.end()) {
            map_forms(map_forms, found->second, target, nullptr);
        }
        for (auto const declaration : module_declarations(storage.declarations, module)) {
            auto const found{declarations_.find(declaration)};
            if (found == declarations_.end()) {
                continue;
            }
            auto const name{
                storage.strings.view(codegen::declaration_name(storage.declarations, declaration))};
            auto const children{target_syntax.children(target)};
            auto const child{std::ranges::find_if(children, [&](FormId const candidate) {
                return target_syntax.is_list(candidate) &&
                       target_syntax.children(candidate).size() >= 2 &&
                       target_syntax.text(target_syntax.child(candidate, 1)) == name;
            })};
            assert(child != children.end());
            auto const renamed{renamed_children_.find(declaration)};
            map_forms(map_forms,
                      found->second,
                      *child,
                      renamed == renamed_children_.end() ? nullptr : &renamed->second);
        }
    }

    auto const count{owners_.size()};
    for (std::size_t index{}; index < count; ++index) {
        auto placement{placements_[index]};
        auto found{mapped.find(owners_[index])};
        if (placement == SourceCommentPlacement::file_header ||
            placement == SourceCommentPlacement::file_trailer) {
            if (spans_[index].file != file) {
                continue;
            }
        } else if (found == mapped.end()) {
            found = mapped.find(property_parents_[index]);
            if (found == mapped.end()) {
                continue;
            }
            placement = SourceCommentPlacement::before_close;
        }

        auto const span{spans_[index]};
        auto comment{
            storage.sources.text(span.file).substr(span.offset, span.end_offset - span.offset)};
        if (comment.ends_with('\r')) {
            comment.remove_suffix(1);
        }
        std::size_t offset{};
        std::string insertion;
        if (placement == SourceCommentPlacement::file_header) {
            insertion = std::string{comment} + newline;
        } else if (placement == SourceCommentPlacement::file_trailer) {
            offset = canonical.size();
            insertion = std::string{comment} + newline;
        } else {
            auto const target{found->second};
            auto const bounds{target_syntax.span(target)};
            auto const indent{indentation(canonical, bounds.offset)};
            switch (placement) {
                case SourceCommentPlacement::leading:
                    offset = bounds.offset;
                    insertion = std::string{comment} + newline + indent;
                    if (offset > indent.size() && canonical[offset - indent.size() - 1] != '\n') {
                        insertion = std::string{newline} + indent + insertion;
                    }
                    break;
                case SourceCommentPlacement::trailing:
                    offset = bounds.end_offset;
                    insertion = " " + std::string{comment} + newline + indent;
                    break;
                case SourceCommentPlacement::before_close:
                    offset = bounds.end_offset - 1;
                    insertion = std::string{newline} + indent + "  " + std::string{comment} +
                                newline + indent;
                    break;
                default:
                    std::unreachable();
            }
        }
        replacements[{offset, offset}] += insertion;
    }

    // Insert comments into freshly rendered syntax; no authored whitespace is patched.
    for (auto const& [range, replacement] : replacements | std::views::reverse) {
        canonical.replace(range.first, range.second - range.first, replacement);
    }
    return canonical;
}

} // namespace lispb::schema
