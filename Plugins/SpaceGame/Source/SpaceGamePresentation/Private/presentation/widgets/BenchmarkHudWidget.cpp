#include <SpaceGamePresentation/presentation/widgets/BenchmarkHudWidget.h>

#include <SandboxGameShared/ui/widgets/ValueWidget.h>
#include <SpaceGamePresentation/ui/common/MenuButtonWidget.h>
#include <SpaceGamePresentation/ui/style/GameUiStyle.h>

#include <Components/Border.h>
#include <Components/TextBlock.h>
#include <Engine/World.h>
#include <TimerManager.h>

namespace ioj {
void UBenchmarkHudWidget::NativeOnInitialized() {
    Super::NativeOnInitialized();
    if (IsValid(benchmark_title)) {
        benchmark_title->SetText(INVTEXT("RUNNING"));
    }
    if (IsValid(ticks_remaining_widget)) {
        ticks_remaining_widget->set_format_spec(TEXT("TICKS REMAINING // {0}"));
    }
    if (IsValid(pause_resume_button)) {
        pause_resume_button->set_text(INVTEXT("Pause"));
        pause_resume_button->clicked.AddUObject(this, &ThisClass::handle_pause_requested);
    }
    if (IsValid(end_benchmark_button)) {
        end_benchmark_button->set_text(INVTEXT("End"));
        end_benchmark_button->clicked.AddUObject(this, &ThisClass::handle_end_requested);
    }
}

void UBenchmarkHudWidget::NativeDestruct() {
    if (IsValid(end_benchmark_button)) {
        end_benchmark_button->clicked.RemoveAll(this);
    }
    if (IsValid(pause_resume_button)) {
        pause_resume_button->clicked.RemoveAll(this);
    }
    Super::NativeDestruct();
}

void UBenchmarkHudWidget::apply_ui_style(FGameUiStyle const& style) {
    auto const& hud_style{style.hud()};
    if (IsValid(benchmark_panel)) {
        benchmark_panel->SetBrush(style.chrome().navigation_background);
    }
    if (IsValid(benchmark_title)) {
        apply_text_style(*benchmark_title, hud_style.caption_text);
    }
    if (IsValid(ticks_remaining_widget)) {
        ticks_remaining_widget->set_text_style(hud_style.secondary_text);
    }
}

void UBenchmarkHudWidget::select_button(host::BenchmarkAction action) {
    auto* target{action == host::BenchmarkAction::End ? end_benchmark_button : pause_resume_button};
    target->focus_target()->SetFocus();
}
void UBenchmarkHudWidget::present_status(presentation::LevelStatus const& state) {
    auto const paused{state.state == sim::LevelState::Paused};
    auto const running{state.state == sim::LevelState::Running};
    if (IsValid(benchmark_title)) {
        benchmark_title->SetText(paused    ? INVTEXT("PAUSED")
                                 : running ? INVTEXT("RUNNING")
                                           : INVTEXT("STOPPED"));
    }
    if (IsValid(pause_resume_button)) {
        pause_resume_button->set_text(paused ? INVTEXT("Resume") : INVTEXT("Pause"));
        pause_resume_button->SetIsEnabled(paused || running);
    }

    if (IsValid(ticks_remaining_widget)) {
        ticks_remaining_widget->SetVisibility(state.ticks_remaining.has_value()
                                                  ? ESlateVisibility::HitTestInvisible
                                                  : ESlateVisibility::Collapsed);
        if (state.ticks_remaining.has_value()) {
            ticks_remaining_widget->update(FNumberFormattingOptions::DefaultNoGrouping(),
                                           *state.ticks_remaining);
        }
    }
}

void UBenchmarkHudWidget::handle_end_requested() {
    end_requested.Broadcast();
}

void UBenchmarkHudWidget::handle_pause_requested() {
    pause_requested.Broadcast();
}

} // namespace ioj
