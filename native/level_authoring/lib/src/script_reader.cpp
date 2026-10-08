#include <ioj/files.h>
#include <ioj/levels/authoring/catalog_parser.h>
#include <ioj/levels/authoring/definition_reader.h>
#include <ioj/levels/authoring/grammar.h>
#include <ioj/s7/interpreter.h>
#include <ioj/s7/sexpression_emitter.h>

#include <format>
#include <utility>

namespace ioj::levels::authoring {
namespace {
using MaterializedSource = std::expected<s7::Ast, Diagnostics>;

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

auto dsl_source() -> std::string {
    std::string source{"(begin\n"};
    for (auto const kind : {RecordKind::Level,
                            RecordKind::Campaign,
                            RecordKind::Entity,
                            RecordKind::Mission,
                            RecordKind::Camera,
                            RecordKind::CollisionGrid,
                            RecordKind::MissionEvent}) {
        auto const name{to_serialized_string(kind)};
        std::format_to(std::back_inserter(source),
                       "(define ({} . properties) (cons :{} properties))\n",
                       name,
                       name);
    }
    auto const criterion{to_serialized_string(RecordKind::LevelCompleted)};
    std::format_to(
        std::back_inserter(source), "(define ({} id) (list :{} id))\n", criterion, criterion);
    source.append(R"(
      (define catalog
        (let ((include-load load-script) (include-format format))
          (define (include path)
            (catch #t
              (lambda () (list :included-definition :path path :value (include-load path)))
              (lambda (type info)
                (list :included-definition :path path :error (apply include-format #f info)))))
          (define (load-properties properties)
            (if (or (null? properties) (null? (cdr properties)))
                properties
                (let ((name (car properties)) (value (cadr properties)))
                  (cons name
                    (cons (if (and (memq name '(:levels :campaigns)) (list? value))
                              (map include value)
                              value)
                          (load-properties (cddr properties)))))))
          (lambda properties (cons :catalog (load-properties properties)))))
    )");
    source.append("\n)");
    return source;
}

auto evaluate(std::string_view const expression, std::filesystem::path const& root)
    -> MaterializedSource {
    s7::InterpreterOptions options;
    if (!root.empty()) {
        options.script_library_root_utf8 = ioj::path_to_utf8(root);
    }
    s7::Interpreter interpreter{std::move(options)};
    auto const initialized{interpreter.evaluate(dsl_source())};
    if (!initialized) {
        return std::unexpected{
            Diagnostics{{DiagnosticCode::ScriptEvaluationFailed, "$", initialized.error()}}};
    }
    auto ast{interpreter.evaluate_ast(std::format("(begin\n{}\n)", expression))};
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
    s7::SexpressionEmitter expression;
    expression.begin_list("load-script");
    expression.string(ioj::path_to_utf8(full_path.filename()));
    expression.end_list();
    auto ast{evaluate(std::move(expression).finish(), full_path.parent_path())};
    if (!ast) {
        return with_source_path(
            std::expected<DefinitionCatalog, Diagnostics>{std::unexpect, std::move(ast.error())},
            full_path);
    }

    auto catalog{parse_catalog(std::move(*ast), full_path.parent_path())};
    if (catalog) {
        for (auto& entry : catalog->levels) {
            if (entry.definition) {
                auto validation{validate_level(*entry.definition)};
                if (!validation) {
                    entry.definition = std::unexpected{std::move(validation.error())};
                }
            }
            entry.definition = with_source_path(std::move(entry.definition), entry.source_path);
        }
        for (auto& entry : catalog->campaigns) {
            if (entry.definition) {
                auto validation{validate_campaign(*entry.definition)};
                if (!validation) {
                    entry.definition = std::unexpected{std::move(validation.error())};
                }
            }
            entry.definition = with_source_path(std::move(entry.definition), entry.source_path);
        }
    }
    return with_source_path(std::move(catalog), full_path);
}

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
    auto source{read_source(path)};
    if (!source) {
        return std::unexpected{std::move(source.error())};
    }
    auto const root{script_library_root_.empty() ? std::filesystem::absolute(path).parent_path()
                                                 : script_library_root_};
    return with_source_path(evaluate(*source, root).and_then(decode_level), path);
}
auto DefinitionReader::read_campaign_file(std::filesystem::path const& path) const
    -> CampaignDefinitionReadResult {
    auto source{read_source(path)};
    if (!source) {
        return std::unexpected{std::move(source.error())};
    }
    auto const root{script_library_root_.empty()
                        ? std::filesystem::absolute(path).parent_path().parent_path()
                        : script_library_root_};
    return with_source_path(evaluate(*source, root).and_then(decode_campaign), path);
}
}
