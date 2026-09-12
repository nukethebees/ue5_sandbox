#pragma once

#include "SpaceGame/ui/common/MenuActivatableWidget.h"
#include "SpaceGamePresentation/ui/style/GameUiStyle.h"
#include "SpaceGameSimulation/missions/TestMissionState.h"
#include "SpaceGameSimulation/simulation/LevelTelemetrySnapshot.h"

#include "LevelCompletionWidget.generated.h"

class UNativeWidgetHost;

namespace ml::ioj {
class SLevelCompletionView;
class UGameSubsystem;

DECLARE_MULTICAST_DELEGATE(FReturnToLevelSelectRequested);

UCLASS()
class SPACEGAME_API ULevelCompletionWidget : public UMenuActivatableWidget {
    GENERATED_BODY()
  public:
    ULevelCompletionWidget();

    void prepare_for_open(FString level_display_name,
                          ETestMissionState state,
                          FLevelTelemetrySnapshot snapshot,
                          TOptional<float> par_time_seconds = NullOpt,
                          bool new_best_time = false);
    void request_return_to_mission_control();
    void request_keep_operating();

    [[nodiscard]] auto get_level_display_name() const -> FString const& {
        return level_display_name_;
    }
    [[nodiscard]] auto get_stats_snapshot() const -> FLevelTelemetrySnapshot const& {
        return stats_snapshot_;
    }
    [[nodiscard]] auto get_par_time_seconds() const -> TOptional<float> const& {
        return par_time_seconds_;
    }
    [[nodiscard]] auto is_new_best_time() const -> bool { return new_best_time_; }

    FReturnToLevelSelectRequested return_to_level_select_requested;
  protected:
    void NativeOnInitialized() override;
    auto NativeGetDesiredFocusTarget() const -> UWidget* override;
    auto NativeOnHandleBackAction() -> bool override;
    auto NativeOnFocusReceived(FGeometry const& geometry, FFocusEvent const& focus_event)
        -> FReply override;
    void ReleaseSlateResources(bool release_children) override;

    UPROPERTY(meta = (BindWidget))
    UNativeWidgetHost* view_host{nullptr};
  private:
    void publish_report();

    FString level_display_name_{};
    ETestMissionState mission_state_{ETestMissionState::Succeeded};
    FLevelTelemetrySnapshot stats_snapshot_{};
    TOptional<float> par_time_seconds_{};
    TSharedPtr<SLevelCompletionView> view_{};
    FGameUiStyle fallback_style_{};
    UPROPERTY(Transient)
    UGameSubsystem* game_{nullptr};
    bool new_best_time_{false};
    bool action_requested_{false};
};
}
