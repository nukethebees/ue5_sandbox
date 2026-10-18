#include <ioj/layout/document.hpp>
#include <ioj/layout/editor_session.hpp>
#include <ioj/layout/planner_controller.hpp>

#include <benchmark/benchmark.h>

#include <cstdint>
#include <utility>

namespace ioj::layout::benchmarks {
enum class Operation { commit, undo, redo };

void replay_and_analyze(benchmark::State& state, Operation operation) {
    EditorSession editor;
    auto loaded{editor.load(IOJ_SOURCE_DIR "/lispb/project.lispb", "sandbox-code")};
    if (!loaded.valid()) {
        state.SkipWithError("Failed to load sandbox-code.");
        return;
    }

    auto const history_depth{state.range(0)};
    for (std::int64_t row{}; row < history_depth; ++row) {
        auto transaction{loaded.transaction()};
        transaction.set_member_count(
            "sandbox_ismc_render", "PackedTransform", "position", row % 2 == 0 ? 4 : 3);
        if (!loaded.commit(transaction)) {
            state.SkipWithError("Failed to prepare document history.");
            return;
        }
    }

    PlannerController controller{editor, std::move(loaded)};
    controller.select_declaration("sandbox_ismc_render", "PackedTransform");
    if (operation == Operation::redo) {
        controller.undo();
    }
    controller.drain();
    auto& document{*controller.document()};

    for (auto _ : state) {
        static_cast<void>(_);
        switch (operation) {
            case Operation::commit: {
                auto transaction{document.transaction()};
                transaction.set_member_count("sandbox_ismc_render",
                                             "PackedTransform",
                                             "position",
                                             history_depth % 2 == 0 ? 4 : 3);
                controller.submit(std::move(transaction));
                break;
            }
            case Operation::undo:
                controller.undo();
                break;
            case Operation::redo:
                controller.redo();
                break;
        }
        if (!controller.drain() ||
            (operation == Operation::commit && controller.last_edit_succeeded() != true)) {
            state.SkipWithError("Document operation failed.");
            break;
        }
        benchmark::DoNotOptimize(controller.analysis_count());

        // Restore the cursor outside the measurement to keep history depth fixed.
        state.PauseTiming();
        if (operation == Operation::undo) {
            controller.redo();
        } else {
            controller.undo();
        }
        auto const restored{controller.drain()};
        state.ResumeTiming();
        if (!restored) {
            state.SkipWithError("Failed to restore the history cursor.");
            break;
        }
    }
    state.SetItemsProcessed(state.iterations());
}

BENCHMARK_CAPTURE(replay_and_analyze, commit, Operation::commit)
    ->ArgName("history")
    ->Arg(1)
    ->Arg(1000)
    ->MinTime(0.1)
    ->UseRealTime()
    ->Unit(benchmark::kMillisecond);
BENCHMARK_CAPTURE(replay_and_analyze, undo, Operation::undo)
    ->ArgName("history")
    ->Arg(1)
    ->Arg(1000)
    ->MinTime(0.1)
    ->UseRealTime()
    ->Unit(benchmark::kMillisecond);
BENCHMARK_CAPTURE(replay_and_analyze, redo, Operation::redo)
    ->ArgName("history")
    ->Arg(1)
    ->Arg(1000)
    ->MinTime(0.1)
    ->UseRealTime()
    ->Unit(benchmark::kMillisecond);
}
