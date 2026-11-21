#include "analysis.h"

#include "completion_kind.h"
#include "outline_kind.h"

#include <codegen/path_utils.h>
#include <codegen/schema/schema_property.h>
#include <codegen/schema/schema_version.h>
#include <codegen/sexpr/reader.h>
#include <codegen/sexpr/source_error.h>
#include <codegen/source_loader.h>
#include <kernel_codegen/emission_field.h>
#include <kernel_codegen/operation_field.h>
#include <kernel_codegen/operation_kind.h>
#include <kernel_codegen/variant_kind.h>
#include <lispb/material_compiler.h>
#include <lispb/project_loader.h>
#include <lispb/project_property.h>
#include <material_gen/MaterialClause.h>
#include <material_gen/MaterialOperator.h>
#include <slate_codegen/parameter_kind.h>
#include <slate_codegen/structural_form.h>

#include <algorithm>

namespace lispb::lsp {
using Kind = SemanticKind;
static auto module_head(std::optional<FormHead> head) -> bool {
    return head && is_schema_module(*head);
}
static auto symbol_kind(std::string_view spelling,
                        std::optional<FormHead> head,
                        std::optional<FormHead> parent) -> std::optional<Kind> {
    if (module_head(parent)) {
        if (auto decl{codegen::try_parse_serialized_declaration_kind(spelling)}) {
            return *decl == codegen::DeclarationKind::enumeration ? Kind::enumeration
                                                                  : Kind::structure;
        }
    }
    if ((parent == FormHead::lispb_project && try_parse_serialized_target_kind(spelling)) ||
        module_head(head)) {
        return Kind::name_space;
    }
    if (kernel_codegen::detail::try_parse_serialized_operation_kind(spelling)) {
        return Kind::function;
    }
    if (parent == FormHead::params) {
        return Kind::parameter;
    }
    if (!head) {
        return {};
    }
    switch (*head) {
        case FormHead::kernel_module:
        case FormHead::material:
        case FormHead::widget_library:
            return Kind::name_space;
        case FormHead::type:
        case FormHead::type_set:
            return Kind::type;
        case FormHead::widget_class:
            return Kind::class_type;
        case FormHead::defmacro:
            return Kind::macro;
        case FormHead::function:
            return Kind::function;
        case FormHead::method:
            return Kind::method;
        case FormHead::value:
            if (parent == FormHead::enum_type) {
                return Kind::enumerator;
            }
            break;
        case FormHead::member:
        case FormHead::column:
        case FormHead::setting:
        case FormHead::category:
            return Kind::property;
        case FormHead::operand:
        case FormHead::component_operand:
        case FormHead::parameter:
            return Kind::parameter;
        case FormHead::let:
            if (parent == FormHead::material) {
                return Kind::variable;
            }
            break;
        default:
            break;
    }
    return {};
}
static auto outline_kind(Kind kind) -> OutlineKind {
    switch (kind) {
        case Kind::name_space:
            return OutlineKind::name_space;
        case Kind::class_type:
        case Kind::type:
            return OutlineKind::class_type;
        case Kind::method:
            return OutlineKind::method;
        case Kind::property:
            return OutlineKind::property;
        case Kind::enumeration:
            return OutlineKind::enumeration;
        case Kind::macro:
        case Kind::function:
            return OutlineKind::function;
        case Kind::variable:
        case Kind::parameter:
            return OutlineKind::variable;
        case Kind::enumerator:
            return OutlineKind::enumerator;
        case Kind::structure:
            return OutlineKind::structure;
        default:
            std::unreachable();
    }
}
static auto key(sexpr::SourceSpan span) -> SourcePosition {
    return {span.file, span.offset};
}
static auto completion_kind(Kind kind) -> CompletionKind {
    switch (kind) {
        case Kind::name_space:
            return CompletionKind::module_kind;
        case Kind::type:
        case Kind::class_type:
        case Kind::structure:
            return CompletionKind::class_type;
        case Kind::enumeration:
            return CompletionKind::enumeration;
        case Kind::enumerator:
            return CompletionKind::value;
        case Kind::macro:
        case Kind::function:
            return CompletionKind::function;
        case Kind::method:
            return CompletionKind::method;
        case Kind::property:
            return CompletionKind::property;
        case Kind::parameter:
        case Kind::variable:
            return CompletionKind::variable;
        default:
            std::unreachable();
    }
}
void Analysis::overlay(std::span<WorkspaceFile const> files) {
    auto& storage{compiler_.storage()};
    storage.syntax.observe(facts_);
    for (auto const& file : files) {
        if (file.buffer) {
            static_cast<void>(storage.sources.overlay(file.path, file.buffer->text));
        }
    }
}
void Analysis::source_error(fs::path const& path, std::string_view message) {
    auto& storage{compiler_.storage()};
    if (!storage.diagnostics.size()) {
        auto file{storage.sources.add(codegen::generic_path_utf8(path), {})};
        storage.diagnostics.add(DiagnosticSeverity::error, {.file = file}, message);
    }
}
Analysis::Analysis(std::span<WorkspaceFile const> files, Project const& project, TargetId target) {
    overlay(files);
    root_ = project.root;
    std::vector<fs::path> inputs;
    for (auto const& input : project.source_files(target)) {
        inputs.push_back((root_ / input).lexically_normal());
    }
    auto const sources{project.sources(target)};
    auto const fallback{sources.empty() ? root_ : (root_ / sources.front()).lexically_normal()};
    try {
        compiler_.compile(project, target, root_, CompileMode::validate);
    } catch (sexpr::SourceError const& error) {
        source_error(fallback, error.what());
    } catch (MaterialSourceError const& error) {
        source_error(fallback, error.what());
    } catch (std::invalid_argument const& error) {
        source_error(fallback, error.what());
    }
    finish(inputs, fallback);
}
Analysis::Analysis(std::span<WorkspaceFile const> files, fs::path const& entry) {
    overlay(files);
    auto& storage{compiler_.storage()};
    auto& syntax{storage.syntax};
    std::vector<fs::path> inputs;
    bool const manifest{entry.filename() == "project.lispb"};
    try {
        inputs.push_back(entry);
        auto const file{storage.sources.load(entry)};
        auto const roots{sexpr::read_forms(syntax, file)};
        if (manifest && roots.count &&
            try_parse_serialized_form_head(syntax.head(syntax.forms(roots).front())) ==
                FormHead::lispb_project) {
            project_.emplace(parse_project(syntax, file, entry));
            root_ = project_->root;
        } else if (roots.count) {
            auto const head{
                try_parse_serialized_form_head(syntax.head(syntax.forms(roots).front()))};
            if (module_head(head) || head == FormHead::type) {
                codegen::Manifest local{codegen::manifest_schema_version,
                                        codegen::TypeRegistry{storage.registered_types},
                                        storage.declarations,
                                        {}};
                if (head == FormHead::type) {
                    local.types = codegen::parse_type_registry(storage, file).types;
                } else {
                    local.modules = codegen::parse_module_source(storage, file).modules;
                }
                if (auto result{compiler_.analyze_schema(std::move(local))}; !result) {
                    auto const& error{result.error()};
                    auto const name{storage.declarations.type_refs.authored_name[error.reference]};
                    storage.diagnostics.add(
                        DiagnosticSeverity::error, syntax.token_span(name), error.message);
                }
            } else {
                auto kind{TargetKind::none};
                if (head == FormHead::kernel_module) {
                    kind = TargetKind::kernel;
                }
                if (head == FormHead::widget_class || head == FormHead::widget_library) {
                    kind = TargetKind::slate;
                }
                if (head == FormHead::material) {
                    kind = TargetKind::material;
                }
                if (kind != TargetKind::none) {
                    Project local{storage.strings};
                    local.root = entry.parent_path();
                    auto const id{local.append(kind, storage.strings.intern("standalone"))};
                    auto const name{
                        storage.strings.intern(codegen::generic_path_utf8(entry.filename()))};
                    local.set_sources(id, local.append_paths(std::span{&name, 1}));
                    local.set_output(id, PathBase::project, name);
                    compiler_.compile(local, id, local.root, CompileMode::validate);
                }
            }
        }
    } catch (sexpr::SourceError const& error) {
        source_error(entry, error.what());
    } catch (MaterialSourceError const& error) {
        source_error(entry, error.what());
    } catch (std::invalid_argument const& error) {
        source_error(entry, error.what());
    }
    finish(inputs, entry);
}
void Analysis::finish(std::vector<fs::path>& inputs, fs::path const& fallback) {
    auto& storage{compiler_.storage()};
    auto& syntax{storage.syntax};
    std::vector<fs::path> paths(storage.sources.file_count());
    for (auto const& [name, file] : storage.sources.loaded_files()) {
        auto const path{codegen::path_from_utf8(storage.strings.view(name))};
        paths[file.value] = path;
        if (syntax.cached_forms(file)) {
            inputs.push_back(path);
        }
    }
    auto const initial_count{storage.diagnostics.size()};
    for (std::uint32_t i{}; i < initial_count; ++i) {
        auto const file{storage.diagnostics.span(DiagnosticId{i}).file};
        if (file.value < paths.size() && !paths[file.value].empty()) {
            inputs.push_back(paths[file.value]);
        }
    }
    std::ranges::sort(inputs);
    auto const duplicates{std::ranges::unique(inputs)};
    inputs.erase(duplicates.begin(), duplicates.end());

    for (auto const& path : inputs) {
        sexpr::SourceFileId file;
        sexpr::HandleRange roots{};
        try {
            file = storage.sources.load(path);
            if (file.value >= paths.size()) {
                paths.resize(file.value + 1);
            }
            paths[file.value] = path;
            roots = sexpr::read_editor_forms(syntax, file);
        } catch (sexpr::SourceError const&) {
            if (!file.value) {
                continue;
            }
        }
        if (file.value >= document_indices_.size()) {
            document_indices_.resize(file.value + 1);
        }
        document_indices_[file.value] = static_cast<std::uint32_t>(documents_.size() + 1);
        auto& doc{documents_.emplace_back(Document{path, file, roots, {}, {}})};
        for (auto form : syntax.forms(roots)) {
            index(doc, form, {}, {});
        }
    }
    for (auto const& doc : documents_) {
        for (auto const& symbol : doc.symbols) {
            declarations_.push_back({key(syntax.token_span(symbol.name)), symbol.kind});
        }
    }
    std::ranges::sort(declarations_, {}, &DeclarationLocation::start);
    link_types(compiler_.types());
    std::ranges::sort(
        facts_.references, {}, [](auto const& reference) { return key(reference.use); });

    auto const final_count{storage.diagnostics.size()};
    for (std::uint32_t i{}; i < final_count; ++i) {
        DiagnosticId const id{i};
        auto const span{storage.diagnostics.span(id)};
        bool const loaded{span.file.value < paths.size() && !paths[span.file.value].empty()};
        auto path{loaded ? paths[span.file.value] : fallback};
        if (!loaded && span.file.value) {
            auto const origin{storage.sources.filesystem_path(span.file)};
            if (origin.is_absolute()) {
                path = origin;
            }
        }
        diagnostics_.push_back({std::move(path), id});
    }
}
auto Analysis::diagnostic(DiagnosticId id) const -> Json {
    auto const& storage{compiler_.storage()};
    auto const span{storage.diagnostics.span(id)};
    auto const severity{storage.diagnostics.severity(id)};
    return {{"source", "lispb"},
            {"message", storage.diagnostics.message(id)},
            {"severity",
             severity == DiagnosticSeverity::error     ? 1
             : severity == DiagnosticSeverity::warning ? 2
                                                       : 3},
            {"range", range(storage.sources, span)}};
}
auto Analysis::document(sexpr::SourceFileId file) const -> Document const& {
    return documents_[document_indices_[file.value] - 1];
}
auto Analysis::contains(fs::path const& path) const -> bool {
    return std::ranges::find(documents_, path, &Document::path) != documents_.end();
}
auto Analysis::root_head(fs::path const& path) -> std::optional<FormHead> {
    auto& syntax{compiler_.storage().syntax};
    auto const roots{syntax.forms((*std::ranges::find(documents_, path, &Document::path)).roots)};
    return roots.empty() ? std::nullopt
                         : try_parse_serialized_form_head(syntax.head(roots.front()));
}
void Analysis::index(Document& doc,
                     sexpr::FormId form,
                     SymbolId parent_symbol,
                     sexpr::FormId parent_form) {
    auto& syntax{compiler_.storage().syntax};
    auto const children{syntax.children(form)};
    auto const spelling{syntax.head(form)};
    auto const head{try_parse_serialized_form_head(spelling)};
    auto const parent{parent_form.value ? try_parse_serialized_form_head(syntax.head(parent_form))
                                        : std::nullopt};
    auto const kind{symbol_kind(spelling, head, parent)};
    auto const name_index{head == FormHead::parameter && parent == FormHead::material ? 2u : 1u};
    if (kind && children.size() > name_index &&
        syntax.kind(children[name_index]) == sexpr::TokenKind::atom) {
        doc.symbols.push_back({form, children[name_index], parent_symbol, *kind});
        parent_symbol = SymbolId{static_cast<std::uint32_t>(doc.symbols.size())};
    }
    if (head == FormHead::defmacro && children.size() > 2) {
        for (auto parameter : syntax.children(children[2])) {
            if (syntax.kind(parameter) == sexpr::TokenKind::atom) {
                doc.symbols.push_back({parameter, parameter, parent_symbol, Kind::parameter});
            }
        }
    }
    for (auto const child : children) {
        if (syntax.is_list(child)) {
            index(doc, child, parent_symbol, form);
        }
    }
}

void Analysis::link_types(schema::TypeGraph const& graph) {
    auto& storage{compiler_.storage()};
    auto& syntax{storage.syntax};
    struct QualifiedName {
        StringId module;
        StringId name;
        auto operator<=>(QualifiedName const&) const = default;
    };
    struct Definition {
        QualifiedName key;
        Symbol* symbol;
    };
    std::vector<Definition> definitions;
    for (auto& doc : documents_) {
        for (auto& symbol : doc.symbols) {
            auto const parent{doc.parent_form(symbol)};
            auto const head{try_parse_serialized_form_head(syntax.head(symbol.form))};
            auto const name{syntax.identifier(symbol.name)};
            if (head == FormHead::type && !parent.value) {
                definitions.push_back({{{}, name}, &symbol});
            } else if (parent.value &&
                       module_head(try_parse_serialized_form_head(syntax.head(parent)))) {
                definitions.push_back(
                    {{syntax.identifier(syntax.child(parent, 1)), name}, &symbol});
            }
        }
    }
    std::ranges::stable_sort(definitions, {}, &Definition::key);
    std::vector<sexpr::FormId> names(graph.types().size());
    for (auto type : graph.types()) {
        QualifiedName const wanted{graph.module_name(type), graph.name(type)};
        auto const found{std::ranges::lower_bound(definitions, wanted, {}, &Definition::key)};
        if (found != definitions.end() && found->key == wanted) {
            found->symbol->type = type;
            names[type.value] = found->symbol->name;
        }
    }

    for (auto const use : graph.type_uses()) {
        auto const form{storage.declarations.type_refs.authored_name[graph.reference(use)]};
        auto const type{graph.occurrences().type[graph.target(use)]};
        if (form.value && schema::is_valid(type) && names[type.value].value) {
            syntax.bind(form, syntax.token_span(names[type.value]));
        }
    }
}
auto Analysis::outline(Document const& doc, bool hierarchical) -> Json {
    auto& storage{compiler_.storage()};
    Json result = Json::array();
    // Parents precede children. Build child arrays back to front, then restore source order.
    std::vector<Json> children(hierarchical ? doc.symbols.size() : 0);
    for (auto i{doc.symbols.size()}; i > 0; --i) {
        auto const& symbol{doc.symbols[i - 1]};
        auto const selection = range(storage.sources, storage.syntax.span(symbol.name));
        Json item{{"name", storage.syntax.text(symbol.name)},
                  {"kind", std::to_underlying(outline_kind(symbol.kind))}};
        if (hierarchical) {
            item["range"] = range(storage.sources, storage.syntax.span(symbol.form));
            item["selectionRange"] = selection;
            auto& nested{children[i - 1]};
            if (!nested.empty()) {
                std::reverse(nested.begin(), nested.end());
                item["children"] = std::move(nested);
            }
            if (symbol.parent.value) {
                children[symbol.parent.value - 1].push_back(std::move(item));
                continue;
            }
        } else {
            item["location"] = {{"uri", file_uri(doc.path)}, {"range", selection}};
        }
        result.push_back(std::move(item));
    }
    std::reverse(result.begin(), result.end());
    return result;
}
auto Analysis::folds(Document const& doc) -> Json {
    auto& storage{compiler_.storage()};
    Json result = Json::array();
    auto visit = [&](auto&& self, sexpr::FormId form) -> void {
        if (!storage.syntax.is_list(form)) {
            return;
        }
        auto span{storage.syntax.span(form)};
        auto const start{storage.sources.location(span).line - 1};
        span.offset = span.end_offset - 1;
        auto const end{storage.sources.location(span).line - 1};
        if (end > start) {
            result.push_back({{"startLine", start}, {"endLine", end}});
        }
        for (auto child : storage.syntax.children(form)) {
            self(self, child);
        }
    };
    for (auto root : storage.syntax.forms(doc.roots)) {
        visit(visit, root);
    }
    return result;
}

auto Analysis::lookup(Document const& doc, std::span<sexpr::FormId const> context)
    -> std::optional<sexpr::SourceSpan> {
    if (context.empty()) {
        return {};
    }
    auto& syntax{compiler_.storage().syntax};
    auto const token{context.back()};
    auto const span{syntax.token_span(token)};
    auto const link{std::ranges::lower_bound(
        facts_.references, key(span), {}, [](auto const& item) { return key(item.use); })};
    if (link != facts_.references.end() && key(link->use) == key(span) &&
        link->declaration.file.value < document_indices_.size() &&
        document_indices_[link->declaration.file.value] != 0) {
        return link->declaration;
    }
    for (auto const& symbol : doc.symbols) {
        if (symbol.name == token) {
            return span;
        }
    }
    return {};
}

auto Analysis::completion(Document const& doc,
                          std::span<sexpr::FormId const> context,
                          std::uint32_t offset,
                          MarkupKind format) -> Json {
    auto& storage{compiler_.storage()};
    auto& syntax{storage.syntax};
    auto const source{storage.sources.text(doc.file)};
    std::string_view prefix;
    Json replacement;
    if (!context.empty() && !syntax.is_list(context.back())) {
        auto const form{context.back()};
        if (syntax.kind(form) == sexpr::TokenKind::raw_literal ||
            syntax.kind(form) == sexpr::TokenKind::string) {
            return Json::array();
        }
        auto const span{syntax.span(form)};
        prefix = source.substr(span.offset, offset - span.offset);
        replacement = range(storage.sources, span);
        context = context.first(context.size() - 1);
    }
    struct Candidate {
        std::string label;
        sexpr::SourceFileId file;
        SymbolId symbol;
    };
    std::vector<Candidate> candidates;
    auto add = [&](std::string_view name, sexpr::SourceFileId file = {}, SymbolId symbol = {}) {
        if (name.starts_with(prefix)) {
            candidates.emplace_back(std::string{name}, file, symbol);
        }
    };
    auto enumeration = [&]<typename E>(std::string_view leader = {}) {
        for (auto i{static_cast<unsigned>(ioj::enum_min<E>)};
             i <= static_cast<unsigned>(ioj::enum_max<E>);
             ++i) {
            add(std::string{leader} + std::string{ioj::to_serialized_string(static_cast<E>(i))});
        }
    };
    auto const root{context.empty() ? root_head(doc.path)
                                    : try_parse_serialized_form_head(syntax.head(context.front()))};
    auto const target_names{root == FormHead::lispb_project && context.size() > 2 &&
                            try_parse_serialized_form_head(syntax.head(context[1])) ==
                                FormHead::group};
    auto parent{context.empty() ? sexpr::FormId{} : context.back()};
    bool head_position{!parent.value || syntax.children(parent).empty() ||
                       offset <= syntax.span(syntax.child(parent, 0)).end_offset};
    if (target_names) {
        head_position = false;
    }
    if (head_position && context.size() >= 2) {
        parent = context[context.size() - 2];
    }
    auto const spelling{parent.value ? syntax.head(parent) : std::string_view{}};
    auto const head{try_parse_serialized_form_head(spelling)};
    bool have_vocabulary{};
    if (parent.value && !syntax.children(parent).empty()) {
        if (parent.value < facts_.form_vocabulary.size() &&
            facts_.form_vocabulary[parent.value].count) {
            auto const range{facts_.form_vocabulary[parent.value]};
            for (auto id : std::span{facts_.names}.subspan(range.offset, range.count)) {
                auto const name{storage.strings.view(id)};
                if (head_position != name.starts_with(':')) {
                    add(name);
                }
            }
            have_vocabulary = true;
        }
    }
    if (root == FormHead::material) {
        if (head == FormHead::material) {
            enumeration.template operator()<material_synth::MaterialClause>();
        } else if (head_position) {
            enumeration.template operator()<material_synth::MaterialOperator>();
        }
    } else if (root == FormHead::kernel_module) {
        if (head == FormHead::kernel_module) {
            add(to_serialized_string(FormHead::type_set));
            add(to_serialized_string(FormHead::emit));
            enumeration.template operator()<kernel_codegen::detail::OperationKind>();
        } else if (head == FormHead::emit) {
            enumeration.template operator()<kernel_codegen::detail::EmissionField>();
        } else if (head == FormHead::variants) {
            enumeration.template operator()<kernel_codegen::detail::VariantKind>();
        } else if (kernel_codegen::detail::try_parse_serialized_operation_kind(spelling)) {
            enumeration.template operator()<kernel_codegen::detail::OperationField>();
        }
    } else if (root == FormHead::lispb_project) {
        if (head == FormHead::lispb_project && head_position) {
            for (auto i{TargetKind::cpp_schema}; i <= TargetKind::group;
                 i = static_cast<TargetKind>(static_cast<unsigned>(i) + 1)) {
                add(to_serialized_string(i));
            }
        } else if (!have_vocabulary && !head_position) {
            enumeration.template operator()<ProjectProperty>(":");
        }
    } else if (root == FormHead::widget_class || root == FormHead::widget_library ||
               root == FormHead::defmacro) {
        if (head == FormHead::params) {
            enumeration.template operator()<slate_codegen::detail::ParameterKind>();
        } else if (head_position) {
            enumeration.template operator()<slate_codegen::detail::StructuralForm>();
        }
    } else {
        if (module_head(head) && head_position) {
            enumeration.template operator()<codegen::DeclarationKind>();
        }
        if (!have_vocabulary && !head_position && prefix.starts_with(':')) {
            enumeration.template operator()<codegen::SchemaProperty>(":");
        }
    }

    for (auto const& other : documents_) {
        auto const count{other.symbols.size()};
        for (std::uint32_t i{}; i < count; ++i) {
            auto const& symbol{other.symbols[i]};
            SymbolId const id{i + 1};
            auto const parent_form{other.parent_form(symbol)};
            auto const name{syntax.text(symbol.name)};
            auto const name_head{syntax.head(symbol.form)};
            auto const kind{try_parse_serialized_form_head(name_head)};
            if (kind == FormHead::type ||
                (parent_form.value &&
                 module_head(try_parse_serialized_form_head(syntax.head(parent_form))) &&
                 codegen::try_parse_serialized_declaration_kind(name_head))) {
                if (!head_position && !prefix.starts_with(':')) {
                    std::string label{"@"};
                    if (parent_form.value && (context.empty() || context.front() != parent_form)) {
                        label += syntax.text(syntax.child(parent_form, 1));
                        label += '/';
                    }
                    label += name;
                    add(label, other.file, id);
                }
                continue;
            }
            if (kind == FormHead::defmacro && head_position) {
                add(name, other.file, id);
                continue;
            }
            if (other.file != doc.file || head_position ||
                (!target_names && syntax.span(symbol.name).offset > offset)) {
                continue;
            }
            if (target_names) {
                add(name, other.file, id);
                continue;
            }
            if (root == FormHead::kernel_module &&
                ((head == FormHead::types) != (kind == FormHead::type_set))) {
                continue;
            }
            bool const local_scope{!parent_form.value ||
                                   std::ranges::find(context, parent_form) != context.end()};
            if (local_scope && ((symbol.kind == Kind::parameter || symbol.kind == Kind::variable) ||
                                kind == FormHead::type_set || root == FormHead::lispb_project)) {
                add(name, other.file, id);
            }
        }
    }
    Json result = Json::array();
    std::ranges::stable_sort(candidates, [](Candidate const& left, Candidate const& right) {
        if (left.label != right.label) {
            return left.label < right.label;
        }
        return left.symbol.value && !right.symbol.value;
    });
    auto const duplicates{std::ranges::unique(candidates, {}, &Candidate::label)};
    candidates.erase(duplicates.begin(), duplicates.end());
    for (auto& candidate : candidates) {
        Json item;
        if (!replacement.is_null()) {
            item["textEdit"] = {{"range", replacement}, {"newText", candidate.label}};
        }
        item["label"] = std::move(candidate.label);
        auto kind{CompletionKind::keyword};
        if (candidate.symbol.value) {
            auto const& owner{document(candidate.file)};
            auto const& symbol{owner.symbols[candidate.symbol.value - 1]};
            kind = completion_kind(symbol.kind);
            item["detail"] = signature(symbol);
            auto details{declaration_details(owner, symbol, format)};
            if (!details.empty()) {
                if (format == MarkupKind::markdown) {
                    item["documentation"] = {{"kind", to_serialized_string(format)},
                                             {"value", std::move(details)}};
                } else {
                    item["documentation"] = std::move(details);
                }
            }
        }
        item["kind"] = std::to_underlying(kind);
        result.push_back(std::move(item));
    }
    return result;
}

auto Analysis::query(fs::path const& path,
                     Method method,
                     Json const& params,
                     ClientCapabilities capabilities) -> Json {
    auto& storage{compiler_.storage()};
    auto& syntax{storage.syntax};
    auto& doc{(*std::ranges::find(documents_, path, &Document::path))};
    if (method == Method::tokens || method == Method::token_range) {
        return highlighting(doc, params);
    }
    if (method == Method::symbols) {
        return outline(doc, capabilities.hierarchical_symbols);
    }
    if (method == Method::folding) {
        return folds(doc);
    }
    auto const offset{offset_at(storage.sources, doc.file, params.at("position"))};
    auto const completing{method == Method::completion};
    std::vector<sexpr::FormId> context;
    auto forms{syntax.forms(doc.roots)};
    while (!forms.empty()) {
        auto const after{std::ranges::upper_bound(
            forms, offset, {}, [&](auto form) { return syntax.span(form).offset; })};
        if (after == forms.begin()) {
            break;
        }
        auto const form{*std::prev(after)};
        auto const span{syntax.span(form)};
        if (offset > span.end_offset || (!completing && offset == span.end_offset)) {
            break;
        }
        context.push_back(form);
        forms = syntax.children(form);
    }
    if (completing) {
        return completion(doc, context, offset, capabilities.completion_format);
    }
    if (context.empty()) {
        return nullptr;
    }
    auto const token{context.back()};
    if (method == Method::definition && syntax.kind(token) == sexpr::TokenKind::string &&
        project_) {
        auto const input{(root_ / codegen::path_from_utf8(syntax.text(token))).lexically_normal()};
        if (input.extension() == ".lispb" && fs::is_regular_file(input)) {
            Json zero{{"line", 0}, {"character", 0}};
            return {{"uri", file_uri(input)}, {"range", {{"start", zero}, {"end", zero}}}};
        }
    }
    auto const target{lookup(doc, context)};
    if (!target) {
        return nullptr;
    }
    auto const& destination{document(target->file)};
    if (method == Method::definition) {
        return {{"uri", file_uri(destination.path)}, {"range", range(storage.sources, *target)}};
    }
    return hover(destination, *target, token, capabilities.hover_format);
}
}
