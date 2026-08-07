#include "Sandbox/ui/ship_hud/MissionStatusWidget.h"

#include <Sandbox/batch_game/TestMissionManager.h>
#include <Sandbox/ui/ship_hud/ShipHealthWidget.h>
#include <Sandbox/ui/widgets/ValueWidget.h>
#include <Sandbox/utilities/enums.h>
#include <SandboxCoreEngine/uobject_utils.h>

#include <Blueprint/WidgetTree.h>
#include <Components/VerticalBox.h>

void UMissionStatusWidget::NativeConstruct() {
    Super::NativeConstruct();

    ml::fatal_if_uobject_ptrs_invalid({
        SANDBOX_NAMED_UOBJECT_PTR(mission_mode_widget),
        SANDBOX_NAMED_UOBJECT_PTR(mission_time_widget),
        SANDBOX_NAMED_UOBJECT_PTR(enemies_remaining_widget),
        SANDBOX_NAMED_UOBJECT_PTR(time_remaining_widget),
        SANDBOX_NAMED_UOBJECT_PTR(surviving_entities_box),
        SANDBOX_NAMED_UOBJECT_PTR(surviving_entity_health_widget_class),
    });

    check(WidgetTree);
}

void UMissionStatusWidget::NativePreConstruct() {
    Super::NativePreConstruct();

    mission_mode_widget->set_format_spec(mission_mode_format);
    mission_time_widget->set_format_spec(mission_time_format);
    enemies_remaining_widget->set_format_spec(enemies_remaining_format);
    time_remaining_widget->set_format_spec(time_remaining_format);

    if (IsDesignTime()) {
        TArray<FShipHealth> const preview_surviving_entity_health{FShipHealth{850, 1000},
                                                                  FShipHealth{420, 500}};
        update_values(ETestMissionMode::KillEnemiesWithinTime,
                      ETestMissionState::Running,
                      37.5f,
                      82.5f,
                      12,
                      preview_surviving_entity_health);
    }
}

void UMissionStatusWidget::update(ATestMissionManager const& mission_manager) {
    update_values(mission_manager.get_mission_mode(),
                  mission_manager.get_mission_state(),
                  mission_manager.get_mission_stopwatch(),
                  mission_manager.get_time_remaining(),
                  mission_manager.get_kills_remaining(),
                  mission_manager.get_entity_health_that_must_survive());
}

void UMissionStatusWidget::update_values(
    ETestMissionMode const mission_mode,
    ETestMissionState const mission_state,
    float const mission_time,
    float const time_remaining,
    int32 const enemies_remaining,
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

    rebuild_surviving_entity_widgets(surviving_entity_health);
}

void UMissionStatusWidget::rebuild_surviving_entity_widgets(
    TConstArrayView<FShipHealth> const health_values) {
    surviving_entities_box->ClearChildren();
    surviving_entities_box->SetVisibility(health_values.IsEmpty() ? ESlateVisibility::Collapsed
                                                                  : ESlateVisibility::Visible);

    auto* const widget_class{surviving_entity_health_widget_class.Get()};
    auto const n_health_values{health_values.Num()};
    for (int32 i{0}; i < n_health_values; ++i) {
        auto const name{FString::Printf(TEXT("surviving_entity_health_%d"), i)};
        auto* const health_widget{
            WidgetTree->ConstructWidget<UShipHealthWidget>(widget_class, *name)};
        check(health_widget);
        health_widget->set_health(health_values[i]);
        surviving_entities_box->AddChild(health_widget);
    }
}
