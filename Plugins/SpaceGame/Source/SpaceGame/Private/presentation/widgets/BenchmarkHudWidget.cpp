#include <SpaceGame/presentation/widgets/BenchmarkHudWidget.h>

#include <SandboxGameShared/ui/widgets/ValueWidget.h>
#include <SpaceGame/simulation/TestBatchOrchestrator.h>
#include <SpaceGame/ui/common/MenuButtonWidget.h>
#include <SpaceGame/ui/style/GameUiStyle.h>

#include <Components/Border.h>
#include <Components/TextBlock.h>
#include <Engine/World.h>
#include <TimerManager.h>

void UBenchmarkHudWidget::NativeOnInitialized() {
    Super::NativeOnInitialized();
    if (IsValid(benchmark_title)) {
        benchmark_title->SetText(INVTEXT("BENCHMARK RUNNING"));
    }
    if (IsValid(ticks_remaining_widget)) {
        ticks_remaining_widget->set_format_spec(TEXT("TICKS REMAINING // {0}"));
    }
    if (IsValid(end_benchmark_button)) {
        end_benchmark_button->set_text(INVTEXT("End Benchmark"));
        end_benchmark_button->OnClicked().AddUObject(this, &ThisClass::handle_end_requested);
    }
}

void UBenchmarkHudWidget::NativeDestruct() {
    clear_update_timer();
    if (IsValid(end_benchmark_button)) {
        end_benchmark_button->OnClicked().RemoveAll(this);
    }
    orchestrator_.Reset();
    Super::NativeDestruct();
}

void UBenchmarkHudWidget::apply_ui_style(ml::ioj::FGameUiStyle const& style) {
    auto const& hud_style{style.hud()};
    if (IsValid(benchmark_panel)) {
        benchmark_panel->SetBrush(style.chrome().navigation_background);
    }
    if (IsValid(benchmark_title)) {
        ml::ioj::apply_text_style(*benchmark_title, hud_style.caption_text);
    }
    if (IsValid(ticks_remaining_widget)) {
        ticks_remaining_widget->set_text_style(hud_style.secondary_text);
    }
}

void UBenchmarkHudWidget::set_orchestrator(ATestBatchOrchestrator& orchestrator) {
    clear_update_timer();
    orchestrator_ = &orchestrator;
    update_ticks_remaining();

    auto* const world{GetWorld()};
    if (IsValid(world)) {
        world->GetTimerManager().SetTimer(
            update_timer_, this, &ThisClass::update_ticks_remaining, 1.0f, true);
    }
}

void UBenchmarkHudWidget::update_ticks_remaining() {
    auto* const orchestrator{orchestrator_.Get()};
    if (!IsValid(orchestrator) || !IsValid(ticks_remaining_widget)) {
        return;
    }
    auto const ticks_remaining{orchestrator->get_benchmark_ticks_remaining()};
    if (ticks_remaining.IsSet()) {
        ticks_remaining_widget->update(FNumberFormattingOptions::DefaultNoGrouping(),
                                       ticks_remaining.GetValue());
    }
}

void UBenchmarkHudWidget::handle_end_requested() {
    end_requested.Broadcast();
}

void UBenchmarkHudWidget::clear_update_timer() {
    if (auto* const world{GetWorld()}; IsValid(world)) {
        world->GetTimerManager().ClearTimer(update_timer_);
    }
}
