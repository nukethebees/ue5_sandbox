#include "SpaceGame/ui/save_game/SaveGameViewerWidget.h"

#include "SpaceGame/persistence/SaveGameBrowser.h"
#include "SpaceGame/support/logging/SandboxLogCategories.h"
#include "SpaceGame/system/GameSubsystem.h"
#include "SpaceGame/ui/save_game/SaveGameRowWidget.h"

#include <Blueprint/WidgetTree.h>
#include <Components/Button.h>
#include <Components/ScrollBox.h>
#include <Components/TextBlock.h>
#include <Components/VerticalBox.h>
#include <Components/WidgetSwitcher.h>
#include <Engine/GameInstance.h>
#include <UObject/ConstructorHelpers.h>

namespace ml::ioj {
USaveGameViewerWidget::USaveGameViewerWidget(FObjectInitializer const& object_initializer)
    : Super{object_initializer} {
    static ConstructorHelpers::FClassFinder<USaveGameRowWidget> const row_widget_class{
        TEXT("/SpaceGame/UI/SaveGame/WBP_SaveGameRow")};
    row_widget_class_ = row_widget_class.Class;
}

void USaveGameViewerWidget::NativeOnInitialized() {
    Super::NativeOnInitialized();

    if (!IsValid(save_list) || !IsValid(empty_state_text) || !IsValid(detail_switcher) ||
        !IsValid(empty_detail_panel) || !IsValid(selected_detail_panel) ||
        !IsValid(detail_name_text) || !IsValid(detail_id_text) || !IsValid(detail_scenario_text) ||
        !IsValid(detail_date_text) || !IsValid(detail_duration_text) ||
        !IsValid(detail_score_text) || !IsValid(detail_result_text) || !IsValid(refresh_button)) {
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
    rebuild_list();
}

void USaveGameViewerWidget::set_browser(FSaveGameBrowser& browser) {
    browser_override_ = &browser;
}

void USaveGameViewerWidget::focus_primary_action() {
    if (!rows_.IsEmpty() && IsValid(rows_[0])) {
        rows_[0]->focus_row();
        return;
    }

    if (IsValid(refresh_button)) {
        refresh_button->SetKeyboardFocus();
    }
}

void USaveGameViewerWidget::handle_refresh() {
    auto* const browser{resolve_browser()};
    if (!browser) {
        show_empty_state();
        return;
    }

    browser->refresh();
    rebuild_list();
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

void USaveGameViewerWidget::rebuild_list() {
    if (!IsValid(row_widget_class_)) {
        row_widget_class_ = LoadClass<USaveGameRowWidget>(
            nullptr, TEXT("/SpaceGame/UI/SaveGame/WBP_SaveGameRow.WBP_SaveGameRow_C"));
    }

    if (!IsValid(save_list) || !IsValid(empty_state_text) || !IsValid(row_widget_class_)) {
        UE_LOG(LogSandboxUI,
               Error,
               TEXT("USaveGameViewerWidget::rebuild_list: List dependencies are invalid."));
        show_empty_state();
        return;
    }

    save_list->ClearChildren();
    rows_.Reset();

    auto* const browser{resolve_browser()};
    auto const summaries{browser ? browser->get_summaries() : TConstArrayView<FSaveGameSummary>{}};
    if (summaries.IsEmpty()) {
        show_empty_state();
        return;
    }

    empty_state_text->SetVisibility(ESlateVisibility::Collapsed);
    auto const n_summaries{summaries.Num()};
    rows_.Reserve(n_summaries);
    for (int32 i{0}; i < n_summaries; ++i) {
        auto const row_name{FName{*FString::Printf(TEXT("save_game_row_%d"), i)}};
        auto* const row{
            WidgetTree->ConstructWidget<USaveGameRowWidget>(row_widget_class_, row_name)};
        if (!IsValid(row)) {
            UE_LOG(LogSandboxUI,
                   Warning,
                   TEXT("USaveGameViewerWidget::rebuild_list: Failed to create row %d."),
                   i);
            continue;
        }

        row->set_summary(summaries[i]);
        row->selected.AddUObject(this, &ThisClass::select_save);
        row->set_selected(false);
        save_list->AddChild(row);
        rows_.Add(row);
    }

    auto const previous_selection{selected_save_id_};
    selected_save_id_.Reset();
    select_save(previous_selection.IsEmpty() ? summaries[0].save_id : previous_selection);
    if (selected_save_id_.IsEmpty()) {
        select_save(summaries[0].save_id);
    }
}

void USaveGameViewerWidget::select_save(FString const& save_id) {
    auto* const browser{resolve_browser()};
    if (!browser) {
        show_empty_state();
        return;
    }

    auto const summaries{browser->get_summaries()};
    auto const n_summaries{summaries.Num()};
    int32 selected_index{INDEX_NONE};
    for (int32 i{0}; i < n_summaries; ++i) {
        if (summaries[i].save_id == save_id) {
            selected_index = i;
            selected_save_id_ = save_id;
            show_summary(summaries[i]);
            break;
        }
    }

    auto const n_rows{rows_.Num()};
    for (int32 i{0}; i < n_rows; ++i) {
        if (IsValid(rows_[i])) {
            rows_[i]->set_selected(i == selected_index);
        }
    }
}

void USaveGameViewerWidget::show_summary(FSaveGameSummary const& summary) {
    if (!IsValid(detail_switcher) || !IsValid(selected_detail_panel) ||
        !IsValid(detail_name_text) || !IsValid(detail_id_text) || !IsValid(detail_scenario_text) ||
        !IsValid(detail_date_text) || !IsValid(detail_duration_text) ||
        !IsValid(detail_score_text) || !IsValid(detail_result_text)) {
        UE_LOG(LogSandboxUI,
               Error,
               TEXT("USaveGameViewerWidget::show_summary: Detail widgets are invalid."));
        return;
    }

    auto const total_seconds{FMath::Max(0, FMath::RoundToInt(summary.simulation_duration_seconds))};
    auto const hours{total_seconds / 3600};
    auto const minutes{total_seconds % 3600 / 60};
    auto const seconds{total_seconds % 60};

    detail_name_text->SetText(FText::FromString(summary.display_name));
    detail_id_text->SetText(FText::Format(NSLOCTEXT("SaveGameViewer", "SaveId", "File: {0}"),
                                          FText::FromString(summary.save_id)));
    detail_scenario_text->SetText(
        FText::Format(NSLOCTEXT("SaveGameViewer", "Scenario", "Scenario: {0}"),
                      FText::FromName(summary.scenario_name)));
    detail_date_text->SetText(
        FText::Format(NSLOCTEXT("SaveGameViewer", "Date", "Date: {0}"),
                      FText::FromString(summary.timestamp.ToString(TEXT("%Y-%m-%d %H:%M")))));
    detail_duration_text->SetText(
        FText::Format(NSLOCTEXT("SaveGameViewer", "Duration", "Duration: {0}:{1}:{2}"),
                      FText::AsNumber(hours, &FNumberFormattingOptions::DefaultNoGrouping()),
                      FText::FromString(FString::Printf(TEXT("%02d"), minutes)),
                      FText::FromString(FString::Printf(TEXT("%02d"), seconds))));
    detail_score_text->SetText(FText::Format(NSLOCTEXT("SaveGameViewer", "Score", "Score: {0}"),
                                             FText::AsNumber(summary.score)));
    detail_result_text->SetText(FText::Format(NSLOCTEXT("SaveGameViewer", "Result", "Result: {0}"),
                                              FText::FromString(summary.result)));
    detail_switcher->SetActiveWidget(selected_detail_panel);
}

void USaveGameViewerWidget::show_empty_state() {
    selected_save_id_.Reset();
    rows_.Reset();

    if (IsValid(save_list)) {
        save_list->ClearChildren();
    }
    if (IsValid(empty_state_text)) {
        empty_state_text->SetVisibility(ESlateVisibility::Visible);
    }
    if (IsValid(detail_switcher) && IsValid(empty_detail_panel)) {
        detail_switcher->SetActiveWidget(empty_detail_panel);
    }
}
}
