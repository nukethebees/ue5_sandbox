#include "mission_parser.h"

#include <limits>
#include <unordered_map>

namespace ioj::levels::authoring::detail {
namespace {
auto outcome(AstParser& parser, s7::NodeIndex const node, std::string const& path)
    -> ObjectiveOutcome {
    auto const name{parser.symbol(node, path)};
    auto const value{try_parse_serialized_objective_outcome(name)};
    if (!value) {
        parser.error(DiagnosticCode::UnknownSymbol, path, "Unknown objective outcome: " + name);
    }
    return value.value_or(ObjectiveOutcome::Passed);
}
auto dependency(AstParser& parser,
                s7::Ast const& ast,
                s7::NodeIndex const node,
                std::string const& path) -> ObjectiveDependency {
    auto const values{parser.list(node, path)};
    if (values.size() != 3 || ast.node(values[0]).kind != s7::NodeKind::Keyword ||
        ast.text(values[0]) != "objective-resolved") {
        parser.error(
            DiagnosticCode::ExpectedRecord, path, "Expected (objective-resolved 'id 'outcome)");
        return {};
    }
    return {ObjectiveName{parser.symbol(values[1], path)}, outcome(parser, values[2], path)};
}
auto position(AstParser& parser, s7::NodeIndex const node, std::string const& path) -> Vector3d {
    auto const values{parser.list(node, path)};
    if (values.size() != 3) {
        parser.error(DiagnosticCode::InvalidType, path, "Expected three position coordinates");
        return {};
    }
    return {parser.number(values[0], path),
            parser.number(values[1], path),
            parser.number(values[2], path)};
}

class ExpressionReader {
  public:
    ExpressionReader(AstParser& parser, s7::Ast const& ast, ObjectiveDefinition& objective)
        : parser_{parser}
        , ast_{ast}
        , objective_{objective} {}

    void named_nodes(s7::NodeIndex const node, std::string const& path) {
        auto const entries{parser_.list(node, path)};
        std::vector<std::pair<ExpressionIndex, s7::NodeIndex>> definitions;
        for (auto const entry : entries) {
            auto const values{parser_.list(entry, path)};
            if (values.size() != 3 || ast_.node(values[0]).kind != s7::NodeKind::Keyword ||
                ast_.text(values[0]) != "expr") {
                parser_.error(
                    DiagnosticCode::ExpectedRecord, path, "Expected (expr 'name expression)");
                continue;
            }
            auto const name{parser_.symbol(values[1], path)};
            auto const index{static_cast<ExpressionIndex>(objective_.expressions.size())};
            if (name.empty() || !names_.emplace(name, index).second) {
                parser_.error(DiagnosticCode::DuplicateProperty,
                              path,
                              "Expression names must be nonempty and unique");
                continue;
            }
            objective_.expressions.emplace_back();
            definitions.emplace_back(index, values[2]);
        }
        for (auto const [index, value] : definitions) {
            // Keep explicit aliases as one-child conjunctions, including forward references.
            auto const child{read(value, path)};
            objective_.expressions[index] = {AllExpression{.children = {child}}};
        }
    }

