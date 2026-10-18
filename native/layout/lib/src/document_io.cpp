#include "document_io.hpp"

#include "document_file_updates.hpp"
#include <ioj/layout/document.hpp>
#include <ioj/layout/document_state.hpp>
#include <ioj/layout/editor_session.hpp>

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
    for (auto const module : state.modules) {
        if (state.source_files.at(module) == file) {
            modules.push_back(module);
        }
    }
    auto const origin{state.io.render_origins.find(file)};
    return lispb::schema::render_source_file(
        state.storage(),
        state.comments,
        modules,
        origin == state.io.render_origins.end() ? file : origin->second);
}
auto path_key(DocumentState const& state, SourceFileId file) -> lispb::IdentifierId {
    return state.storage().strings.find(state.storage().sources.path(file));
}
void render_sources(DocumentState const& state) {
    if (state.io.rendered_revision == state.revision) {
        return;
    }
    auto& strings{state.storage().strings};
    for (auto const file : state.source_order) {
        auto const text{render(state, file)};
        auto const saved{state.io.saved_sources.find(path_key(state, file))};
        auto const previous{state.io.rendered_sources.find(file)};
        if (saved != state.io.saved_sources.end() && strings.view(saved->second) == text) {
            state.io.rendered_sources[file] = saved->second;
        } else if (previous != state.io.rendered_sources.end() &&
                   strings.view(previous->second) == text) {
            continue;
        } else if (state.storage().sources.text(file) == text) {
            state.io.rendered_sources[file] = state.storage().sources.text_id(file);
        } else {
            state.io.rendered_sources[file] = strings.retain(text);
        }
    }
    state.io.rendered_revision = state.revision;
}
void append_source(DocumentState& state, SourceFileId file) {
    auto const parsed{lispb::codegen::parse_module_source(state.storage(), file)};
    state.source_order.push_back(file);
    state.io.source_origins.emplace(file, file);
    for (auto const module : parsed.modules) {
        state.modules.push_back(module);
        state.source_files.emplace(module, file);
        if (module.kind() == lispb::codegen::ModuleKind::normal) {
            auto const& store{state.storage().declarations};
            for (auto const declaration : store.items(
                     store.normal_modules.declarations[store.normal_modules.index(module)])) {
                state.register_declaration(declaration);
            }
        }
    }
    state.comments.capture(state.storage(), parsed.modules, file);
}
void finish_load(DocumentState& state) {
    state.types = lispb::schema::resolve_type_graph(state.manifest());
    state.baseline = state.modules;
    state.baseline_sources = state.source_order;
    render_sources(state);
    for (auto const file : state.source_order) {
        auto const path{path_key(state, file)};
        state.io.saved_sources.emplace(path, state.io.rendered_sources.at(file));
        state.io.persisted_sources.emplace(path, state.storage().sources.text_id(file));
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
    auto& syntax{state.storage().syntax};
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
    std::vector<lispb::IdentifierId> paths;
    for (auto const file : state.source_order) {
        paths.push_back(path_key(state, file));
    }
    if (paths == state.io.saved_source_order) {
        return std::string{state.storage().strings.view(state.io.saved_project_text)};
    }
    auto const span{state.io.project_sources_span};
    auto const original{state.storage().sources.text(state.io.project_source)};
    auto const newline{original.contains("\r\n") ? "\r\n" : "\n"};
    std::string list{"("};
    // Preserve comments from the source list when rewriting its paths.
    for (auto const comment : state.storage().syntax.comments()) {
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
        list += quote(state.storage().sources.path(file));
    }
    list += ')';
    std::string result{original};
    result.replace(span.offset, span.end_offset - span.offset, list);
    return result;
}
}

