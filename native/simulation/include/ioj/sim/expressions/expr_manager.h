#pragma once

#include <ioj/levels/mission_expr.h>
#include <ioj/sim/expressions/expr_compilation_context.h>
#include <ioj/sim/expressions/expr_evaluation_context.h>
#include <ioj/sim/expressions/expr_leaves.h>
#include <ioj/sim/expressions/expr_operators.h>

#include <cassert>
#include <expected>
#include <span>
#include <string>
#include <vector>

namespace ioj::sim {
class Mission;

class ExprManager {
  public:
    struct Compilation {
        std::vector<ExprHandle> handles;
        std::vector<TimerHandle> timers;
        IndexSpan leaves;
        IndexSpan composites;
    };

    auto append(std::span<levels::MissionExpr const> expressions, ExprCompilationContext const& ctx)
        -> std::expected<Compilation, std::vector<std::string>>;

    void reset_progress();
    void activate(ExprHandle root);
    void deactivate(ExprHandle root);
    void activate(IndexSpan leaves, IndexSpan composites);
    void deactivate(IndexSpan leaves, IndexSpan composites);
    void evaluate(ExprEvaluationContext const& inputs, Mission const& mission);

    auto leaves() const -> ExprLeaves const& { return leaves_; }
    auto result(ExprHandle expression) const -> ExprResult;

    // Visit the root before its descendants. Absent roots are empty; shared nodes may repeat.
    template <typename Visitor>
    void visit_expression_tree(ExprHandle const expression, Visitor&& visitor) const {
        if (expression == ExprHandle{}) {
            return;
        }
        assert(expression.is_valid());

        visitor(expression);
        if (expression.kind() == ExprKind::Leaf) {
            return;
        }

        auto const index{expression.data_composite_index()};
        assert(index < composites_.child_ranges.size());
        auto const range{composites_.child_ranges[index]};
        auto const children{std::span{composites_.children}.subspan(range.offset, range.count)};
        for (auto const child : children) {
            visit_expression_tree(child, visitor);
        }
    }

  private:
    void evaluate_composites();
    void resolve_leaf(LeafIndex index, ExprResult value);
    void evaluate_leaves(ExprEvaluationContext const& inputs, Mission const& mission);
    void evaluate_destroyed(std::span<LeafHandle const> run, ExprEvaluationContext const& inputs);
    void evaluate_elapsed(std::span<LeafHandle const> run, ExprEvaluationContext const& inputs);
    void evaluate_resolved(std::span<LeafHandle const> run, Mission const& mission);
    void evaluate_kills(std::span<LeafHandle const> run, ExprEvaluationContext const& inputs);
    void evaluate_reached_position(std::span<LeafHandle const> run,
                                   ExprEvaluationContext const& inputs);

    ExprOperators composites_;
    ExprLeaves leaves_;
};
}
