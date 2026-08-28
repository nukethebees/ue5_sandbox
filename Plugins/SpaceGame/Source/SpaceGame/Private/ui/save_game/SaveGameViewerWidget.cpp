#include "SpaceGame/ui/save_game/SaveGameViewerWidget.h"

#include "SpaceGame/persistence/SaveGameBrowser.h"
#include "SpaceGame/support/logging/SandboxLogCategories.h"
#include "SpaceGame/system/GameSubsystem.h"
#include "SpaceGame/ui/save_game/LevelOutcomeRowWidget.h"
#include "SpaceGame/ui/save_game/SaveGameRowWidget.h"

#include <Blueprint/WidgetTree.h>
#include <Components/Border.h>
#include <Components/Button.h>
#include <Components/ScrollBox.h>
#include <Components/TextBlock.h>
#include <Components/VerticalBox.h>
#include <Components/WidgetSwitcher.h>
#include <Engine/GameInstance.h>
#include <UObject/ConstructorHelpers.h>

namespace ml::ioj {
namespace {
auto format_duration(float const duration_seconds) -> FText {
    auto const total_seconds{FMath::Max(0, FMath::RoundToInt(duration_seconds))};
    auto const hours{total_seconds / 3600};
    auto const minutes{total_seconds % 3600 / 60};
    auto const seconds{total_seconds % 60};
    return FText::Format(NSLOCTEXT("SaveGameViewer", "DurationValue", "{0}:{1}:{2}"),
                         FText::AsNumber(hours, &FNumberFormattingOptions::DefaultNoGrouping()),
                         FText::FromString(FString::Printf(TEXT("%02d"), minutes)),
                         FText::FromString(FString::Printf(TEXT("%02d"), seconds)));
}
}

USaveGameViewerWidget::USaveGameViewerWidget(FObjectInitializer const& object_initializer)
    : Super{object_initializer} {
    static ConstructorHelpers::FClassFinder<USaveGameRowWidget> const profile_row_widget_class{
        TEXT("/SpaceGame/UI/SaveGame/WBP_SaveGameRow")};
    static ConstructorHelpers::FClassFinder<ULevelOutcomeRowWidget> const outcome_row_widget_class{
        TEXT("/SpaceGame/UI/SaveGame/WBP_LevelOutcomeRow")};
    profile_row_widget_class_ = profile_row_widget_class.Class;
    outcome_row_widget_class_ = outcome_row_widget_class.Class;
}

void USaveGameViewerWidget::NativeOnInitialized() {
    Super::NativeOnInitialized();

    if (!IsValid(profile_list) || !IsValid(profile_empty_state_text) || !IsValid(refresh_button) ||
        !IsValid(outcome_list) || !IsValid(outcome_empty_state_text) || !IsValid(report_switcher) ||
        !IsValid(empty_report_panel) || !IsValid(selected_report_panel) ||
        !IsValid(profile_name_text) || !IsValid(profile_id_text) ||
        !IsValid(profile_created_text) || !IsValid(profile_last_played_text) ||
        !IsValid(profile_duration_text) || !IsValid(profile_score_text) ||
        !IsValid(results_scroll_box) || !IsValid(result_sections_box)) {
        UE_LOG(LogSandboxUI,
               Error,
               TEXT("USaveGameViewerWidget::NativeOnInitialized: One or more bound widgets are "
                    "invalid."));
        return;
    }

    refresh_button->OnClicked.AddDynamic(this, &ThisClass::handle_refresh);
}

void USaveGameViewerWidget::NativeConstruct() {
    Super::NativeConstruct();
    rebuild_profiles();
}

void USaveGameViewerWidget::set_browser(FSaveGameBrowser& browser) {
    browser_override_ = &browser;
}

void USaveGameViewerWidget::focus_primary_action() {
    if (!profile_rows_.IsEmpty() && IsValid(profile_rows_[0])) {
        profile_rows_[0]->focus_row();
        return;
    }

    if (IsValid(refresh_button)) {
        refresh_button->SetKeyboardFocus();
    }
}

void USaveGameViewerWidget::handle_refresh() {
    auto* const browser{resolve_browser()};
    if (!browser) {
        show_empty_profiles();
        return;
    }

    browser->refresh();
    rebuild_profiles();
    focus_primary_action();
}

auto USaveGameViewerWidget::resolve_browser() -> FSaveGameBrowser* {
    if (browser_override_) {
        return browser_override_;
    }

    auto* const game_instance{GetGameInstance()};
    if (!IsValid(game_instance)) {
        UE_LOG(LogSandboxUI,
               Warning,
               TEXT("USaveGameViewerWidget::resolve_browser: Game instance is invalid."));
        return nullptr;
    }

    auto* const subsystem{game_instance->GetSubsystem<UGameSubsystem>()};
    if (!IsValid(subsystem)) {
        UE_LOG(LogSandboxUI,
               Warning,
               TEXT("USaveGameViewerWidget::resolve_browser: Game subsystem is invalid."));
        return nullptr;
    }

    return &subsystem->get_save_game_browser();
}

void USaveGameViewerWidget::rebuild_profiles() {
    if (!IsValid(profile_row_widget_class_)) {
        profile_row_widget_class_ = LoadClass<USaveGameRowWidget>(
            nullptr, TEXT("/SpaceGame/UI/SaveGame/WBP_SaveGameRow.WBP_SaveGameRow_C"));
    }
    if (!IsValid(outcome_row_widget_class_)) {
        outcome_row_widget_class_ = LoadClass<ULevelOutcomeRowWidget>(
            nullptr, TEXT("/SpaceGame/UI/SaveGame/WBP_LevelOutcomeRow.WBP_LevelOutcomeRow_C"));
    }

    if (!IsValid(profile_list) || !IsValid(profile_empty_state_text) ||
        !IsValid(profile_row_widget_class_) || !IsValid(outcome_row_widget_class_)) {
        UE_LOG(LogSandboxUI,
               Error,
               TEXT("USaveGameViewerWidget::rebuild_profiles: List dependencies are invalid."));
        show_empty_profiles();
        return;
    }

    profile_list->ClearChildren();
    profile_rows_.Reset();

    auto* const browser{resolve_browser()};
    auto const profiles{browser ? browser->get_summaries() : TConstArrayView<FSaveGameSummary>{}};
    if (profiles.IsEmpty()) {
        show_empty_profiles();
        return;
    }

    profile_empty_state_text->SetVisibility(ESlateVisibility::Collapsed);
    auto const n_profiles{profiles.Num()};
    profile_rows_.Reserve(n_profiles);
    for (int32 i{0}; i < n_profiles; ++i) {
        auto const row_name{FName{*FString::Printf(TEXT("profile_row_%d"), i)}};
        auto* const row{
            WidgetTree->ConstructWidget<USaveGameRowWidget>(profile_row_widget_class_, row_name)};
        if (!IsValid(row)) {
            UE_LOG(LogSandboxUI,
                   Warning,
                   TEXT("USaveGameViewerWidget::rebuild_profiles: Failed to create row %d."),
                   i);
            continue;
        }

        row->set_summary(profiles[i]);
        row->selected.AddUObject(this, &ThisClass::select_profile);
        row->set_selected(false);
        profile_list->AddChild(row);
        profile_rows_.Add(row);
    }

    auto const previous_selection{selected_profile_id_};
    selected_profile_id_.Reset();
    select_profile(previous_selection.IsEmpty() ? profiles[0].save_id : previous_selection);
    if (selected_profile_id_.IsEmpty()) {
        select_profile(profiles[0].save_id);
    }
}

void USaveGameViewerWidget::select_profile(FString const& save_id) {
    auto* const browser{resolve_browser()};
    if (!browser) {
        show_empty_profiles();
        return;
    }

    auto const profiles{browser->get_summaries()};
    auto const n_profiles{profiles.Num()};
    int32 selected_index{INDEX_NONE};
    for (int32 i{0}; i < n_profiles; ++i) {
        if (profiles[i].save_id == save_id) {
            selected_index = i;
            auto const profile_changed{selected_profile_id_ != save_id};
            selected_profile_id_ = save_id;
            if (profile_changed) {
                selected_outcome_id_.Reset();
            }
            show_profile(profiles[i]);
            rebuild_outcomes(profiles[i]);
            break;
        }
    }

    auto const n_rows{profile_rows_.Num()};
    for (int32 i{0}; i < n_rows; ++i) {
        if (IsValid(profile_rows_[i])) {
            profile_rows_[i]->set_selected(i == selected_index);
        }
    }
}

void USaveGameViewerWidget::rebuild_outcomes(FSaveGameSummary const& profile) {
    if (!IsValid(outcome_list) || !IsValid(outcome_empty_state_text) ||
        !IsValid(result_sections_box)) {
        UE_LOG(LogSandboxUI,
               Error,
               TEXT("USaveGameViewerWidget::rebuild_outcomes: Outcome widgets are invalid."));
        return;
    }

    outcome_list->ClearChildren();
    result_sections_box->ClearChildren();
    outcome_rows_.Reset();
    result_sections_.Reset();

    if (profile.outcomes.IsEmpty()) {
        selected_outcome_id_.Reset();
        outcome_empty_state_text->SetVisibility(ESlateVisibility::Visible);
        return;
    }

    outcome_empty_state_text->SetVisibility(ESlateVisibility::Collapsed);
    auto const n_outcomes{profile.outcomes.Num()};
    outcome_rows_.Reserve(n_outcomes);
    result_sections_.Reserve(n_outcomes);
    for (int32 i{0}; i < n_outcomes; ++i) {
        auto const& outcome{profile.outcomes[i]};
        auto const row_name{FName{*FString::Printf(TEXT("outcome_row_%d"), i)}};
        auto* const row{WidgetTree->ConstructWidget<ULevelOutcomeRowWidget>(
            outcome_row_widget_class_, row_name)};
        if (!IsValid(row)) {
            UE_LOG(LogSandboxUI,
                   Warning,
                   TEXT("USaveGameViewerWidget::rebuild_outcomes: Failed to create row %d."),
                   i);
            continue;
        }

        row->set_outcome(outcome);
        row->selected.AddUObject(this, &ThisClass::select_outcome);
        row->set_selected(false);
        outcome_list->AddChild(row);
        outcome_rows_.Add(row);
        add_result_section(outcome, i);
    }

    auto const previous_selection{selected_outcome_id_};
    selected_outcome_id_.Reset();
    select_outcome(previous_selection.IsEmpty() ? profile.outcomes[0].outcome_id
                                                : previous_selection);
    if (selected_outcome_id_.IsEmpty()) {
        select_outcome(profile.outcomes[0].outcome_id);
    }
}

void USaveGameViewerWidget::select_outcome(FString const& outcome_id) {
    auto* const browser{resolve_browser()};
    if (!browser) {
        return;
    }

    for (FSaveGameSummary const& profile : browser->get_summaries()) {
        if (profile.save_id != selected_profile_id_) {
            continue;
        }

        auto const n_outcomes{profile.outcomes.Num()};
        for (int32 i{0}; i < n_outcomes; ++i) {
            if (profile.outcomes[i].outcome_id != outcome_id) {
                continue;
            }

            selected_outcome_id_ = outcome_id;
            auto const n_rows{outcome_rows_.Num()};
            for (int32 row_index{0}; row_index < n_rows; ++row_index) {
                if (IsValid(outcome_rows_[row_index])) {
                    outcome_rows_[row_index]->set_selected(row_index == i);
                }
            }

            if (result_sections_.IsValidIndex(i) && IsValid(result_sections_[i]) &&
                IsValid(results_scroll_box)) {
                results_scroll_box->ScrollWidgetIntoView(
                    result_sections_[i], true, EDescendantScrollDestination::TopOrLeft, 0.f);
            }
            return;
        }
    }
}

void USaveGameViewerWidget::show_profile(FSaveGameSummary const& profile) {
    if (!IsValid(report_switcher) || !IsValid(selected_report_panel) ||
        !IsValid(profile_name_text) || !IsValid(profile_id_text) ||
        !IsValid(profile_created_text) || !IsValid(profile_last_played_text) ||
        !IsValid(profile_duration_text) || !IsValid(profile_score_text)) {
        UE_LOG(LogSandboxUI,
               Error,
               TEXT("USaveGameViewerWidget::show_profile: Profile widgets are invalid."));
        return;
    }

    profile_name_text->SetText(FText::FromString(profile.display_name));
    profile_id_text->SetText(FText::Format(NSLOCTEXT("SaveGameViewer", "ProfileId", "ID: {0}"),
                                           FText::FromString(profile.save_id)));
    profile_created_text->SetText(
        FText::Format(NSLOCTEXT("SaveGameViewer", "Created", "Created: {0}"),
                      FText::FromString(profile.created_at.ToString(TEXT("%Y-%m-%d %H:%M")))));
    profile_last_played_text->SetText(
        FText::Format(NSLOCTEXT("SaveGameViewer", "LastPlayed", "Last played: {0}"),
                      FText::FromString(profile.last_played_at.ToString(TEXT("%Y-%m-%d %H:%M")))));
    profile_duration_text->SetText(
        FText::Format(NSLOCTEXT("SaveGameViewer", "TotalDuration", "Total duration: {0}"),
                      format_duration(profile.total_simulation_duration_seconds)));
    profile_score_text->SetText(
        FText::Format(NSLOCTEXT("SaveGameViewer", "AggregateScore", "Aggregate score: {0}"),
                      FText::AsNumber(profile.aggregate_score)));
    report_switcher->SetActiveWidget(selected_report_panel);
}

void USaveGameViewerWidget::add_result_section(FLevelOutcomeSummary const& outcome,
                                               int32 const index) {
    auto const border_name{FName{*FString::Printf(TEXT("result_section_%d"), index)}};
    auto* const border{WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), border_name)};
    auto* const content{WidgetTree->ConstructWidget<UVerticalBox>(
        UVerticalBox::StaticClass(),
        FName{*FString::Printf(TEXT("result_section_content_%d"), index)})};
    if (!IsValid(border) || !IsValid(content)) {
        UE_LOG(LogSandboxUI,
               Warning,
               TEXT("USaveGameViewerWidget::add_result_section: Failed to create section %d."),
               index);
        return;
    }

    border->SetPadding(FMargin{16.f});
    border->SetBrushColor(FLinearColor{0.08f, 0.1f, 0.12f, 1.f});
    border->SetContent(content);

    int32 text_index{};
    auto add_text = [this, content, index, &text_index](FString const& name, FText const& text) {
        auto* const text_widget{WidgetTree->ConstructWidget<UTextBlock>(
            UTextBlock::StaticClass(),
            FName{*FString::Printf(TEXT("%s_%d_%d"), *name, index, text_index++)})};
        if (IsValid(text_widget)) {
            text_widget->SetText(text);
            text_widget->SetAutoWrapText(true);
            content->AddChild(text_widget);
        }
    };

    add_text(TEXT("result_name"), FText::FromString(outcome.display_name));
    add_text(TEXT("result_outcome"),
             FText::Format(NSLOCTEXT("SaveGameViewer", "LevelResult", "Result: {0}"),
                           FText::FromString(outcome.result)));
    add_text(
        TEXT("result_completed"),
        FText::Format(NSLOCTEXT("SaveGameViewer", "LevelCompleted", "Completed: {0}"),
                      FText::FromString(outcome.completed_at.ToString(TEXT("%Y-%m-%d %H:%M")))));
    add_text(TEXT("result_duration"),
             FText::Format(NSLOCTEXT("SaveGameViewer", "LevelDuration", "Duration: {0}"),
                           format_duration(outcome.simulation_duration_seconds)));
    add_text(TEXT("result_score"),
             FText::Format(NSLOCTEXT("SaveGameViewer", "LevelScore", "Score: {0}"),
                           FText::AsNumber(outcome.score)));

    for (FLevelOutcomeStatistic const& statistic : outcome.statistics) {
        add_text(TEXT("result_statistic"),
                 FText::Format(NSLOCTEXT("SaveGameViewer", "LevelStatistic", "{0}: {1}"),
                               FText::FromString(statistic.label),
                               FText::FromString(statistic.value)));
    }

    result_sections_box->AddChild(border);
    result_sections_.Add(border);
}

void USaveGameViewerWidget::show_empty_profiles() {
    selected_profile_id_.Reset();
    selected_outcome_id_.Reset();
    profile_rows_.Reset();
    outcome_rows_.Reset();
    result_sections_.Reset();

    if (IsValid(profile_list)) {
        profile_list->ClearChildren();
    }
    if (IsValid(outcome_list)) {
        outcome_list->ClearChildren();
    }
    if (IsValid(result_sections_box)) {
        result_sections_box->ClearChildren();
    }
    if (IsValid(profile_empty_state_text)) {
        profile_empty_state_text->SetVisibility(ESlateVisibility::Visible);
    }
    if (IsValid(outcome_empty_state_text)) {
        outcome_empty_state_text->SetVisibility(ESlateVisibility::Visible);
    }
    if (IsValid(report_switcher) && IsValid(empty_report_panel)) {
        report_switcher->SetActiveWidget(empty_report_panel);
    }
}
}
