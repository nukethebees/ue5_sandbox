#include <ioj/layout/editor_session.hpp>

#include "document_io.hpp"

#include <codegen/sexpr/source_error.h>
#include <codegen/source_loader.h>
#include <lispb/diagnostic_severity.h>
#include <lispb/project_loader.h>

#include <algorithm>
#include <stdexcept>

namespace ioj::layout {

auto EditorSession::from_source(std::string_view source, std::string_view registry) -> Document {
    DocumentSource const input{"document.lispb", source};
    return from_sources(std::span{&input, 1}, registry);
}
auto EditorSession::from_sources(std::span<DocumentSource const> sources, std::string_view registry)
    -> Document {
    begin_operation();
    Document document{*this};
    auto& state{document.state_};
    try {
        std::vector<std::string> paths;
        std::vector<std::string> texts;
        for (auto const source : sources) {
            paths.emplace_back(source.path);
            texts.emplace_back(source.text);
        }
        std::vector<DocumentSource> inputs;
        auto const count{paths.size()};
        for (std::size_t index{}; index < count; ++index) {
            inputs.push_back({paths[index], texts[index]});
        }
        std::string const registry_text{registry};
        if (!registry_text.empty()) {
            auto const file{state.storage().sources.add("document-types.lispb", registry_text)};
            state.io.registry_text = state.storage().sources.text_id(file);
            state.registered_types =
                lispb::codegen::parse_type_registry(state.storage(), file).types.entries;
        }
        for (auto const& input : inputs) {
            auto const path{std::filesystem::path{input.path}.lexically_normal()};
            if (path.empty() || path == "." || path.has_root_path() ||
                std::ranges::find(path, std::filesystem::path{".."}) != path.end()) {
                throw std::invalid_argument{
                    "Source path must be relative and stay within the project"};
            }
            auto const name{path.generic_string()};
            auto const key{state.storage().strings.intern_retained(name)};
            if (std::ranges::any_of(state.source_order, [&](auto file) {
                    return state.storage().strings.find(state.storage().sources.path(file)) == key;
                })) {
                throw std::invalid_argument{"Duplicate source path"};
            }
            document_io::append_source(state, state.storage().sources.add(name, input.text));
        }
        document_io::finish_load(state);
    } catch (lispb::codegen::sexpr::SourceError const&) {
    } catch (std::runtime_error const& error) {
        state.session->record_error(error.what());
    } catch (std::invalid_argument const& error) {
        state.session->record_error(error.what());
    }
    return document;
}
auto EditorSession::load(std::filesystem::path const& project_path, std::string_view target_value)
    -> Document {
    begin_operation();
    std::string const target_name{target_value};
    Document document{*this};
    auto& state{document.state_};
    try {
        state.storage().sources.refresh();
        state.project_path = std::filesystem::absolute(project_path).lexically_normal();
        state.project.emplace(lispb::load_project(state.storage().syntax, state.project_path));
        state.target = state.project->find_target(target_name);
        if (!state.target || state.target->kind() != lispb::TargetKind::cpp_schema) {
            throw std::invalid_argument{"Expected an existing C++ schema target"};
        }
        state.io.project_source = state.storage().sources.load(state.project_path);
        state.io.saved_project_text = state.storage().sources.text_id(state.io.project_source);
        state.io.project_sources_span =
            document_io::sources_span(state, state.io.project_source, target_name);
        state.registered_types =
            lispb::codegen::load_type_registry(
                state.storage(), state.project->root / state.project->types(*state.target))
                .types.entries;
        for (auto const& source : state.project->sources(*state.target)) {
            auto const file{state.storage().sources.load(state.project->root / source,
                                                         source.generic_string())};
            document_io::append_source(state, file);
        }
        document_io::finish_load(state);
    } catch (lispb::codegen::sexpr::SourceError const&) {
    } catch (std::invalid_argument const& error) {
        state.session->record_error(error.what());
    } catch (std::runtime_error const& error) {
        state.session->record_error(error.what());
    }
    return document;
}

auto EditorSession::storage() -> lispb::CompilerStorage& {
    return storage_;
}
auto EditorSession::diagnostics() const -> lispb::Diagnostics const& {
    return storage_.diagnostics;
}
auto EditorSession::format_diagnostic(lispb::DiagnosticId diagnostic) const -> std::string {
    return storage_.diagnostics.format(diagnostic, storage_.sources);
}
auto EditorSession::last_diagnostic_begin() const -> std::size_t {
    return diagnostic_begin_;
}
void EditorSession::begin_operation() {
    diagnostic_begin_ = storage_.diagnostics.size();
}
void EditorSession::record_error(std::string_view message) {
    storage_.diagnostics.add(lispb::DiagnosticSeverity::error, {}, message);
}
auto EditorSession::allocate_document_id() -> DocumentId {
    auto const id{next_document_id_};
    ++next_document_id_.value;
    return id;
}

}
