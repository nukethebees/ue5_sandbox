#pragma once

#include "SpaceGame/missions/TestMissionState.h"
#include "SpaceGame/simulation/LevelTelemetrySnapshot.h"
#include "SpaceGame/ui/common/MenuActivatableWidget.h"
#include "SpaceGame/ui/style/GameUiStyle.h"

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
                          FLevelTelemetrySnapshot snapshot);
    void request_return_to_mission_control();
    void request_keep_operating();

    [[nodiscard]] auto get_level_display_name() const -> FString const& {
        return level_display_name_;
    }
    [[nodiscard]] auto get_stats_snapshot() const -> FLevelTelemetrySnapshot const& {
        return stats_snapshot_;
    }

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
    TSharedPtr<SLevelCompletionView> view_{};
    FGameUiStyle fallback_style_{};
    UPROPERTY(Transient)
    UGameSubsystem* game_{nullptr};
    bool action_requested_{false};
};
}
