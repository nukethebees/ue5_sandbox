#include "document_io.hpp"

#include "document_file_updates.hpp"
#include <ioj/layout/document.hpp>
#include <ioj/layout/document_state.hpp>
#include <ioj/layout/editor_session.hpp>
#include <ioj/layout/rendered_source.hpp>

#include <codegen/path_utils.h>
#include <codegen/schema/declaration_schema.h>
#include <codegen/sexpr/fields.h>
#include <codegen/sexpr/parser.h>
#include <codegen/sexpr/reader.h>
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
using lispb::codegen::output_path_key;

namespace document_io {
using lispb::codegen::sexpr::SourceFileId;

auto read(std::filesystem::path const& path) -> std::string {
    std::ifstream input{path, std::ios::binary};
    if (!input) {
        throw std::runtime_error{"Cannot read source: " + path.string()};
    }
    return {std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
}
auto relative_path(std::string_view path) -> std::string {
    auto const value{std::filesystem::path{path}.lexically_normal()};
    if (value.empty() || value == "." || value.has_root_path() ||
        std::ranges::find(value, std::filesystem::path{".."}) != value.end()) {
        throw std::invalid_argument{"Source path must be relative and stay within the project"};
    }
    return value.generic_string();
}
auto render(DocumentState const& state, SourceFileId file) -> std::string {
    std::vector<lispb::codegen::ModuleHandle> modules;
    for (auto const module : state.modules) {
        if (state.source_files.at(module) == file) {
            modules.push_back(module);
        }
    }
    auto const origin{state.render_origins.find(file)};
    return lispb::schema::render_source_file(state.storage(),
                                             state.comments,
                                             modules,
                                             origin == state.render_origins.end() ? file
                                                                                  : origin->second);
}
void freeze_registry(DocumentState& state, SourceFileId root) {
    auto& storage{state.storage()};
    auto& strings{state.session->command_strings()};
    auto& frozen{state.io.registry};
    std::vector<SourceFileId> parents;
    std::vector<SourceFileId> targets;
    std::vector<std::string> includes;
    auto const parsed{lispb::codegen::parse_type_registry(
        storage, root, [&](SourceFileId parent, std::string_view include) {
            std::string const spelling{include};
            auto const path{std::filesystem::weakly_canonical(
                storage.sources.filesystem_path(parent).parent_path() / spelling)};
            auto const target{storage.sources.load(path)};
            parents.push_back(parent);
            targets.push_back(target);
            includes.push_back(spelling);
            return target;
        })};
    std::map<SourceFileId, std::size_t> rows;
    for (auto const file : parsed.sources) {
        rows.emplace(file, frozen.sources.paths.size());
        frozen.sources.paths.push_back(strings.intern(storage.sources.path(file)));
        frozen.sources.texts.push_back(strings.intern_retained(storage.sources.text(file)));
    }
    for (std::size_t index{}; index < parents.size(); ++index) {
        frozen.parents.push_back(rows.at(parents[index]));
        frozen.targets.push_back(rows.at(targets[index]));
        frozen.includes.push_back(strings.intern(includes[index]));
        auto const ordinal{static_cast<std::size_t>(
            std::count(parents.begin(),
                       parents.begin() + static_cast<std::ptrdiff_t>(index),
                       parents[index]))};
        auto const span{storage.sources.include_spans(parents[index])[ordinal]};
        frozen.begins.push_back(span.offset);
        frozen.ends.push_back(span.end_offset);
    }
}
auto sources_span(lispb::CompilerStorage& storage, SourceFileId file, std::string_view target)
    -> lispb::codegen::sexpr::SourceSpan {
    auto& syntax{storage.syntax};
    lispb::codegen::sexpr::Parser parser{syntax};
    auto const forms{lispb::codegen::sexpr::read_forms(syntax, file)};
    lispb::codegen::sexpr::Fields const project_fields{
        parser, syntax.forms(forms).front(), "lispb-project", 0};
    for (auto const declaration : project_fields.declarations()) {
        if (syntax.head(declaration) == "cpp-schema") {
            lispb::codegen::sexpr::Fields const fields{parser, declaration, "cpp-schema", 1};
            if (parser.text(fields.positional(0), "target name") == target) {
                return syntax.span(fields.required("sources"));
            }
        }
    }
    assert(false && "Selected target has a source list");
    return {};
}
void set_project(DocumentState& state,
                 lispb::CompilerStorage& storage,
                 SourceFileId file,
                 std::filesystem::path const& path,
                 std::filesystem::path const& root,
                 std::filesystem::path const& registry,
                 std::string_view target) {
    auto const span{sources_span(storage, file, target)};
    auto const original{storage.sources.text(file)};
    std::string comments;
    auto const newline{original.contains("\r\n") ? "\r\n" : "\n"};
    for (auto const comment : storage.syntax.comments()) {
        if (comment.file == file && comment.offset >= span.offset &&
            comment.end_offset <= span.end_offset) {
            comments += newline;
            comments += original.substr(comment.offset, comment.end_offset - comment.offset);
            comments += newline;
        }
    }
    auto& strings{state.session->command_strings()};
    state.io.project_path = strings.intern(path.generic_string());
    state.io.project_root = strings.intern(root.generic_string());
    state.io.registry_path = strings.intern(registry.generic_string());
    state.io.target_name = strings.intern(target);
    state.io.project_text = strings.intern_retained(original);
    state.io.saved_project_text = state.io.project_text;
    state.io.sources_begin = span.offset;
    state.io.sources_end = span.end_offset;
    state.io.source_comments = strings.intern_retained(comments);
}
void initialize_inputs(DocumentState& state,
                       std::span<DocumentSource const> sources,
                       std::string_view registry) {
    // Inputs may borrow the session pool that these insertions grow.
    std::vector<std::string> paths;
    std::vector<std::string> texts;
    std::string const registry_text{registry};
    std::set<std::string> unique;
    for (auto const& source : sources) {
        auto path{relative_path(source.path)};
        if (!unique.insert(path).second) {
            throw std::invalid_argument{"Source is already registered: " + path};
        }
        paths.push_back(std::move(path));
        texts.emplace_back(source.text);
    }
    auto& strings{state.session->command_strings()};
    for (std::size_t index{}; index < paths.size(); ++index) {
        state.baseline_sources.paths.push_back(strings.intern(paths[index]));
        state.baseline_sources.texts.push_back(strings.intern_retained(texts[index]));
    }
    auto const file{state.storage().sources.add("document-types.lispb", registry_text)};
    freeze_registry(state, file);
}
void load_inputs(DocumentState& state,
                 std::filesystem::path const& path,
                 std::string_view target_value) {
    std::string const target_name{target_value};
    auto const destination{std::filesystem::absolute(path).lexically_normal()};
    auto& storage{state.storage()};
    auto const file{storage.sources.load(destination)};
    auto project{lispb::parse_project(storage.syntax, file, destination)};
    auto const target{project.find_target(target_name)};
    if (!target || target->kind() != lispb::TargetKind::cpp_schema) {
        throw std::invalid_argument{"Unknown project target: " + target_name};
    }
    auto const paths{project.sources(*target)};
    auto const registry{std::filesystem::weakly_canonical(project.root / project.types(*target))};
    set_project(state, storage, file, destination, project.root, registry, target_name);
    auto& strings{state.session->command_strings()};
    std::set<std::string> unique;
    for (auto const& path_value : paths) {
        auto const name{relative_path(path_value.generic_string())};
        if (!unique.insert(name).second) {
            throw std::invalid_argument{"Source is already registered: " + name};
        }
        state.baseline_sources.paths.push_back(strings.intern(name));
        state.baseline_sources.texts.push_back(
            strings.intern_retained(read(project.root / path_value)));
    }
    freeze_registry(state, storage.sources.load(registry));
}
void finish_load(DocumentState& state) {
    auto& strings{state.session->command_strings()};
    for (std::size_t index{}; index < state.source_order.size(); ++index) {
        auto const file{state.source_order[index]};
        auto const path{state.baseline_sources.paths[index]};
        state.io.saved_sources.emplace(path, strings.intern_retained(render(state, file)));
        state.io.persisted_sources.emplace(path, state.baseline_sources.texts[index]);
        state.io.saved_paths.emplace(state.source_ids.at(file), path);
        state.io.saved_order.push_back(path);
    }
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
auto project_text(DocumentState const& state, std::span<RenderedSource const> sources)
    -> std::string {
    auto const& strings{state.session->command_strings()};
    std::vector<lispb::StringId> paths;
    for (auto const& source : sources) {
        paths.push_back(strings.find(source.path));
    }
    if (paths == state.io.saved_order) {
        return std::string{strings.view(state.io.saved_project_text)};
    }
    std::string list{"("};
    list += strings.view(state.io.source_comments);
    for (auto const& source : sources) {
        if (list.back() != '(' && list.back() != '\n') {
            list += ' ';
        }
        list += quote(source.path);
    }
    list += ')';
    std::string result{strings.view(state.io.project_text)};
    result.replace(state.io.sources_begin, state.io.sources_end - state.io.sources_begin, list);
    return result;
}
void accept_sources(DocumentState& state,
                    std::span<RenderedSource const> sources,
                    bool all_written) {
    auto& strings{state.session->command_strings()};
    std::map<lispb::StringId, lispb::StringId> saved;
    state.io.saved_order.clear();
    for (auto const& source : sources) {
        auto const path{strings.intern(source.path)};
        auto const previous{state.io.saved_sources.find(path)};
        auto const unchanged{previous != state.io.saved_sources.end() &&
                             strings.view(previous->second) == source.text};
        auto const text{unchanged ? previous->second : strings.intern_retained(source.text)};
        saved.emplace(path, text);
        state.io.saved_order.push_back(path);
        state.io.saved_paths[source.id] = path;
        if (all_written || !unchanged) {
            state.io.persisted_sources[path] = text;
        }
    }
    state.io.saved_sources = std::move(saved);
}
auto export_registry(DocumentState const& state) -> std::string {
    auto const& frozen{state.io.registry};
    auto const& strings{state.session->command_strings()};
    std::set<std::size_t> emitted;
    auto expand = [&](auto&& self, std::size_t row) -> std::string {
        if (!emitted.insert(row).second) {
            return {};
        }
        auto const original{strings.view(frozen.sources.texts[row])};
        std::string result;
        std::size_t offset{};
        for (std::size_t index{}; index < frozen.parents.size(); ++index) {
            if (frozen.parents[index] == row) {
                result += original.substr(offset, frozen.begins[index] - offset);
                result += self(self, frozen.targets[index]);
                result += '\n';
                offset = frozen.ends[index];
            }
        }
        result += original.substr(offset);
        return result;
    };
    return expand(expand, 0);
}
}

void DocumentState::restore_baseline() {
    auto& compiler{storage()};
    auto const& strings{session->command_strings()};
    auto const& registry{io.registry};
    std::vector<document_io::SourceFileId> files;
    for (std::size_t index{}; index < registry.sources.paths.size(); ++index) {
        files.push_back(compiler.sources.add(strings.view(registry.sources.paths[index]),
                                             strings.view(registry.sources.texts[index])));
    }
    assert(!files.empty());
    auto const parsed{lispb::codegen::parse_type_registry(
        compiler, files.front(), [&](document_io::SourceFileId parent, std::string_view include) {
            for (std::size_t index{}; index < registry.parents.size(); ++index) {
                if (files[registry.parents[index]] == parent &&
                    strings.view(registry.includes[index]) == include) {
                    return files[registry.targets[index]];
                }
            }
            assert(false && "Frozen registry contains every include");
            return document_io::SourceFileId{};
        })};
    registered_types = parsed.types.entries;
    for (std::size_t index{}; index < baseline_sources.paths.size(); ++index) {
        auto const file{compiler.sources.add(strings.view(baseline_sources.paths[index]),
                                             strings.view(baseline_sources.texts[index]))};
        auto const source{lispb::codegen::parse_module_source(compiler, file)};
        source_order.push_back(file);
        source_ids.emplace(file, SourceId{InstructionId{}, static_cast<std::uint32_t>(index + 1)});
        for (auto const module : source.modules) {
            modules.push_back(module);
            source_files.emplace(module, file);
            if (module.kind() == lispb::codegen::ModuleKind::normal) {
                auto const& declarations{compiler.declarations};
                for (auto const declaration : declarations.items(
                         declarations.normal_modules
                             .declarations[declarations.normal_modules.index(module)])) {
                    register_declaration(declaration);
                }
            }
        }
        comments.capture(compiler, source.modules, file);
    }
}

auto Document::project_path() const -> std::filesystem::path {
    auto const& value{state()};
    return std::filesystem::path{value.session->command_strings().view(value.io.project_path)};
}
auto Document::target_name() const -> std::string_view {
    auto const& value{state()};
    return value.session->command_strings().view(value.io.target_name);
}
auto Document::sources() const -> std::vector<RenderedSource> {
    auto const& value{state()};
    std::vector<RenderedSource> result;
    for (auto const file : value.source_order) {
        result.push_back({value.source_ids.at(file),
                          std::string{value.storage().sources.path(file)},
                          document_io::render(value, file)});
    }
    return result;
}
auto Document::declaration_source(std::string_view module_name, std::string_view name) const
    -> std::optional<std::string> {
    auto const& value{state()};
    auto const& store{value.storage().declarations};
    for (auto const module : value.modules) {
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
    auto const& value{state()};
    if (value.source_order.size() != value.io.saved_order.size()) {
        return true;
    }
    auto const& strings{value.session->command_strings()};
    for (std::size_t index{}; index < value.source_order.size(); ++index) {
        auto const file{value.source_order[index]};
        auto const path{strings.find(value.storage().sources.path(file))};
        if (path != value.io.saved_order[index] ||
            document_io::render(value, file) != strings.view(value.io.saved_sources.at(path))) {
            return true;
        }
    }
    return false;
}
auto Document::save() -> bool {
    auto& value{state()};
    auto& strings{value.session->command_strings()};
    value.session->begin_operation(value.id);
    if (strings.view(value.io.project_path).empty()) {
        value.session->record_error("Document has no project destination");
        return false;
    }
    try {
        auto const rendered{sources()};
        auto const text{document_io::project_text(value, rendered)};
        auto const destination{project_path()};
        std::filesystem::path const root{strings.view(value.io.project_root)};
        if (document_io::read(destination) != strings.view(value.io.saved_project_text)) {
            throw std::invalid_argument{"Project changed on disk; reload before saving"};
        }
        std::set<lispb::StringId> current_paths;
        for (auto const& source : rendered) {
            auto const key{strings.find(source.path)};
            current_paths.insert(key);
            auto const path{root / source.path};
            auto const saved{value.io.persisted_sources.find(key)};
            if (saved != value.io.persisted_sources.end()) {
                if (document_io::read(path) != strings.view(saved->second)) {
                    throw std::invalid_argument{"Source changed on disk: " + path.string()};
                }
            } else if (std::filesystem::exists(path)) {
                throw std::invalid_argument{"Source destination already exists: " + path.string()};
            }
        }
        std::set<lispb::StringId> removed;
        for (auto const& source : rendered) {
            auto const saved{value.io.saved_paths.find(source.id)};
            if (saved == value.io.saved_paths.end() || strings.view(saved->second) == source.path) {
                continue;
            }
            auto const expected{value.io.persisted_sources.find(saved->second)};
            if (expected != value.io.persisted_sources.end() &&
                document_io::read(root / strings.view(saved->second)) !=
                    strings.view(expected->second)) {
                throw std::invalid_argument{"Renamed source changed on disk: " +
                                            std::string{strings.view(saved->second)}};
            }
            if (!current_paths.contains(saved->second)) {
                removed.insert(saved->second);
            }
        }
        DocumentFileUpdates updates;
        for (auto const& source : rendered) {
            auto const saved{value.io.saved_sources.find(strings.find(source.path))};
            if (saved == value.io.saved_sources.end() ||
                strings.view(saved->second) != source.text) {
                updates.write(root / source.path, source.text);
            }
        }
        if (text != strings.view(value.io.saved_project_text)) {
            updates.write(destination, text);
        }
        for (auto const path : removed) {
            updates.erase(root / strings.view(path));
        }
        updates.publish();
        if (text != strings.view(value.io.saved_project_text)) {
            value.io.saved_project_text = strings.intern_retained(text);
        }
        for (auto const path : removed) {
            value.io.persisted_sources.erase(path);
        }
        document_io::accept_sources(value, rendered, false);
        return true;
    } catch (std::invalid_argument const& error) {
        value.session->record_error(error.what());
    } catch (std::runtime_error const& error) {
        value.session->record_error(error.what());
    }
    return false;
}
auto Document::save_as(std::filesystem::path const& path, std::string_view target_value) -> bool {
    auto& value{state()};
    auto& strings{value.session->command_strings()};
    std::string const target{target_value};
    value.session->begin_operation(value.id);
    try {
        auto const destination{std::filesystem::absolute(path).lexically_normal()};
        if (destination == project_path()) {
            return save();
        }
        if (target.empty() || !std::ranges::all_of(target, [](unsigned char character) {
                return std::isalnum(character) != 0 || character == '_' || character == '-';
            })) {
            throw std::invalid_argument{
                "Expected a target name containing letters, digits, '_' or '-'"};
        }
        if (std::filesystem::exists(destination)) {
            throw std::invalid_argument{"Project destination already exists: " +
                                        destination.string()};
        }
        auto const rendered{sources()};
        auto const root{destination.parent_path()};
        bool const has_project{!strings.view(value.io.project_path).empty()};
        auto const registry{has_project
                                ? std::filesystem::path{strings.view(value.io.registry_path)}
                                : root / "types.lispb"};
        std::set<std::string> destinations{output_path_key(destination)};
        if (!destinations.insert(output_path_key(registry)).second) {
            throw std::invalid_argument{"Project destination collides with the type registry"};
        }
        if (!has_project && std::filesystem::exists(registry)) {
            throw std::invalid_argument{"Type registry destination already exists: " +
                                        registry.string()};
        }
        std::string text{"(lispb-project :language-version 1 :project-root \".\"\n  (cpp-schema "};
        text += target + "\n    :types " + document_io::quote(registry.generic_string()) +
                "\n    :sources (";
        for (auto const& source : rendered) {
            auto const source_path{root / document_io::relative_path(source.path)};
            if (!destinations.insert(output_path_key(source_path)).second) {
                throw std::invalid_argument{
                    "Source destination collides with another project file: " +
                    source_path.string()};
            }
            if (std::filesystem::exists(source_path)) {
                throw std::invalid_argument{"Source destination already exists: " +
                                            source_path.string()};
            }
            text += document_io::quote(source.path) + ' ';
        }
        text += ")\n    :output-root (project-path \"generated\")))\n";
        lispb::CompilerStorage temporary;
        auto const file{temporary.sources.add(destination.generic_string(), text)};
        DocumentFileUpdates updates;
        for (auto const& source : rendered) {
            updates.write(root / source.path, source.text);
        }
        if (!has_project) {
            updates.write(registry, document_io::export_registry(value));
        }
        updates.write(destination, text);
        updates.publish();
        document_io::set_project(value, temporary, file, destination, root, registry, target);
        value.io.persisted_sources.clear();
        value.io.saved_paths.clear();
        document_io::accept_sources(value, rendered, true);
        return true;
    } catch (std::invalid_argument const& error) {
        value.session->record_error(error.what());
    } catch (std::runtime_error const& error) {
        value.session->record_error(error.what());
    }
    return false;
}
}
