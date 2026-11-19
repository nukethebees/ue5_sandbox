#include "logic_dsl.h"
#include "reader_diagnostics.h"
#include <ioj/files.h>
#include <ioj/levels/authoring/definition_reader.h>
#include <ioj/levels/authoring/level_inputs_reader.h>
#include <ioj/levels/authoring/record_constructors.h>
#include <ioj/levels/campaign_validation.h>
#include <ioj/s7/interpreter.h>

#include <codegen/sexpr/form_builder.h>
#include <codegen/sexpr/sexpr_renderer.h>
#include <lispb/strings.h>

#include <algorithm>
#include <concepts>
#include <format>
#include <utility>

namespace ioj::levels::authoring {
namespace {
using MaterializedSource = std::expected<s7::Ast, Diagnostics>;

auto dsl_source() -> std::string {
    std::string source{"(begin\n"};
    source.append(s7_record_constructors());
    source.append(logic_dsl_source);
    source.append("(define (settings-layers . layers) (cons :settings-layers layers))\n");
    source.append("\n)");
    return source;
}

template <typename Run>
auto with_interpreter(std::filesystem::path const& root, Run run)
    -> decltype(run(std::declval<s7::Interpreter&>())) {
    s7::InterpreterOptions options;
    if (!root.empty()) {
        options.script_library_root_utf8 = ioj::path_to_utf8(root);
    }
    s7::Interpreter interpreter{std::move(options)};
    auto initialized{interpreter.evaluate(dsl_source())};
    if (!initialized) {
        return std::unexpected{Diagnostics{
            {DiagnosticCode::ScriptEvaluationFailed, "$", std::move(initialized.error())}}};
    }
    return run(interpreter);
}

auto evaluate(std::string_view source, std::filesystem::path const& root) -> MaterializedSource {
    return with_interpreter(root, [&](s7::Interpreter& interpreter) {
        return interpreter.evaluate_ast(std::format("(begin\n{}\n)", source))
            .transform_error(detail::diagnostics);
    });
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

auto decode_campaign(s7::Ast const& ast) -> CampaignDefinitionReadResult {
    auto definition{parse_campaign(ast, ast.root)};
    if (!definition) {
        return definition;
    }
    auto validation{validate_campaign(*definition)};
    if (!validation) {
        return std::unexpected{std::move(validation.error())};
    }
    return definition;
}

auto module_value(std::string_view source,
                  std::filesystem::path const& path,
                  std::string_view export_name,
                  std::filesystem::path const& root) -> MaterializedSource {
    lispb::Strings strings;
    lispb::codegen::sexpr::SExprIR ir{strings};
    auto const module{
        ir.form("read-module", {ir.string(ioj::path_to_utf8(path)), ir.string(source)}).build()};
    auto const lookup{ir.form("module-ref", {module, ir.quote(export_name)}).build()};
    return with_source_path(
        evaluate(
            lispb::codegen::sexpr::SExprRenderer{ir, lispb::codegen::sexpr::Dialect::scheme}.render(
                lookup),
            root),
        path);
}

template <typename Definition, typename Decode>
auto read_entry(s7::Interpreter& interpreter,
                std::filesystem::path const& root,
                s7::ReadResult<std::string> const& path,
                char const* export_name,
                Decode decode) -> DefinitionEntry<Definition> {
    if (!path) {
        return {.definition = std::unexpected{detail::diagnostics(path.error())}};
    }
    auto const relative{std::filesystem::path{std::u8string{path->begin(), path->end()}}};
    if (relative.empty() || relative.has_root_path() ||
        std::ranges::any_of(relative, [](auto const& part) { return part == ".."; })) {
        return {.definition = std::unexpected{
                    Diagnostics{{DiagnosticCode::InvalidType,
                                 "path",
                                 "Definition paths must be relative to the root directory"}}}};
    }
    auto file{root / relative};
    std::string source;
    auto result{interpreter
                    .import_ast(*path,
                                export_name,
                                std::same_as<Definition, LevelDefinition> ? &source : nullptr)
                    .transform_error(detail::diagnostics)
                    .and_then(decode)};
    auto definition{with_source_path(std::move(result), file)};
    return {std::move(file), std::move(definition), std::move(source)};
}

auto read_source(std::filesystem::path const& path) -> std::expected<std::string, Diagnostics> {
    auto source{ioj::read_file(path)};
    if (!source) {
        auto const& error{source.error()};
        return std::unexpected{Diagnostics{
            {error.code == ioj::FileReadErrorCode::OpenFailed ? DiagnosticCode::FileOpenFailed
                                                              : DiagnosticCode::FileReadFailed,
             "$",
             std::format("Unable to read script file: {}", error.system_error.message()),
             path}}};
    }
    return std::move(*source);
}
}

DefinitionReader::DefinitionReader(std::filesystem::path script_library_root)
    : script_library_root_{std::move(script_library_root)} {}

auto DefinitionReader::read_root_file(std::filesystem::path const& path) const
    -> std::expected<DefinitionCatalog, Diagnostics> {
    auto const full_path{std::filesystem::absolute(path)};
    auto const root{full_path.parent_path()};
    return with_source_path(
        with_interpreter(
            root,
            [&](s7::Interpreter& interpreter) -> std::expected<DefinitionCatalog, Diagnostics> {
                auto paths{interpreter
                               .import_ast(ioj::path_to_utf8(root),
                                           ioj::path_to_utf8(full_path.filename()),
                                           "catalog")
                               .and_then([](s7::Ast const& ast) {
                                   return s7::read<CatalogInput>(
                                       s7::Reader{ast}, ast.root, "catalog");
                               })};
                if (!paths) {
                    return std::unexpected{detail::diagnostics(std::move(paths.error()))};
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
                return DefinitionCatalog{std::move(levels), std::move(campaigns)};
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
                                        std::string_view export_name) const
    -> std::expected<s7::Ast, Diagnostics> {
    auto source{read_source(path)};
    if (!source) {
        return std::unexpected{std::move(source.error())};
    }
    auto const root{script_library_root_.empty() ? std::filesystem::absolute(path).parent_path()
                                                 : script_library_root_};
    return module_value(*source, path, export_name, root);
}
auto DefinitionReader::read_level_file(std::filesystem::path const& path) const
    -> LevelDefinitionReadResult {
    return with_source_path(read_script_file(path, "mission").and_then([&](s7::Ast const& ast) {
        return parse_level(ast, ast.root);
    }),
                            path);
}
}
