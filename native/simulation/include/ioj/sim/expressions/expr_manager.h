#pragma once
#include <ioj/levels/expression.h>
#include <ioj/sim/expressions/expr_compilation_context.h>
#include <ioj/sim/expressions/expr_evaluation_context.h>
#include <ioj/sim/expressions/expr_leaves.h>
#include <ioj/sim/expressions/expr_operators.h>

#include <span>
#include <vector>
namespace ioj::sim {
class ExprManager {
  public:
    auto append(std::span<levels::Expression const> expressions, ExprCompilationContext const& ctx)
        -> std::vector<ExprHandle>;
    auto add_timer_elapsed(TimerHandle timer) -> ExprHandle;
    void evaluate(ExprEvaluationContext const& inputs);
    auto result(ExprHandle expression) const -> bool;
  private:
    void evaluate_composites();
    void evaluate_leaves(ExprEvaluationContext const& inputs);
    void evaluate_destroyed(std::span<LeafHandle const> run, ExprEvaluationContext const& inputs);
    void evaluate_elapsed(std::span<LeafHandle const> run, ExprEvaluationContext const& inputs);
    void evaluate_kills(std::span<LeafHandle const> run, ExprEvaluationContext const& inputs);
    void evaluate_reached_position(std::span<LeafHandle const> run,
                                   ExprEvaluationContext const& inputs);
    ExprOperators composites_;
    ExprLeaves leaves_;
};
}
