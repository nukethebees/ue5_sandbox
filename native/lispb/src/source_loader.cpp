#include <codegen/source_loader.h>

#include "schema_parser.h"

#include <codegen/schema/declarations.h>
#include <codegen/schema/schema_version.h>
#include <codegen/schema/type_registry.h>
#include <codegen/sexpr/fields.h>
#include <codegen/sexpr/parser.h>
#include <codegen/sexpr/reader.h>
#include <codegen/sexpr/source_error.h>
#include <codegen/sexpr/sources.h>
#include <codegen/sexpr/syntax.h>
#include <lispb/compiler_storage.h>
#include <lispb/diagnostics.h>

#include <cstdint>
#include <exception>
#include <filesystem>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace lispb::codegen {
namespace detail {
namespace {
thread_local ModuleSourceReadHook module_source_read_hook{};
}

auto set_module_source_read_hook_for_testing(ModuleSourceReadHook const hook) noexcept
    -> ModuleSourceReadHook {
    return std::exchange(module_source_read_hook, hook);
}

} // namespace detail

auto load_type_registry(lispb::CompilerStorage& storage, std::filesystem::path const& root)
    -> LoadedTypeRegistry {
    auto& syntax{storage.syntax};
    SchemaParser parser{syntax};
    sexpr::Parser fields_parser{syntax};
    LoadedTypeRegistry result{
        .types = TypeRegistry{storage.registered_types}, .sources = {}, .declarations = {}};
    std::map<std::filesystem::path, sexpr::SourceFileId> loaded;
    std::set<std::filesystem::path> active;
    auto load = [&](auto&& self, std::filesystem::path const& source_path) -> sexpr::SourceFileId {
        auto const path{std::filesystem::weakly_canonical(source_path)};
        if (active.contains(path)) {
            storage.diagnostics.fail(syntax.sources(),
                                     {.file = loaded.at(path)},
                                     "Cyclic type registry include: " + path.string());
        }
        if (auto const found{loaded.find(path)}; found != loaded.end()) {
            return found->second;
        }
        active.insert(path);
        auto const file{syntax.sources().load(path)};
        syntax.sources().set_kind(file, sexpr::SourceFileKind::type_registry);
        loaded.emplace(path, file);
        auto const forms{sexpr::read_forms(syntax, file)};
        result.sources.push_back(file);
        std::vector<sexpr::SourceFileId> include_targets;
        std::vector<sexpr::SourceSpan> include_spans;
        for (std::uint32_t form_index{}; form_index < forms.count; ++form_index) {
            auto const form{syntax.forms(forms)[form_index]};
            if (syntax.head(form) == "include") {
                sexpr::Fields const include{fields_parser, form, "include", 1};
                include.validate({});
                auto const target{std::filesystem::weakly_canonical(
                    path.parent_path() /
                    fields_parser.text(include.positional(0), "include path"))};
                try {
                    auto const target_file{self(self, target)};
                    include_targets.push_back(target_file);
                    include_spans.push_back(syntax.span(form));
                } catch (sexpr::SourceError const&) {
                    throw;
                } catch (std::exception const& error) {
                    fields_parser.fail(form, error.what());
                }
                continue;
            }
            auto [name, type]{parser.parse_type_definition(form, result.types)};
            if (!result.types.bind(name, type)) {
                fields_parser.fail(form,
                                   "duplicate type definition '" +
                                       std::string{storage.strings.view(name)} + "'");
            }
            result.declarations.emplace(name, form);
        }
        syntax.sources().set_includes(file, include_targets, include_spans);
        active.erase(path);
        return file;
    };
    load(load, root);
    return result;
}

auto parse_module_source(lispb::CompilerStorage& storage, sexpr::SourceFileId const file)
    -> LoadedModuleSource {
    auto& syntax{storage.syntax};
    syntax.sources().set_kind(file, sexpr::SourceFileKind::module);
    SchemaParser parser{syntax};
    LoadedModuleSource result{.file = file, .forms = {}, .modules = {}};
    auto const forms{sexpr::read_forms(storage.syntax, file)};
    result.forms = forms;
    std::optional<DiagnosticId> first_error;
    for (auto const form : syntax.forms(forms)) {
        try {
            result.modules.push_back(parser.parse_module(storage.declarations, form));
        } catch (sexpr::SourceError const& error) {
            if (!first_error) {
                first_error = error.diagnostic();
            }
        }
    }
    if (first_error) {
        throw sexpr::SourceError{storage.diagnostics, storage.sources, *first_error};
    }
    return result;
}

auto load_module_source(lispb::CompilerStorage& storage, std::filesystem::path const& path)
    -> LoadedModuleSource {
    auto const file{storage.sources.load(path)};
    if (detail::module_source_read_hook != nullptr) {
        detail::module_source_read_hook(path);
    }
    return parse_module_source(storage, file);
}

auto load_sources(lispb::CompilerStorage& storage,
                  std::filesystem::path const& types_path,
                  std::span<std::filesystem::path const> const module_paths) -> Manifest {
    return load_sources(storage, load_type_registry(storage, types_path), module_paths);
}

auto load_sources(lispb::CompilerStorage& storage,
                  LoadedTypeRegistry const& registry,
                  std::span<std::filesystem::path const> const module_paths) -> Manifest {
    std::vector<ModuleHandle> modules;
    for (auto const& module_path : module_paths) {
        auto source{load_module_source(storage, module_path)};
        modules.insert(modules.end(), source.modules.begin(), source.modules.end());
    }
    return Manifest{
        manifest_schema_version, registry.types, storage.declarations, std::move(modules)};
}

} // namespace lispb::codegen
