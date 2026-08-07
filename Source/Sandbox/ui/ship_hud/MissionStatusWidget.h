#pragma once

#include <Sandbox/batch_game/TestMissionMode.h>
#include <Sandbox/batch_game/TestMissionState.h>
#include <Sandbox/health/ShipHealth.h>

#include <Blueprint/UserWidget.h>
#include <CoreMinimal.h>

#include "MissionStatusWidget.generated.h"

class ATestMissionManager;
class UShipHealthWidget;
class UValueWidget;
class UVerticalBox;

UCLASS()
class SANDBOX_API UMissionStatusWidget : public UUserWidget {
    GENERATED_BODY()
  public:
    void update(ATestMissionManager const& mission_manager);
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

    UPROPERTY(EditAnywhere, Category = "UI")
    TSubclassOf<UShipHealthWidget> surviving_entity_health_widget_class;

    UPROPERTY(EditAnywhere, Category = "UI|Format")
    FName mission_mode_format{TEXT("{0} ({1})")};

    UPROPERTY(EditAnywhere, Category = "UI|Format")
    FName mission_time_format{TEXT("Mission time: {0}")};

    UPROPERTY(EditAnywhere, Category = "UI|Format")
    FName enemies_remaining_format{TEXT("Enemies remaining: {0}")};

    UPROPERTY(EditAnywhere, Category = "UI|Format")
    FName time_remaining_format{TEXT("Time remaining: {0}")};
  private:
    void update_values(ETestMissionMode mission_mode,
                       ETestMissionState mission_state,
                       float mission_time,
                       float time_remaining,
                       int32 enemies_remaining,
                       TConstArrayView<FShipHealth> surviving_entity_health);
    void rebuild_surviving_entity_widgets(TConstArrayView<FShipHealth> health_values);
};
