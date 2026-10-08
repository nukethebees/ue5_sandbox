#include <ioj/files.h>
#include <ioj/levels/authoring/campaign_parser.h>
#include <ioj/levels/authoring/definition_reader.h>
#include <ioj/levels/authoring/grammar.h>
#include <ioj/levels/authoring/level_parser.h>
#include <ioj/s7/interpreter.h>

#include <format>
#include <utility>

namespace ioj::levels::authoring {
namespace {
auto diagnostic_code(s7::AstErrorCode const code) -> DiagnosticCode {
    switch (code) {
        case s7::AstErrorCode::EvaluationFailed:
            return DiagnosticCode::ScriptEvaluationFailed;
        case s7::AstErrorCode::UnsupportedValue:
            return DiagnosticCode::UnsupportedValue;
        case s7::AstErrorCode::ImproperList:
            return DiagnosticCode::ImproperList;
        case s7::AstErrorCode::CyclicStructure:
            return DiagnosticCode::CyclicStructure;
        case s7::AstErrorCode::LimitExceeded:
            return DiagnosticCode::LimitExceeded;
    }
    std::unreachable();
}
auto evaluate(std::string_view const source, std::filesystem::path const& library_root)
    -> std::expected<s7::Ast, Diagnostics> {
    s7::InterpreterOptions options;
    if (!library_root.empty()) {
        options.script_library_root_utf8 = ioj::path_to_utf8(library_root);
    }
    s7::Interpreter interpreter{std::move(options)};
    std::string expression{"(begin\n"};
    for (auto const kind : {RecordKind::Level,
                            RecordKind::Campaign,
                            RecordKind::Entity,
                            RecordKind::Mission,
                            RecordKind::Camera,
                            RecordKind::CollisionGrid,
                            RecordKind::MissionEvent}) {
        auto const name{to_serialized_string(kind)};
        std::format_to(std::back_inserter(expression),
                       "(define ({} . properties) (cons :{} properties))\n",
                       name,
                       name);
    }
    auto const criterion{to_serialized_string(RecordKind::LevelCompleted)};
    std::format_to(
        std::back_inserter(expression), "(define ({} id) (list :{} id))\n", criterion, criterion);
    expression.append(source);
    expression.append("\n)");
    auto ast{interpreter.evaluate_ast(expression)};
    if (!ast) {
        Diagnostics errors;
        for (auto& error : ast.error()) {
            errors.emplace_back(
                diagnostic_code(error.code), std::move(error.node_path), std::move(error.message));
        }
        return std::unexpected{std::move(errors)};
    }
    return std::move(*ast);
}
auto file_error(ioj::FileReadError const& error) -> Diagnostics {
    return {{error.code == ioj::FileReadErrorCode::OpenFailed ? DiagnosticCode::FileOpenFailed
                                                              : DiagnosticCode::FileReadFailed,
             "$",
             std::format("Unable to read script file: {}", error.system_error.message()),
             error.source_path}};
}

auto evaluate_file(std::filesystem::path const& path, std::filesystem::path const& library_root)
    -> std::expected<s7::Ast, Diagnostics> {
    auto source{ioj::read_file(path)};
    if (!source) {
        return std::unexpected{file_error(source.error())};
    }
    return evaluate(*source, library_root);
}

template <typename Result>
auto with_source_path(Result result, std::filesystem::path const& path) -> Result {
    if (!result) {
        for (auto& error : result.error()) {
            error.source_path = path;
        }
    }
    return result;
}

auto decode_level(s7::Ast const& ast) -> LevelDefinitionReadResult {
    auto definition{parse_level(ast)};
    if (!definition) {
        return definition;
    }
    auto validation{validate_level(*definition)};
    if (!validation) {
        return std::unexpected{std::move(validation.error())};
    }
    return definition;
}

auto decode_campaign(s7::Ast const& ast) -> CampaignDefinitionReadResult {
    auto definition{parse_campaign(ast)};
    if (!definition) {
        return definition;
    }
    auto validation{validate_campaign(*definition)};
    if (!validation) {
        return std::unexpected{std::move(validation.error())};
    }
    return definition;
}
}

DefinitionReader::DefinitionReader(std::filesystem::path script_library_root)
    : script_library_root_{std::move(script_library_root)} {}
auto DefinitionReader::read_level_source(std::string_view const source) const
    -> LevelDefinitionReadResult {
    return evaluate(source, script_library_root_).and_then(decode_level);
}
auto DefinitionReader::read_campaign_source(std::string_view const source) const
    -> CampaignDefinitionReadResult {
    return evaluate(source, script_library_root_).and_then(decode_campaign);
}
auto DefinitionReader::read_level_file(std::filesystem::path const& path) const
    -> LevelDefinitionReadResult {
    auto const library{script_library_root_.empty() ? path.parent_path() / "Libraries"
                                                    : script_library_root_};
    return with_source_path(evaluate_file(path, library).and_then(decode_level), path);
}
auto DefinitionReader::read_campaign_file(std::filesystem::path const& path) const
    -> CampaignDefinitionReadResult {
    auto const library{script_library_root_.empty() ? path.parent_path().parent_path() / "Libraries"
                                                    : script_library_root_};
    return with_source_path(evaluate_file(path, library).and_then(decode_campaign), path);
}
}
