#include "mission_processing.h"

#include <format>
#include <unordered_map>

namespace ioj::levels::authoring::detail {
namespace {
auto append_expr(ExprInput input,
                 ObjectiveDefinition& objective,
                 std::unordered_map<ObjectiveName, ExprIndex> const& names,
                 Diagnostics& errors,
                 std::string_view path) -> ExprIndex {
    if (input.reference) {
        auto const found{names.find(*input.reference)};
        if (found == names.end()) {
            errors.emplace_back(DiagnosticCode::UnknownSymbol,
                                std::string{path},
                                "Unknown objective-local expression: " + input.reference->value);
            return absent_expression;
        }
        return found->second;
    }

    auto const append_children{[&](auto& composite) {
        auto const count{input.children.size()};
        for (std::size_t i{}; i < count; ++i) {
            composite.children.push_back(append_expr(std::move(input.children[i]),
                                                     objective,
                                                     names,
                                                     errors,
                                                     std::format("{}[{}]", path, i + 1)));
        }
    }};
    if (auto* all{std::get_if<AllExpr>(&input.expression.payload)}) {
        append_children(*all);
    } else if (auto* any{std::get_if<AnyExpr>(&input.expression.payload)}) {
        append_children(*any);
    }
    auto const index{static_cast<ExprIndex>(objective.expressions.size())};
    objective.expressions.push_back(std::move(input.expression));
    return index;
}
}

auto process_mission(MissionInput input, std::string_view path)
    -> std::expected<MissionDefinition, Diagnostics> {
    MissionDefinition result;
    Diagnostics errors;
    auto const count{input.objectives.size()};
    for (std::size_t i{}; i < count; ++i) {
        auto& source{input.objectives[i]};
        auto const location{std::format("{}.objectives[{}]", path, i)};
        ObjectiveDefinition objective{.id = std::move(source.id),
                                      .title = std::move(source.title),
                                      .team = source.team,
                                      .dependencies = std::move(source.depends_on)};
        std::unordered_map<ObjectiveName, ExprIndex> names;
        auto const node_count{source.nodes.size()};
        objective.expressions.resize(node_count);
        for (std::size_t j{}; j < node_count; ++j) {
            if (!names.emplace(source.nodes[j].name, static_cast<ExprIndex>(j)).second) {
                errors.emplace_back(DiagnosticCode::DuplicateProperty,
                                    std::format("{}.nodes[{}].name", location, j),
                                    "Expression names must be unique");
            }
        }
        for (std::size_t j{}; j < node_count; ++j) {
            // Preserve aliases, including forward references and cycles for validation.
            auto const child{append_expr(std::move(source.nodes[j].value),
                                         objective,
                                         names,
                                         errors,
                                         std::format("{}.nodes[{}].value", location, j))};
            objective.expressions[j] = {AllExpr{.children = {child}}};
        }
        auto const root{
            [&](std::optional<ExprInput>& value, ObjectiveOutcome outcome, std::string_view name) {
                if (value) {
                    objective.roots[outcome] = append_expr(std::move(*value),
                                                           objective,
                                                           names,
                                                           errors,
                                                           std::format("{}.{}", location, name));
                }
            }};
        root(source.pass, ObjectiveOutcome::Passed, "pass");
        root(source.fail, ObjectiveOutcome::Failed, "fail");
        root(source.expire, ObjectiveOutcome::Expired, "expire");
        result.objectives.push_back(std::move(objective));
    }
    result.team_results = std::move(input.team_results);
    if (!errors.empty()) {
        return std::unexpected{std::move(errors)};
    }
    return result;
}
}
