#include "Sandbox/ui/ship_hud/MissionStatusWidget.h"

#include <Sandbox/batch_game/TestMissionManager.h>
#include <Sandbox/logging/SandboxLogCategories.h>
#include <Sandbox/ui/ship_hud/MissionEntityHealthRowWidget.h>
#include <Sandbox/ui/widgets/ValueWidget.h>
#include <Sandbox/utilities/enums.h>
#include <SandboxCore/error_msg.h>
#include <SandboxCoreEngine/uobject_utils.h>

#include <Blueprint/WidgetTree.h>
#include <Components/VerticalBox.h>

void UMissionStatusWidget::NativeConstruct() {
    Super::NativeConstruct();
    check(check_widget_bindings());
    reconstruct_surviving_entity_widgets(TConstArrayView<TestEntityUniqueId>{},
                                         TConstArrayView<ETestEntityType>{},
                                         TConstArrayView<FShipHealth>{});
}

auto UMissionStatusWidget::check_widget_bindings() const -> bool {
    ml::FErrorMsg error_msg;
    if (ml::report_invalid_uobject_ptrs(
            {
                SANDBOX_NAMED_UOBJECT_PTR(mission_mode_widget),
                SANDBOX_NAMED_UOBJECT_PTR(mission_time_widget),
                SANDBOX_NAMED_UOBJECT_PTR(enemies_remaining_widget),
                SANDBOX_NAMED_UOBJECT_PTR(time_remaining_widget),
                SANDBOX_NAMED_UOBJECT_PTR(surviving_entities_box),
                SANDBOX_NAMED_UOBJECT_PTR(surviving_entity_health_row_widget_class),
                SANDBOX_NAMED_UOBJECT_PTR(WidgetTree),
            },
            error_msg)) {
        UE_LOG(LogSandboxUI,
               Error,
               TEXT("UMissionStatusWidget: Invalid widget bindings: %s"),
               *error_msg.message);
        return false;
    }

    return true;
}

void UMissionStatusWidget::NativePreConstruct() {
    Super::NativePreConstruct();
    if (!check_widget_bindings()) {
        return;
    }

    mission_mode_widget->set_format_spec(mission_mode_format);
    mission_time_widget->set_format_spec(mission_time_format);
    enemies_remaining_widget->set_format_spec(enemies_remaining_format);
    time_remaining_widget->set_format_spec(time_remaining_format);

    if (IsDesignTime()) {
        TArray<FShipHealth> const preview_surviving_entity_health{FShipHealth{850, 1000},
                                                                  FShipHealth{420, 500}};
        TArray<TestEntityUniqueId> const preview_surviving_entity_ids{{.id = 12}, {.id = 24}};
        TArray<ETestEntityType> const preview_surviving_entity_types{
            ETestEntityType::CapitalShip, ETestEntityType::CapitalShipFighter};
        update_values(ETestMissionMode::KillEnemiesWithinTime,
                      ETestMissionState::Running,
                      37.5f,
                      82.5f,
                      12,
                      preview_surviving_entity_ids,
                      preview_surviving_entity_types,
                      preview_surviving_entity_health);
    } else {
        reconstruct_surviving_entity_widgets(TConstArrayView<TestEntityUniqueId>{},
                                             TConstArrayView<ETestEntityType>{},
                                             TConstArrayView<FShipHealth>{});
    }
}

void UMissionStatusWidget::update(ATestMissionManager const& mission_manager) {
    update_values(mission_manager.get_mission_mode(),
                  mission_manager.get_mission_state(),
                  mission_manager.get_mission_stopwatch(),
                  mission_manager.get_time_remaining(),
                  mission_manager.get_kills_remaining(),
                  mission_manager.get_entity_ids_that_must_survive(),
                  mission_manager.get_entity_types_that_must_survive(),
                  mission_manager.get_entity_health_that_must_survive());
}

