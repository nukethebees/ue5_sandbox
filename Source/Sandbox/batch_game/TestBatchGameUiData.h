#pragma once

#include <Containers/StaticArray.h>
#include <CoreMinimal.h>
#include <Engine/DataAsset.h>

#include "Sandbox/ui/HudCrosshairDistances.h"

#include "TestBatchGameUiData.generated.h"

class UTestTeamVisualData;

class UEntityCountTableWidget;
class UMissionEntityHealthRowWidget;
class UMissionStatusWidget;
class UShipHealthWidget;
class UShipHudWidget;
class UShipSpeedWidget;
class UShipThrusterEnergyWidget;
class UDebugGraphWidget;
class UValueWidget;

USTRUCT(BlueprintType)
struct FBatchGameUiClasses {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Ship HUD")
    TSubclassOf<UEntityCountTableWidget> entity_count_table_widget_class{nullptr};

    UPROPERTY(EditAnywhere, Category = "Ship HUD")
    TSubclassOf<UMissionEntityHealthRowWidget> mission_entity_health_row_widget_class{nullptr};

    UPROPERTY(EditAnywhere, Category = "Ship HUD")
    TSubclassOf<UMissionStatusWidget> mission_status_widget_class{nullptr};

    UPROPERTY(EditAnywhere, Category = "Ship HUD")
    TSubclassOf<UShipHealthWidget> ship_health_widget_class{nullptr};

    UPROPERTY(EditAnywhere, Category = "Ship HUD")
    TSubclassOf<UShipHudWidget> ship_hud_widget_class{nullptr};

    UPROPERTY(EditAnywhere, Category = "Ship HUD")
    TSubclassOf<UShipSpeedWidget> ship_speed_widget_class{nullptr};

    UPROPERTY(EditAnywhere, Category = "Ship HUD")
    TSubclassOf<UShipThrusterEnergyWidget> ship_thruster_energy_widget_class{nullptr};

    UPROPERTY(EditAnywhere, Category = "Widgets")
    TSubclassOf<UDebugGraphWidget> debug_graph_widget_class{nullptr};

    UPROPERTY(EditAnywhere, Category = "Widgets")
    TSubclassOf<UValueWidget> value_widget_class{nullptr};
};

namespace ml::test_batch_game_ui_data {
inline auto get_data_asset_path() -> FName {
    return FName{TEXT("/Game/UI/DA_ui_data")};
}
}

USTRUCT(BlueprintType)
struct FTestBatchGameUiUpdateFrequencies {
    GENERATED_BODY()

    [[nodiscard]] auto to_array() const -> TStaticArray<float, 3> {
        return {
            player_status_update_period, entity_count_update_period, mission_status_update_period};
    }

    UPROPERTY(EditAnywhere, Category = "UI")
    float player_status_update_period{0.25f};

    UPROPERTY(EditAnywhere, Category = "UI")
    float entity_count_update_period{0.25f};

    UPROPERTY(EditAnywhere, Category = "UI")
    float mission_status_update_period{0.25f};
};

UCLASS(BlueprintType)
class SANDBOX_API UTestBatchGameUiData : public UDataAsset {
    GENERATED_BODY()
  public:
    UPROPERTY(EditAnywhere, Category = "Widget Classes")
    FBatchGameUiClasses widget_classes{};

    UPROPERTY(EditAnywhere, Category = "Update Frequencies")
    FTestBatchGameUiUpdateFrequencies update_frequencies{};

    UPROPERTY(EditAnywhere, Category = "Colours")
    TObjectPtr<UTestTeamVisualData> team_visual_data{nullptr};

    UPROPERTY(EditAnywhere, Category = "Crosshair")
    FHudCrosshairDistances crosshair_distances{};
};
