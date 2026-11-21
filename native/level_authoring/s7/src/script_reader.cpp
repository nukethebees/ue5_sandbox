#include "logic_dsl.h"
#include "reader_diagnostics.h"
#include <ioj/files.h>
#include <ioj/levels/authoring/definition_reader.h>
#include <ioj/levels/authoring/level_inputs_reader.h>
#include <ioj/levels/authoring/record_constructors.h>
#include <ioj/s7/interpreter.h>

#include <algorithm>
#include <concepts>
#include <format>
#include <utility>

namespace ioj::levels::authoring {
namespace {

auto dsl_source() -> std::string {
    std::string source{"(begin\n"};
    source.append(s7_record_constructors());
    source.append(logic_dsl_source);
    source.append("(define (settings-layers . layers) (cons :settings-layers layers))\n");
    source.append("\n)");
    return source;
}

template <typename Run>
auto with_interpreter(std::filesystem::path const& file_directory,
                      std::filesystem::path const& library_root,
                      Run run,
                      s7::ModuleInput* module = nullptr)
    -> decltype(run(std::declval<s7::Interpreter&>())) {
    s7::InterpreterOptions options;
    options.script_library_root_utf8 =
        ioj::path_to_utf8(library_root.empty() ? file_directory : library_root);
    if (module) {
        module->imported = false;
    }
    options.module_input = module;
    s7::Interpreter interpreter{std::move(options)};
    auto initialized{interpreter.evaluate(dsl_source())};
    if (!initialized) {
        return decltype(run(interpreter)){std::unexpect,
                                          {Diagnostic{DiagnosticCode::ScriptEvaluationFailed,
                                                      "$",
                                                      std::move(initialized.error())}}};
    }
    return run(interpreter);
}

template <typename Result>
auto with_source_path(Result result, std::filesystem::path const& path) -> Result {
    if (!result) {
        for (auto& error : result.error()) {
            if (error.source_path.empty()) {
                error.source_path = path;
            }
        }
    }
    return result;
}

template <typename Definition, typename Decode>
auto read_entry(s7::Interpreter& interpreter,
                std::filesystem::path const& root,
                s7::ReadResult<std::string> const& path,
                char const* export_name,
                Decode decode) -> DefinitionEntry<Definition> {
    if (!path) {
        return {.definition = DefinitionReadResult<Definition>{std::unexpect,
                                                               detail::diagnostics(path.error())}};
    }
    auto const relative{std::filesystem::path{std::u8string{path->begin(), path->end()}}};
    if (relative.empty() || relative.has_root_path() ||
        std::ranges::any_of(relative, [](auto const& part) { return part == ".."; })) {
        return {.definition = DefinitionReadResult<Definition>{
                    std::unexpect,
                    {Diagnostic{DiagnosticCode::InvalidType,
                                "path",
                                "Definition paths must be relative to the root directory"}}}};
    }
    auto file{root / relative};
    std::string source;
    auto result{interpreter
                    .import_ast(ioj::path_to_utf8(root),
                                *path,
                                export_name,
                                std::same_as<Definition, LevelDefinition> ? &source : nullptr)
                    .transform_error(detail::diagnostics)
                    .and_then(decode)};
    auto definition{with_source_path(std::move(result), file)};
    return {std::move(file), std::move(definition), std::move(source)};
}

}

DefinitionReader::DefinitionReader(std::filesystem::path script_library_root)
    : script_library_root_{std::move(script_library_root)} {}

auto DefinitionReader::read_root_file(std::filesystem::path const& path) const
    -> CatalogReadResult {
    auto const full_path{std::filesystem::absolute(path)};
    auto const root{full_path.parent_path()};
    return with_source_path(
        with_interpreter(root,
                         script_library_root_,
                         [&](s7::Interpreter& interpreter) -> CatalogReadResult {
                             auto paths{interpreter
                                            .import_ast(ioj::path_to_utf8(root),
                                                        ioj::path_to_utf8(full_path.filename()),
                                                        "catalog")
                                            .and_then([](s7::Ast const& ast) {
                                                return s7::read<CatalogInput>(
                                                    s7::Reader{ast}, ast.root, "catalog");
                                            })};
                             if (!paths) {
                                 return CatalogReadResult{
                                     std::unexpect, detail::diagnostics(std::move(paths.error()))};
                             }
                             std::vector<DefinitionEntry<LevelDefinition>> levels;
                             std::vector<DefinitionEntry<CampaignDefinition>> campaigns;
                             for (auto const& path : paths->levels) {
                                 levels.push_back(read_entry<LevelDefinition>(
                                     interpreter, root, path, "mission", [](s7::Ast const& ast) {
                                         return parse_level(ast, ast.root);
                                     }));
                             }
                             for (auto const& path : paths->campaigns) {
                                 campaigns.push_back(read_entry<CampaignDefinition>(
                                     interpreter, root, path, "campaign", [](s7::Ast const& ast) {
                                         return parse_campaign(ast, ast.root);
                                     }));
                             }
                             return CatalogReadResult{
                                 std::in_place, std::move(levels), std::move(campaigns)};
                         }),
        full_path);
}

auto DefinitionReader::read_settings_file(std::filesystem::path const& path,
                                          settings::Settings base) const -> SettingsReadResult {
    return with_source_path(
        read_script_file(path, "game-settings").and_then([&](s7::Ast const& ast) {
            return parse_settings(ast, ast.root, std::move(base));
        }),
        path);
}

auto DefinitionReader::read_script_file(std::filesystem::path const& path,
                                        char const* export_name,
                                        s7::ModuleInput* module) const
    -> std::expected<s7::Ast, Diagnostics> {
    auto const file{std::filesystem::absolute(path)};
    auto const directory{file.parent_path()};
    return with_source_path(with_interpreter(
                                directory,
                                script_library_root_,
                                [&](s7::Interpreter& interpreter) {
                                    return interpreter
                                        .import_ast(ioj::path_to_utf8(directory),
                                                    ioj::path_to_utf8(file.filename()),
                                                    export_name)
                                        .transform_error(detail::diagnostics);
                                },
                                module),
                            path);
}
auto DefinitionReader::read_level_file(std::filesystem::path const& path,
                                       s7::ModuleInput* module) const -> LevelDefinitionReadResult {
    return with_source_path(
        read_script_file(path, "mission", module).and_then([&](s7::Ast const& ast) {
            return parse_level(ast, ast.root);
        }),
        path);
}
}
