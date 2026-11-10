#pragma once

#include "SpaceGame/levels/LevelOutcome.h"
#include "SpaceGame/ui/common/MenuActivatableWidget.h"
#include "SpaceGamePresentation/ui/style/GameUiStyle.h"
#include <SpaceGame/levels/LevelCompletion.h>

#include "LevelCompletionWidget.generated.h"

class UNativeWidgetHost;

namespace ioj {
class SLevelCompletionView;
class UGameSubsystem;

DECLARE_MULTICAST_DELEGATE(FReturnToLevelSelectRequested);

UCLASS()
class SPACEGAME_API ULevelCompletionWidget : public UMenuActivatableWidget {
    GENERATED_BODY()
  public:
    ULevelCompletionWidget();

    void prepare_for_open(FLevelCompletion completion);
    void request_return_to_mission_control();
    auto completion() const -> FLevelCompletion const& { return completion_; }

    [[nodiscard]] auto get_level_display_name() const -> FString const& {
        return completion_.level_display_name;
    }
    [[nodiscard]] auto get_par_time_seconds() const -> TOptional<float> const& {
        return completion_.par_time_seconds;
    }
    [[nodiscard]] auto is_new_best_time() const -> bool { return completion_.new_best_time; }

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

    FLevelCompletion completion_;
    TSharedPtr<SLevelCompletionView> view_{};
    FGameUiStyle fallback_style_{};
    UPROPERTY(Transient)
    UGameSubsystem* game_{nullptr};
    bool action_requested_{false};
};
}
