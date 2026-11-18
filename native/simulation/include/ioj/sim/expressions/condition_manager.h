#pragma once
#include <ioj/sim/expressions/expr_leaves.h>
#include <ioj/sim/expressions/expr_operators.h>
#include <ioj/sim/expressions/latch_memory.h>

#include <span>
#include <vector>

namespace ioj::levels {
struct Expression;
}

namespace ioj::sim {
struct ExprCompilationContext;
struct ExprEvaluationContext;

struct LatchInput {
    ExprHandle when;
    ExprHandle value;
};

// Own Boolean expressions and persistent latch state.
class ConditionManager {
  public:
    /* **************************************** */
    // Compilation
    /* **************************************** */
    // Require one output handle per input expression, in input order.
    void append(std::span<levels::Expression const> expressions,
                ExprCompilationContext const& ctx,
                std::span<ExprHandle> output);
    auto add_timer_elapsed(TimerHandle timer) -> ExprHandle;
    auto add_latch(LatchInput input) -> LatchHandle;

    /* **************************************** */
    // Evaluation
    /* **************************************** */
    void commit_latches();
    // Evaluate with committed latch values, then capture next-tick latch inputs.
    void evaluate(ExprEvaluationContext const& inputs);

    /* **************************************** */
    // Inspection
    /* **************************************** */
    auto result(ExprHandle expression) const -> bool;
    auto children(ExprHandle expression) const -> std::span<ExprHandle const>;
    auto destroyed_entity(ExprHandle expression) const -> LevelEntityRef;
    auto latched_true_value(ExprHandle expression) const -> ExprHandle;
    // Return the committed state, excluding pending captures.
    auto latch_state(LatchHandle handle) const -> LatchState;

  private:
    void evaluate_latches();
    auto is_valid(LatchHandle handle) const -> bool;
    void evaluate_composites();
    void evaluate_leaves(ExprEvaluationContext const& inputs);
    void evaluate_destroyed(std::span<LeafIndex const> run, ExprEvaluationContext const& inputs);
    void evaluate_elapsed(std::span<LeafIndex const> run, ExprEvaluationContext const& inputs);
    void evaluate_kills(std::span<LeafIndex const> run, ExprEvaluationContext const& inputs);
    void evaluate_reached_position(std::span<LeafIndex const> run,
                                   ExprEvaluationContext const& inputs);

    ExprOperators composites_;
    ExprLeaves leaves_;

    std::vector<LatchInput> latch_inputs_;
    std::vector<LatchMemory> latch_states_;
    std::vector<LatchHandle> active_latches_;
};
}