auto Document::project_path() const -> std::filesystem::path const& {
    return state_.project_path;
}
auto Document::target_name() const -> std::string_view {
    return state_.target ? state_.project->target_name(*state_.target) : std::string_view{};
}
auto Document::sources() const -> std::vector<DocumentSource> {
    auto& state{state_};
    document_io::render_sources(state);
    std::vector<DocumentSource> result;
    for (auto const file : state.source_order) {
        result.push_back({state.storage().sources.path(file),
                          state.storage().strings.view(state.io.rendered_sources.at(file))});
    }
    return result;
}
auto Document::declaration_source(std::string_view module_name, std::string_view name) const
    -> std::optional<std::string> {
    auto const& store{state_.storage().declarations};
    for (auto const module : state_.modules) {
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
    auto const& state{state_};
    if (state.source_order.size() != state.io.saved_sources.size()) {
        return true;
    }
    auto const count{state.source_order.size()};
    for (std::size_t index{}; index < count; ++index) {
        if (document_io::path_key(state, state.source_order[index]) !=
            state.io.saved_source_order[index]) {
            return true;
        }
    }
    document_io::render_sources(state);
    for (auto const file : state.source_order) {
        auto const saved{state.io.saved_sources.find(document_io::path_key(state, file))};
        if (saved == state.io.saved_sources.end() ||
            saved->second != state.io.rendered_sources.at(file)) {
            return true;
        }
    }
    return false;
}
auto Document::save() -> bool {
    auto& state{state_};
    auto& strings{state.storage().strings};
    state.session->begin_operation();
    if (!state.project) {
        state.session->record_error("Document has no project destination");
        return false;
    }
    try {
        document_io::render_sources(state);
        auto const project_source{document_io::project_text(state)};
        if (document_io::read(state.project_path) != strings.view(state.io.saved_project_text)) {
            throw std::invalid_argument{"Project changed on disk; reload before saving"};
        }
        std::set<lispb::IdentifierId> current_paths;
        for (auto const file : state.source_order) {
            auto const key{document_io::path_key(state, file)};
            current_paths.insert(key);
            auto const path{state.project->root / strings.view(key)};
            auto const saved{state.io.persisted_sources.find(key)};
            if (saved != state.io.persisted_sources.end()) {
                if (document_io::read(path) != strings.view(saved->second)) {
                    throw std::invalid_argument{"Source changed on disk: " + path.string()};
                }
            } else if (std::filesystem::exists(path)) {
                throw std::invalid_argument{"Source destination already exists: " + path.string()};
            }
        }
        std::vector<lispb::IdentifierId> removed_paths;
        for (auto const file : state.source_order) {
            auto const saved{state.io.saved_source_paths.find(state.io.source_origins.at(file))};
            if (saved == state.io.saved_source_paths.end() ||
                saved->second == document_io::path_key(state, file)) {
                continue;
            }
            auto const expected{state.io.persisted_sources.find(saved->second)};
            if (expected != state.io.persisted_sources.end() &&
                document_io::read(state.project->root / strings.view(saved->second)) !=
                    strings.view(expected->second)) {
                throw std::invalid_argument{"Renamed source changed on disk: " +
                                            std::string{strings.view(saved->second)}};
            }
            if (!current_paths.contains(saved->second)) {
                removed_paths.push_back(saved->second);
            }
        }
        DocumentFileUpdates updates;
        for (auto const file : state.source_order) {
            auto const path{document_io::path_key(state, file)};
            auto const text{state.io.rendered_sources.at(file)};
            auto const saved{state.io.saved_sources.find(path)};
            if (saved == state.io.saved_sources.end() || saved->second != text) {
                updates.write(state.project->root / strings.view(path), strings.view(text));
            }
        }
        if (project_source != strings.view(state.io.saved_project_text)) {
            updates.write(state.project_path, project_source);
        }
        for (auto const path : removed_paths) {
            updates.erase(state.project->root / strings.view(path));
        }
        updates.publish();

        if (project_source != strings.view(state.io.saved_project_text)) {
            state.io.saved_project_text = strings.retain(project_source);
        }
        for (auto const path : removed_paths) {
            state.io.persisted_sources.erase(path);
        }
        for (auto const file : state.source_order) {
            auto const path{document_io::path_key(state, file)};
            auto const text{state.io.rendered_sources.at(file)};
            auto const saved{state.io.saved_sources.find(path)};
            if (saved == state.io.saved_sources.end() || saved->second != text) {
                state.io.persisted_sources[path] = text;
            }
            state.io.saved_source_paths[state.io.source_origins.at(file)] = path;
        }
        state.io.saved_sources.clear();
        state.io.saved_source_order.clear();
        for (auto const file : state.source_order) {
            auto const path{document_io::path_key(state, file)};
            state.io.saved_sources.emplace(path, state.io.rendered_sources.at(file));
            state.io.saved_source_order.push_back(path);
        }
        return true;
    } catch (std::invalid_argument const& error) {
        state.session->record_error(error.what());
    } catch (std::runtime_error const& error) {
        state.session->record_error(error.what());
    }
    return false;
}
auto Document::save_as(std::filesystem::path const& project_path, std::string_view target_value)
    -> bool {
    auto& state{state_};
    auto& strings{state.storage().strings};
    std::string const target_name{target_value};
    state.session->begin_operation();
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
        document_io::render_sources(state);
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
        for (auto const file : state.source_order) {
            auto const source_path{state.storage().sources.path(file)};
            auto const relative{std::filesystem::path{source_path}.lexically_normal()};
            if (relative.has_root_path() ||
                std::ranges::find(relative, std::filesystem::path{".."}) != relative.end()) {
                throw std::invalid_argument{
                    "Save-as sources must stay within the destination project"};
            }
            auto const path{root / source_path};
            if (!destinations.insert(lispb::codegen::output_path_key(path)).second) {
                throw std::invalid_argument{
                    "Source destination collides with another project file: " + path.string()};
            }
            if (std::filesystem::exists(path)) {
                throw std::invalid_argument{"Source destination already exists: " + path.string()};
            }
            project_text += document_io::quote(source_path) + ' ';
        }
        project_text += ")\n    :output-root (project-path \"generated\")))\n";
        auto const file{state.storage().sources.add(destination.generic_string(), project_text)};
        auto project{lispb::parse_project(state.storage().syntax, file, destination)};
        auto const target{project.find_target(target_name)};
        auto const span{document_io::sources_span(state, file, target_name)};

        DocumentFileUpdates updates;
        for (auto const source : state.source_order) {
            updates.write(root / state.storage().sources.path(source),
                          strings.view(state.io.rendered_sources.at(source)));
        }
        if (!state.project) {
            updates.write(registry, strings.view(state.io.registry_text));
        }
        updates.write(destination, project_text);
        updates.publish();
        state.project_path = destination;
        state.project = std::move(project);
        state.target = target;
        state.io.project_source = file;
        state.io.project_sources_span = span;
        state.io.saved_project_text = state.storage().sources.text_id(file);
        state.io.saved_sources.clear();
        state.io.persisted_sources.clear();
        state.io.saved_source_paths.clear();
        state.io.saved_source_order.clear();
        for (auto const source : state.source_order) {
            auto const path{document_io::path_key(state, source)};
            auto const text{state.io.rendered_sources.at(source)};
            state.io.saved_sources.emplace(path, text);
            state.io.persisted_sources.emplace(path, text);
            state.io.saved_source_order.push_back(path);
        }
        for (auto const source : state.source_order) {
            state.io.saved_source_paths.emplace(state.io.source_origins.at(source),
                                                document_io::path_key(state, source));
        }
        return true;
    } catch (lispb::codegen::sexpr::SourceError const&) {
    } catch (std::invalid_argument const& error) {
        state.session->record_error(error.what());
    } catch (std::runtime_error const& error) {
        state.session->record_error(error.what());
    }
    return false;
}
}
