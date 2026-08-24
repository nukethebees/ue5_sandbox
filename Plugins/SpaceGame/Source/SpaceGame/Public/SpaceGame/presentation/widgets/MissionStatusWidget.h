#pragma once

#include <SpaceGame/entities/TestEntityUniqueId.h>
#include <SpaceGame/entities/TestEntityType.h>
#include <SpaceGame/missions/TestMissionMode.h>
#include <SpaceGame/missions/TestMissionState.h>
#include <SpaceGame/ships/common/ShipHealth.h>

#include <Blueprint/UserWidget.h>
#include <CoreMinimal.h>

#include "MissionStatusWidget.generated.h"

class UMissionEntityHealthRowWidget;
class UValueWidget;
class UVerticalBox;

namespace ml::hud_manager {
struct FMissionDataCache;
}

UCLASS()
class SPACEGAME_API UMissionStatusWidget : public UUserWidget {
    GENERATED_BODY()
  public:
    void set_mission_data(ml::hud_manager::FMissionDataCache const& data);
    void set_mission_state(ETestMissionState const new_state);
    void set_mission_time(float const mission_time);
    void set_enemies_remaining(int32 const enemies_remaining);
    void set_time_remaining(float const time_remaining);
    void set_font_size(int32 const new_font_size);
    auto get_font_size() const noexcept -> int32 { return font_size; }
  protected:
    void NativeConstruct() override;
    void NativePreConstruct() override;

    UPROPERTY(meta = (BindWidget))
    UValueWidget* mission_mode_widget{nullptr};

    UPROPERTY(meta = (BindWidget))
    UValueWidget* mission_time_widget{nullptr};

    UPROPERTY(meta = (BindWidget))
    UValueWidget* enemies_remaining_widget{nullptr};

    UPROPERTY(meta = (BindWidget))
    UValueWidget* time_remaining_widget{nullptr};

    UPROPERTY(meta = (BindWidget))
    UVerticalBox* surviving_entities_box{nullptr};

    UPROPERTY(meta = (BindWidget))
    UVerticalBox* required_kill_entities_box{nullptr};

    UPROPERTY(EditAnywhere, Category = "UI|Format")
    FName mission_mode_format{TEXT("{0} ({1})")};

    UPROPERTY(EditAnywhere, Category = "UI|Format")
    FName mission_time_format{TEXT("Mission time: {0}")};

    UPROPERTY(EditAnywhere, Category = "UI|Format")
    FName enemies_remaining_format{TEXT("Enemies remaining: {0}")};

    UPROPERTY(EditAnywhere, Category = "UI|Format")
    FName time_remaining_format{TEXT("Time remaining: {0}")};

    UPROPERTY(EditAnywhere, Category = "UI")
    int32 font_size{24};
  private:
    void set_mission_mode(ETestMissionMode const new_mode, ETestMissionState const initial_state);
    void set_mission_values(ETestMissionMode const mission_mode,
                            ETestMissionState const mission_state,
                            float const mission_time,
                            float const time_remaining,
                            int32 const enemies_remaining,
                            TConstArrayView<TestEntityUniqueId> const surviving_entity_ids,
                            TConstArrayView<ETestEntityType> const surviving_entity_types,
                            TConstArrayView<FShipHealth> const surviving_entity_health,
                            TConstArrayView<TestEntityUniqueId> const required_kill_entity_ids,
                            TConstArrayView<ETestEntityType> const required_kill_entity_types,
                            TConstArrayView<FShipHealth> const required_kill_entity_health);
    auto check_widget_bindings() const -> bool;
    auto update_entity_widgets(UVerticalBox& entity_box,
                               FStringView widget_name_prefix,
                               TConstArrayView<TestEntityUniqueId> entity_ids,
                               TConstArrayView<ETestEntityType> entity_types,
                               TConstArrayView<FShipHealth> health_values,
                               TArray<TestEntityUniqueId>& cached_entity_ids,
                               TArray<TObjectPtr<UMissionEntityHealthRowWidget>>& entity_widgets)
        -> bool;

    TArray<TestEntityUniqueId> surviving_entity_ids{};
    TArray<TObjectPtr<UMissionEntityHealthRowWidget>> surviving_entity_widgets{};
    TArray<TestEntityUniqueId> required_kill_entity_ids{};
    TArray<TObjectPtr<UMissionEntityHealthRowWidget>> required_kill_entity_widgets{};
    ETestMissionMode current_mission_mode{ETestMissionMode::None};
};
