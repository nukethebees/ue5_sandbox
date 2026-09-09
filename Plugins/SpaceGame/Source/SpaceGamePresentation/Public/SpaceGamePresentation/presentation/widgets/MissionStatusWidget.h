#pragma once

#include <SpaceGamePresentation/ui/style/GameUiStyle.h>
#include <SpaceGameSimulation/missions/TestMissionMode.h>
#include <SpaceGameSimulation/missions/TestMissionState.h>

#include <Blueprint/UserWidget.h>
#include <CoreMinimal.h>

#include "MissionStatusWidget.generated.h"

class UValueWidget;

namespace ml::hud_manager {
struct FMissionDataCache;
}

UCLASS()
class SPACEGAMEPRESENTATION_API UMissionStatusWidget : public UUserWidget {
    GENERATED_BODY()
  public:
    void set_mission_data(ml::hud_manager::FMissionDataCache const& data);
    void set_mission_state(ETestMissionState const new_state);
    void set_mission_time(float const mission_time);
    void set_enemies_remaining(int32 const enemies_remaining);
    void set_time_remaining(float const time_remaining);
    void apply_hud_style(ml::ioj::FGameHudStyle const& style);
    void set_font_size(int32 const new_font_size);
    auto get_font_size() const noexcept -> int32 { return font_size; }
  protected:
    void NativeConstruct() override;
    void NativePreConstruct() override;
    auto RebuildWidget() -> TSharedRef<SWidget> override;

    UPROPERTY(meta = (BindWidget))
    UValueWidget* mission_mode_widget{nullptr};

    UPROPERTY(meta = (BindWidget))
    UValueWidget* mission_time_widget{nullptr};

    UPROPERTY(meta = (BindWidget))
    UValueWidget* enemies_remaining_widget{nullptr};

    UPROPERTY(meta = (BindWidget))
    UValueWidget* time_remaining_widget{nullptr};

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
                            int32 const enemies_remaining);
    auto check_widget_bindings() const -> bool;
    void apply_mission_state_style(ETestMissionState state);
    ETestMissionMode current_mission_mode{ETestMissionMode::None};
    TOptional<ml::ioj::FGameHudStyle> hud_style_{};
};