void UMissionStatusWidget::update_values(
    ETestMissionMode const mission_mode,
    ETestMissionState const mission_state,
    float const mission_time,
    float const time_remaining,
    int32 const enemies_remaining,
    TConstArrayView<TestEntityUniqueId> const entity_ids,
    TConstArrayView<ETestEntityType> const entity_types,
    TConstArrayView<FShipHealth> const surviving_entity_health) {
    auto const mode_name{ml::to_string_without_type_prefix(mission_mode)};
    auto const state_name{ml::to_string_without_type_prefix(mission_state)};
    mission_mode_widget->update(FStringView{mode_name}, FStringView{state_name});
    mission_time_widget->update(mission_time);

    auto const show_enemies{mission_mode == ETestMissionMode::KillEnemies ||
                            mission_mode == ETestMissionMode::KillEnemiesWithinTime};
    enemies_remaining_widget->SetVisibility(show_enemies ? ESlateVisibility::Visible
                                                         : ESlateVisibility::Collapsed);
    if (show_enemies) {
        enemies_remaining_widget->update(enemies_remaining);
    }

    auto const show_time{mission_mode == ETestMissionMode::SurviveTime ||
                         mission_mode == ETestMissionMode::KillEnemiesWithinTime};
    time_remaining_widget->SetVisibility(show_time ? ESlateVisibility::Visible
                                                   : ESlateVisibility::Collapsed);
    if (show_time) {
        time_remaining_widget->update(time_remaining);
    }

    auto entity_list_changed{surviving_entity_ids.Num() != entity_ids.Num()};
    if (!entity_list_changed) {
        auto const n_entities{entity_ids.Num()};
        for (int32 i{0}; i < n_entities; ++i) {
            if (surviving_entity_ids[i] != entity_ids[i]) {
                entity_list_changed = true;
                break;
            }
        }
    }

    if (entity_list_changed) {
        reconstruct_surviving_entity_widgets(entity_ids, entity_types, surviving_entity_health);
    }

    for (auto const unique_id : entity_ids) {
        auto const health_index{entity_ids.Find(unique_id)};
        auto const row_index{surviving_entity_ids.Find(unique_id)};
        check(health_index != INDEX_NONE);
        check(row_index != INDEX_NONE);
        surviving_entity_widgets[row_index]->set_health(surviving_entity_health[health_index]);
    }
}

void UMissionStatusWidget::reconstruct_surviving_entity_widgets(
    TConstArrayView<TestEntityUniqueId> const entity_ids,
    TConstArrayView<ETestEntityType> const entity_types,
    TConstArrayView<FShipHealth> const health_values) {
    check(entity_ids.Num() == entity_types.Num());
    check(entity_ids.Num() == health_values.Num());

    surviving_entities_box->ClearChildren();
    surviving_entity_ids.Reset(entity_ids.Num());
    surviving_entity_widgets.Reset(entity_ids.Num());
    surviving_entities_box->SetVisibility(entity_ids.IsEmpty() ? ESlateVisibility::Collapsed
                                                               : ESlateVisibility::Visible);

    if (!surviving_entity_health_row_widget_class) {
        UE_LOG(LogSandboxUI,
               Error,
               TEXT("UMissionStatusWidget: Surviving entity health row widget class is null."));
        return;
    }

    auto* const widget_class{surviving_entity_health_row_widget_class.Get()};
    auto const n_entities{entity_ids.Num()};
    for (int32 i{0}; i < n_entities; ++i) {
        auto const name{FString::Printf(TEXT("surviving_entity_health_%d"), i)};
        auto* const health_widget{
            WidgetTree->ConstructWidget<UMissionEntityHealthRowWidget>(widget_class, *name)};
        check(health_widget);
        health_widget->set_entity(entity_ids[i], entity_types[i]);
        health_widget->set_health(health_values[i]);
        surviving_entities_box->AddChild(health_widget);
        surviving_entity_ids.Add(entity_ids[i]);
        surviving_entity_widgets.Add(health_widget);
    }
}