    auto read(s7::NodeIndex const node, std::string const& path, std::uint32_t const depth = 0)
        -> ExpressionIndex {
        if (depth > 256) {
            parser_.error(DiagnosticCode::LimitExceeded, path, "Expression nesting exceeds 256");
            return absent_expression;
        }
        auto const values{parser_.list(node, path)};
        if (values.empty() || ast_.node(values[0]).kind != s7::NodeKind::Keyword) {
            parser_.error(DiagnosticCode::ExpectedRecord, path, "Expected a mission expression");
            return absent_expression;
        }
        auto const name{ast_.text(values[0])};
        if (name == "expr-ref") {
            if (values.size() != 2) {
                parser_.error(DiagnosticCode::InvalidType, path, "expr-ref requires one name");
                return absent_expression;
            }
            auto const found{names_.find(parser_.symbol(values[1], path))};
            if (found == names_.end()) {
                parser_.error(
                    DiagnosticCode::UnknownSymbol, path, "Unknown objective-local expression");
                return absent_expression;
            }
            return found->second;
        }
        auto const type{try_parse_serialized_expression_type(name)};
        if (!type) {
            parser_.error(DiagnosticCode::UnknownSymbol,
                          path,
                          "Unknown mission expression: " + std::string{name});
            return absent_expression;
        }
        MissionExpression expression;
        auto const arity = [&](std::size_t const arguments) {
            if (values.size() != arguments + 1) {
                parser_.error(
                    DiagnosticCode::InvalidType, path, "Wrong number of condition arguments");
                return false;
            }
            return true;
        };
        switch (*type) {
            case ExpressionType::All:
            case ExpressionType::Any: {
                std::vector<ExpressionIndex> children;
                for (auto const child : values.subspan(1)) {
                    children.push_back(read(child, path, depth + 1));
                }
                if (*type == ExpressionType::All) {
                    expression.payload = AllExpression{std::move(children)};
                } else {
                    expression.payload = AnyExpression{std::move(children)};
                }
                break;
            }
            case ExpressionType::EntityDestroyed:
                if (arity(1)) {
                    expression.payload =
                        EntityDestroyedExpression{EntityId{parser_.symbol(values[1], path)}};
                }
                break;
            case ExpressionType::EntityReachedPosition:
                if (arity(3)) {
                    auto const target{
                        FiniteVector<double, 3>::try_make(position(parser_, values[2], path))};
                    auto const radius{
                        NonNegativeFinite<double>::try_make(parser_.number(values[3], path))};
                    if (!target || !radius) {
                        parser_.error(DiagnosticCode::InvalidNumber,
                                      path,
                                      "Position must be finite and radius finite and nonnegative");
                    } else {
                        expression.payload = EntityReachedPositionExpression{
                            EntityId{parser_.symbol(values[1], path)}, *target, *radius};
                    }
                }
                break;
            case ExpressionType::TimeElapsed:
                if (arity(1)) {
                    auto const seconds{
                        NonNegativeFinite<double>::try_make(parser_.number(values[1], path))};
                    if (!seconds) {
                        parser_.error(DiagnosticCode::InvalidNumber,
                                      path,
                                      "Timer must be finite and nonnegative");
                    } else {
                        expression.payload = TimeElapsedExpression{*seconds};
                    }
                }
                break;
            case ExpressionType::ObjectiveResolved:
                expression.payload =
                    ObjectiveResolvedExpression{dependency(parser_, ast_, node, path)};
                break;
            case ExpressionType::EntityKillsReached:
                if (arity(2)) {
                    auto sources{parser_.ids<EntityId>(values[1], path)};
                    auto const kills{parser_.integer(values[2], path)};
                    if (kills < 0 || static_cast<std::uint64_t>(kills) >
                                         std::numeric_limits<std::uint32_t>::max()) {
                        parser_.error(DiagnosticCode::InvalidNumber,
                                      path,
                                      "Kill target must fit a nonnegative 32-bit integer");
                    } else {
                        expression.payload = EntityKillsReachedExpression{
                            std::move(sources), static_cast<std::uint32_t>(kills)};
                    }
                }
                break;
        }
        auto const index{static_cast<ExpressionIndex>(objective_.expressions.size())};
        objective_.expressions.push_back(std::move(expression));
        return index;
    }
  private:
    AstParser& parser_;
    s7::Ast const& ast_;
    ObjectiveDefinition& objective_;
    std::unordered_map<std::string, ExpressionIndex> names_;
};

auto objective(AstParser& parser,
               s7::Ast const& ast,
               s7::NodeIndex const node,
               std::string const& path) -> ObjectiveDefinition {
    ObjectiveDefinition result;
    auto const fields{parser.record(node, RecordKind::Objective, path)};
    ExpressionReader expressions{parser, ast, result};
    for (auto const& field : fields) {
        if (field.property == Property::Nodes) {
            expressions.named_nodes(field.value, field.node_path);
        }
    }
    for (auto const& field : fields) {
        switch (field.property) {
            case Property::Id:
                result.id = ObjectiveName{parser.symbol(field.value, field.node_path)};
                break;
            case Property::Title:
                result.title = parser.string(field.value, field.node_path);
                break;
            case Property::Team:
                result.team = parser.team(field.value, field.node_path);
                break;
            case Property::Nodes:
                break;
            case Property::Pass:
                result.roots[ObjectiveOutcome::Passed] =
                    expressions.read(field.value, field.node_path);
                break;
            case Property::Fail:
                result.roots[ObjectiveOutcome::Failed] =
                    expressions.read(field.value, field.node_path);
                break;
            case Property::Expire:
                result.roots[ObjectiveOutcome::Expired] =
                    expressions.read(field.value, field.node_path);
                break;
            case Property::DependsOn:
                for (auto const value : parser.list(field.value, field.node_path)) {
                    result.dependencies.push_back(dependency(parser, ast, value, field.node_path));
                }
                break;
            default:
                parser.unknown(field);
                break;
        }
    }
    parser.require(fields, Property::Id, path);
    parser.require(fields, Property::Team, path);
    return result;
}
}

auto parse_mission(AstParser& parser,
                   s7::Ast const& ast,
                   s7::NodeIndex const node,
                   std::string const& path) -> MissionDefinition {
    MissionDefinition result;
    auto const fields{parser.record(node, RecordKind::Mission, path)};
    for (auto const& field : fields) {
        if (field.property == Property::Objectives) {
            auto const values{parser.list(field.value, field.node_path)};
            for (std::size_t index{}; index < values.size(); ++index) {
                result.objectives.push_back(objective(
                    parser, ast, values[index], std::format("{}[{}]", field.node_path, index)));
            }
        } else if (field.property == Property::TeamResults) {
            for (auto const value : parser.list(field.value, field.node_path)) {
                TeamResultDefinition mapping;
                auto const properties{
                    parser.record(value, RecordKind::TeamResult, field.node_path)};
                for (auto const& property : properties) {
                    switch (property.property) {
                        case Property::Team:
                            mapping.team = parser.team(property.value, property.node_path);
                            break;
                        case Property::When:
                            mapping.when =
                                dependency(parser, ast, property.value, property.node_path);
                            break;
                        case Property::Result: {
                            auto const name{parser.symbol(property.value, property.node_path)};
                            auto const outcome{try_parse_serialized_team_outcome(name)};
                            if (outcome) {
                                mapping.result = *outcome;
                            } else {
                                parser.error(DiagnosticCode::UnknownSymbol,
                                             property.node_path,
                                             "Unknown team outcome: " + name);
                            }
                            break;
                        }
                        default:
                            parser.unknown(property);
                            break;
                    }
                }
                for (auto const required : {Property::Team, Property::When, Property::Result}) {
                    parser.require(properties, required, field.node_path);
                }
                result.team_results.push_back(std::move(mapping));
            }
        } else {
            parser.unknown(field);
        }
    }
    parser.require(fields, Property::Objectives, path);
    return result;
}
}
