#include "document_file_updates.hpp"
#include "document_state.hpp"
#include <ioj/layout/document.hpp>

#include <codegen/path_utils.h>
#include <codegen/schema/declaration_schema.h>
#include <codegen/sexpr/fields.h>
#include <codegen/sexpr/parser.h>
#include <codegen/sexpr/reader.h>
#include <codegen/sexpr/source_error.h>
#include <codegen/source_loader.h>
#include <lispb/project_loader.h>
#include <lispb/schema/source_renderer.h>

#include <algorithm>
#include <cassert>
#include <cctype>
#include <fstream>
#include <iterator>
#include <set>
#include <stdexcept>

namespace ioj::layout {
namespace document_io {
using lispb::codegen::sexpr::SourceFileId;

auto read(std::filesystem::path const& path) -> std::string {
    std::ifstream input{path, std::ios::binary};
    if (!input) {
        throw std::runtime_error{"Cannot read source: " + path.string()};
    }
    return {std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
}
auto render(DocumentState const& state, SourceFileId file) -> std::string {
    std::vector<lispb::codegen::ModuleHandle> modules;
    for (auto const module : state.manifest.modules) {
        if (state.source_files.at(module) == file) {
            modules.push_back(module);
        }
    }
    auto const origin{state.io.render_origins.find(file)};
    return lispb::schema::render_source_file(
        state.storage,
        state.comments,
        modules,
        origin == state.io.render_origins.end() ? file : origin->second);
}
void append_source(DocumentState& state, SourceFileId file) {
    auto const parsed{lispb::codegen::parse_module_source(state.storage, file)};
    state.source_order.push_back(file);
    state.io.source_origins.emplace(file, file);
    for (auto const module : parsed.modules) {
        state.manifest.modules.push_back(module);
        state.source_files.emplace(module, file);
        if (module.kind() == lispb::codegen::ModuleKind::normal) {
            auto const& store{state.storage.declarations};
            for (auto const declaration : store.items(
                     store.normal_modules.declarations[store.normal_modules.index(module)])) {
                state.register_declaration(declaration);
            }
        }
    }
    state.comments.capture(state.storage, parsed.modules);
}
void finish_load(DocumentState& state) {
    state.types = lispb::schema::resolve_type_graph(state.manifest);
    state.baseline = state.manifest.modules;
    state.baseline_sources = state.source_order;
    for (auto const file : state.source_order) {
        auto const path{std::string{state.storage.sources.path(file)}};
        state.io.saved_sources.emplace(path, render(state, file));
        state.io.persisted_sources.emplace(path, state.storage.sources.text(file));
        state.io.saved_source_paths.emplace(file, path);
        state.io.saved_source_order.push_back(path);
    }
    state.valid = true;
}
auto quote(std::string_view value) -> std::string {
    std::string result{"\""};
    for (auto const character : value) {
        if (character == '\\' || character == '"') {
            result += '\\';
        }
        result += character;
    }
    return result + '"';
}
auto sources_span(DocumentState& state, SourceFileId file, std::string_view target_name)
    -> lispb::codegen::sexpr::SourceSpan {
    auto& syntax{state.storage.syntax};
    lispb::codegen::sexpr::Parser parser{syntax};
    auto const forms{lispb::codegen::sexpr::read_forms(syntax, file)};
    lispb::codegen::sexpr::Fields const project_fields{
        parser, syntax.forms(forms).front(), "lispb-project", 0};
    for (auto const declaration : project_fields.declarations()) {
        if (syntax.head(declaration) == "cpp-schema") {
            lispb::codegen::sexpr::Fields const fields{parser, declaration, "cpp-schema", 1};
            if (parser.text(fields.positional(0), "target name") == target_name) {
                return syntax.span(fields.required("sources"));
            }
        }
    }
    assert(false && "Selected target has a source list");
    return {};
}
auto project_text(DocumentState const& state) -> std::string {
    std::vector<std::string> paths;
    for (auto const file : state.source_order) {
        paths.emplace_back(state.storage.sources.path(file));
    }
    if (paths == state.io.saved_source_order) {
        return state.io.saved_project_text;
    }
    auto const span{state.io.project_sources_span};
    auto const original{state.storage.sources.text(state.io.project_source)};
    auto const newline{original.contains("\r\n") ? "\r\n" : "\n"};
    std::string list{"("};
    // Preserve comments from the source list when rewriting its paths.
    for (auto const comment : state.storage.syntax.comments()) {
        if (comment.file == span.file && comment.offset >= span.offset &&
            comment.end_offset <= span.end_offset) {
            list += newline;
            list += original.substr(comment.offset, comment.end_offset - comment.offset);
            list += newline;
        }
    }
    for (auto const file : state.source_order) {
        if (list.back() != '(' && list.back() != '\n') {
            list += ' ';
        }
        list += quote(state.storage.sources.path(file));
    }
    list += ')';
    std::string result{original};
    result.replace(span.offset, span.end_offset - span.offset, list);
    return result;
}
}

auto Document::from_source(std::string_view source, std::string_view registry) -> Document {
    DocumentSource const input{"document.lispb", source};
    return from_sources(std::span{&input, 1}, registry);
}
auto Document::from_sources(std::span<DocumentSource const> sources, std::string_view registry)
    -> Document {
    Document document;
    auto& state{*document.state_};
    try {
        state.io.registry_text = registry;
        if (!registry.empty()) {
            auto const file{state.storage.sources.add("document-types.lispb", registry)};
            state.manifest.types = lispb::codegen::parse_type_registry(state.storage, file).types;
        }
        for (auto const& input : sources) {
            auto const path{std::filesystem::path{input.path}.lexically_normal()};
            if (path.empty() || path == "." || path.has_root_path() ||
                std::ranges::find(path, std::filesystem::path{".."}) != path.end()) {
                throw std::invalid_argument{
                    "Source path must be relative and stay within the project"};
            }
            auto const name{path.generic_string()};
            if (std::ranges::any_of(state.source_order, [&](auto file) {
                    return state.storage.sources.path(file) == name;
                })) {
                throw std::invalid_argument{"Duplicate source path"};
            }
            document_io::append_source(state, state.storage.sources.add(name, input.text));
        }
        document_io::finish_load(state);
    } catch (lispb::codegen::sexpr::SourceError const&) {
    } catch (std::runtime_error const& error) {
        state.error(error.what());
    } catch (std::invalid_argument const& error) {
        state.error(error.what());
    }
    return document;
}
auto Document::load(std::filesystem::path const& project_path, std::string_view target_name)
    -> Document {
    Document document;
    auto& state{*document.state_};
    try {
        state.project_path = std::filesystem::absolute(project_path).lexically_normal();
        state.project.emplace(lispb::load_project(state.storage.syntax, state.project_path));
        state.target = state.project->find_target(target_name);
        if (!state.target || state.target->kind() != lispb::TargetKind::cpp_schema) {
            throw std::invalid_argument{"Expected an existing C++ schema target"};
        }
        state.io.project_source = state.storage.sources.load(state.project_path);
        state.io.saved_project_text = state.storage.sources.text(state.io.project_source);
        state.io.project_sources_span =
            document_io::sources_span(state, state.io.project_source, target_name);
        state.manifest.types =
            lispb::codegen::load_type_registry(
                state.storage, state.project->root / state.project->types(*state.target))
                .types;
        for (auto const& source : state.project->sources(*state.target)) {
            auto const file{
                state.storage.sources.load(state.project->root / source, source.generic_string())};
            document_io::append_source(state, file);
        }
        document_io::finish_load(state);
    } catch (lispb::codegen::sexpr::SourceError const&) {
    } catch (std::invalid_argument const& error) {
        state.error(error.what());
    } catch (std::runtime_error const& error) {
        state.error(error.what());
    }
    return document;
}
auto Document::project_path() const -> std::filesystem::path const& {
    return state_->project_path;
}
auto Document::target_name() const -> std::string_view {
    return state_->target ? state_->project->target_name(*state_->target) : std::string_view{};
}
auto Document::sources() const -> std::vector<DocumentSource> {
    auto& state{*state_};
    std::vector<DocumentSource> result;
    for (auto const file : state.source_order) {
        auto& text{state.io.rendered_sources[file]};
        text = document_io::render(state, file);
        result.push_back({state.storage.sources.path(file), text});
    }
    return result;
}
auto Document::declaration_source(std::string_view module_name, std::string_view name) const
    -> std::optional<std::string> {
    auto const& store{state_->storage.declarations};
    for (auto const module : state_->manifest.modules) {
        auto const settings{store.module_settings(module)};
        if (module.kind() != lispb::codegen::ModuleKind::normal ||
            store.strings().view(
                store.module_settings_store.name[store.module_settings_store.index(settings)]) !=
                module_name) {
            continue;
        }
        for (auto const declaration :
             store.items(store.normal_modules.declarations[store.normal_modules.index(module)])) {
            if (store.strings().view(lispb::codegen::declaration_name(store, declaration)) ==
                name) {
                return lispb::schema::render_declaration_source(store, declaration);
            }
        }
    }
    return std::nullopt;
}
auto Document::dirty() const -> bool {
    auto const& state{*state_};
    if (state.source_order.size() != state.io.saved_sources.size()) {
        return true;
    }
    auto const count{state.source_order.size()};
    for (std::size_t index{}; index < count; ++index) {
        if (state.storage.sources.path(state.source_order[index]) !=
            state.io.saved_source_order[index]) {
            return true;
        }
    }
    for (auto const source : sources()) {
        auto const saved{state.io.saved_sources.find(std::string{source.path})};
        if (saved == state.io.saved_sources.end() || saved->second != source.text) {
            return true;
        }
    }
    return false;
}
auto Document::save() -> bool {
    auto& state{*state_};
    state.diagnostic_begin = state.storage.diagnostics.size();
    if (!state.project) {
        state.error("Document has no project destination");
        return false;
    }
    try {
        auto const rendered{sources()};
        auto const project_source{document_io::project_text(state)};
        if (document_io::read(state.project_path) != state.io.saved_project_text) {
            throw std::invalid_argument{"Project changed on disk; reload before saving"};
        }
        // Check every destination before publishing any files.
        for (auto const& source : rendered) {
            auto const path{state.project->root / source.path};
            std::filesystem::create_directories(path.parent_path());
            auto const saved{state.io.persisted_sources.find(std::string{source.path})};
            if (saved != state.io.persisted_sources.end()) {
                if (document_io::read(path) != saved->second) {
                    throw std::invalid_argument{"Source changed on disk: " + path.string()};
                }
            } else if (std::filesystem::exists(path)) {
                throw std::invalid_argument{"Source destination already exists: " + path.string()};
            }
        }
        for (auto const file : state.source_order) {
            auto const saved{state.io.saved_source_paths.find(state.io.source_origins.at(file))};
            if (saved != state.io.saved_source_paths.end() &&
                saved->second != state.storage.sources.path(file)) {
                auto const expected{state.io.persisted_sources.find(saved->second)};
                if (expected != state.io.persisted_sources.end() &&
                    document_io::read(state.project->root / saved->second) != expected->second) {
                    throw std::invalid_argument{"Renamed source changed on disk: " + saved->second};
                }
            }
        }
        DocumentFileUpdates updates;
        for (auto const& source : rendered) {
            auto const saved{state.io.saved_sources.find(std::string{source.path})};
            if (saved == state.io.saved_sources.end() || saved->second != source.text) {
                updates.write(state.project->root / source.path, source.text);
            }
        }
        if (project_source != state.io.saved_project_text) {
            updates.write(state.project_path, project_source);
        }
        for (auto const file : state.source_order) {
            auto const origin{state.io.source_origins.at(file)};
            auto const path{std::string{state.storage.sources.path(file)}};
            auto const saved{state.io.saved_source_paths.find(origin)};
            if (saved != state.io.saved_source_paths.end() && saved->second != path &&
                !std::ranges::any_of(
                    rendered, [&](auto const& current) { return current.path == saved->second; })) {
                updates.erase(state.project->root / saved->second);
            }
        }
        updates.publish();
        state.io.saved_project_text = project_source;
        for (auto const& source : rendered) {
            auto const saved{state.io.saved_sources.find(std::string{source.path})};
            if (saved == state.io.saved_sources.end() || saved->second != source.text) {
                state.io.persisted_sources[std::string{source.path}] = source.text;
            }
        }
        for (auto const file : state.source_order) {
            auto const origin{state.io.source_origins.at(file)};
            auto const path{std::string{state.storage.sources.path(file)}};
            auto const saved{state.io.saved_source_paths.find(origin)};
            if (saved != state.io.saved_source_paths.end() && saved->second != path &&
                !std::ranges::any_of(
                    rendered, [&](auto const& current) { return current.path == saved->second; })) {
                state.io.persisted_sources.erase(saved->second);
            }
            state.io.saved_source_paths[origin] = path;
        }
        state.io.saved_sources.clear();
        state.io.saved_source_order.clear();
        for (auto const& source : rendered) {
            state.io.saved_sources.emplace(source.path, source.text);
            state.io.saved_source_order.emplace_back(source.path);
        }
        return true;
    } catch (std::invalid_argument const& error) {
        state.error(error.what());
    } catch (std::runtime_error const& error) {
        state.error(error.what());
    }
    return false;
}
auto Document::save_as(std::filesystem::path const& project_path, std::string_view target_value)
    -> bool {
    auto& state{*state_};
    std::string const target_name{target_value};
    state.diagnostic_begin = state.storage.diagnostics.size();
    try {
        auto const destination{std::filesystem::absolute(project_path).lexically_normal()};
        if (destination == state.project_path) {
            return save();
        }
        if (target_name.empty() || !std::ranges::all_of(target_name, [](unsigned char character) {
                return std::isalnum(character) != 0 || character == '_' || character == '-';
            })) {
            throw std::invalid_argument{
                "Expected a target name containing letters, digits, '_' or '-'"};
        }
        if (std::filesystem::exists(destination)) {
            throw std::invalid_argument{"Project destination already exists: " +
                                        destination.string()};
        }
        std::vector<std::string> paths;
        std::vector<std::string> texts;
        for (auto const source : sources()) {
            paths.emplace_back(source.path);
            texts.emplace_back(source.text);
        }
        // Project parsing grows the compiler's string store; retain source views independently.
        std::vector<DocumentSource> rendered;
        auto const count{paths.size()};
        for (std::size_t index{}; index < count; ++index) {
            rendered.push_back({paths[index], texts[index]});
        }
        auto const root{destination.parent_path()};
        auto const registry{state.project
                                ? state.project->root / state.project->types(*state.target)
                                : root / "types.lispb"};
        std::set<std::string> destinations{lispb::codegen::output_path_key(destination)};
        if (!destinations.insert(lispb::codegen::output_path_key(registry)).second) {
            throw std::invalid_argument{"Project destination collides with the type registry"};
        }
        if (!state.project && std::filesystem::exists(registry)) {
            throw std::invalid_argument{"Type registry destination already exists: " +
                                        registry.string()};
        }
        std::string project_text{
            "(lispb-project :language-version 1 :project-root \".\"\n  (cpp-schema "};
        project_text += target_name;
        project_text +=
            "\n    :types " + document_io::quote(registry.generic_string()) + "\n    :sources (";
        for (auto const& source : rendered) {
            auto const relative{std::filesystem::path{source.path}.lexically_normal()};
            if (relative.has_root_path() ||
                std::ranges::find(relative, std::filesystem::path{".."}) != relative.end()) {
                throw std::invalid_argument{
                    "Save-as sources must stay within the destination project"};
            }
            auto const path{root / source.path};
            if (!destinations.insert(lispb::codegen::output_path_key(path)).second) {
                throw std::invalid_argument{
                    "Source destination collides with another project file: " + path.string()};
            }
            if (std::filesystem::exists(path)) {
                throw std::invalid_argument{"Source destination already exists: " + path.string()};
            }
            project_text += document_io::quote(source.path) + ' ';
        }
        project_text += ")\n    :output-root (project-path \"generated\")))\n";
        auto const file{state.storage.sources.overlay(destination, project_text)};
        auto project{lispb::load_project(state.storage.syntax, destination)};
        auto const target{project.find_target(target_name)};
        auto const span{document_io::sources_span(state, file, target_name)};

        DocumentFileUpdates updates;
        for (auto const& source : rendered) {
            updates.write(root / source.path, source.text);
        }
        if (!state.project) {
            updates.write(registry, state.io.registry_text);
        }
        updates.write(destination, project_text);
        updates.publish();
        state.project_path = destination;
        state.project = std::move(project);
        state.target = target;
        state.io.project_source = file;
        state.io.project_sources_span = span;
        state.io.saved_project_text = project_text;
        state.io.saved_sources.clear();
        state.io.persisted_sources.clear();
        state.io.saved_source_paths.clear();
        state.io.saved_source_order.clear();
        for (auto const& source : rendered) {
            state.io.saved_sources.emplace(source.path, source.text);
            state.io.persisted_sources.emplace(source.path, source.text);
            state.io.saved_source_order.emplace_back(source.path);
        }
        for (auto const source : state.source_order) {
            state.io.saved_source_paths.emplace(state.io.source_origins.at(source),
                                                state.storage.sources.path(source));
        }
        return true;
    } catch (lispb::codegen::sexpr::SourceError const&) {
    } catch (std::invalid_argument const& error) {
        state.error(error.what());
    } catch (std::runtime_error const& error) {
        state.error(error.what());
    }
    return false;
}
}
