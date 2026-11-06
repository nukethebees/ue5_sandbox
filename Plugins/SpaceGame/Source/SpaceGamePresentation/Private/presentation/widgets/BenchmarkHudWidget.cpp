#include <SpaceGamePresentation/presentation/widgets/BenchmarkHudWidget.h>

#include <SandboxGameShared/ui/widgets/ValueWidget.h>
#include <SpaceGamePresentation/ui/common/MenuButtonWidget.h>
#include <SpaceGamePresentation/ui/style/GameUiStyle.h>

#include <Components/Border.h>
#include <Components/TextBlock.h>
#include <Engine/World.h>
#include <Input/CommonUIInputTypes.h>
#include <TimerManager.h>

TOptional<FUIInputConfig> UBenchmarkHudWidget::GetDesiredInputConfig() const {
    auto config{FUIInputConfig{
        ECommonInputMode::All, EMouseCaptureMode::NoCapture, EMouseLockMode::DoNotLock, false}};
    config.bIgnoreMoveInput = true;
    config.bIgnoreLookInput = true;
    return config;
}

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
        pause_resume_button->OnClicked().AddUObject(this, &ThisClass::handle_pause_requested);
    }
    if (IsValid(end_benchmark_button)) {
        end_benchmark_button->set_text(INVTEXT("End"));
        end_benchmark_button->OnClicked().AddUObject(this, &ThisClass::handle_end_requested);
    }
}

void UBenchmarkHudWidget::NativeDestruct() {
    clear_update_timer();
    if (IsValid(end_benchmark_button)) {
        end_benchmark_button->OnClicked().RemoveAll(this);
    }
    if (IsValid(pause_resume_button)) {
        pause_resume_button->OnClicked().RemoveAll(this);
    }
    state_source_ = {};
    Super::NativeDestruct();
}

void UBenchmarkHudWidget::apply_ui_style(ioj::FGameUiStyle const& style) {
    auto const& hud_style{style.hud()};
    if (IsValid(benchmark_panel)) {
        benchmark_panel->SetBrush(style.chrome().navigation_background);
    }
    if (IsValid(benchmark_title)) {
        ioj::apply_text_style(*benchmark_title, hud_style.caption_text);
    }
    if (IsValid(ticks_remaining_widget)) {
        ticks_remaining_widget->set_text_style(hud_style.secondary_text);
    }
}

void UBenchmarkHudWidget::set_state_source(TFunction<FRunHudState()> source) {
    clear_update_timer();
    state_source_ = MoveTemp(source);
    update_state();

    auto* const world{GetWorld()};
    if (IsValid(world)) {
        world->GetTimerManager().SetTimer(
            update_timer_, this, &ThisClass::update_state, 1.0f, true);
    }
}

void UBenchmarkHudWidget::update_state() {
    if (!state_source_) {
        return;
    }
    auto const state{state_source_()};
    auto const paused{state.state == ioj::sim::LevelState::Paused};
    auto const running{state.state == ioj::sim::LevelState::Running};
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
        ticks_remaining_widget->SetVisibility(state.ticks_remaining.IsSet()
                                                  ? ESlateVisibility::HitTestInvisible
                                                  : ESlateVisibility::Collapsed);
        if (state.ticks_remaining.IsSet()) {
            ticks_remaining_widget->update(FNumberFormattingOptions::DefaultNoGrouping(),
                                           state.ticks_remaining.GetValue());
        }
    }
}

void UBenchmarkHudWidget::handle_end_requested() {
    end_requested.Broadcast();
}

void UBenchmarkHudWidget::handle_pause_requested() {
    pause_requested.Broadcast();
    update_state();
}

void UBenchmarkHudWidget::clear_update_timer() {
    if (auto* const world{GetWorld()}; IsValid(world)) {
        world->GetTimerManager().ClearTimer(update_timer_);
    }
}
