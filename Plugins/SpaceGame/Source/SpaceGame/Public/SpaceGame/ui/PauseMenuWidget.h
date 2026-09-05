#pragma once

#include "SpaceGame/simulation/LevelTelemetrySnapshot.h"
#include "SpaceGame/ui/common/MenuActivatableWidget.h"

#include <Input/UIActionBindingHandle.h>

#include "PauseMenuWidget.generated.h"

class UInputAction;
class UBorder;
class UNativeWidgetHost;
class UOverlay;
class UTextBlock;
class UWidgetSwitcher;
class SGraphPlot;

namespace ml::ioj {
class UMenuButtonWidget;

DECLARE_MULTICAST_DELEGATE(FPauseReturnToLevelSelectRequested);
DECLARE_MULTICAST_DELEGATE(FPauseQuitRequested);

enum class EPauseMenuTab : uint8 {
    Overview,
    Stats,
    Options,
};

UCLASS()
class SPACEGAME_API UPauseMenuWidget : public UMenuActivatableWidget {
    GENERATED_BODY()
  public:
    [[nodiscard]] auto get_active_tab() const noexcept -> EPauseMenuTab { return active_tab; }
    void prepare_for_open(UInputAction& toggle_action, FLevelTelemetrySnapshot snapshot);

    FPauseReturnToLevelSelectRequested return_to_level_select_requested;
    FPauseQuitRequested quit_requested;
  protected:
    void NativeOnInitialized() override;
    void NativeConstruct() override;
    void NativeOnActivated() override;
    void NativeOnDeactivated() override;
    auto NativeGetDesiredFocusTarget() const -> UWidget* override;
    void ReleaseSlateResources(bool release_children) override;

    UPROPERTY(meta = (BindWidget, GeneratorRoot))
    UOverlay* root_widget{nullptr};

    UPROPERTY(meta = (BindWidget))
    UMenuButtonWidget* resume_button{nullptr};
    UPROPERTY(meta = (BindWidget))
    UMenuButtonWidget* overview_button{nullptr};
    UPROPERTY(meta = (BindWidget))
    UMenuButtonWidget* stats_button{nullptr};
    UPROPERTY(meta = (BindWidget))
    UMenuButtonWidget* options_button{nullptr};
    UPROPERTY(meta = (BindWidget))
    UMenuButtonWidget* return_to_level_select_button{nullptr};
    UPROPERTY(meta = (BindWidget))
    UMenuButtonWidget* quit_button{nullptr};

    UPROPERTY(meta = (BindWidget))
    UTextBlock* page_heading{nullptr};
    UPROPERTY(meta = (BindWidget))
    UTextBlock* paused_heading{nullptr};
    UPROPERTY(meta = (BindWidget))
    UWidgetSwitcher* page_switcher{nullptr};
    UPROPERTY(meta = (BindWidget))
    UTextBlock* overview_placeholder{nullptr};
    UPROPERTY(meta = (BindWidget))
    UTextBlock* options_placeholder{nullptr};

    UPROPERTY(meta = (BindWidget))
    UBorder* stats_summary_panel{nullptr};
    UPROPERTY(meta = (BindWidget))
    UTextBlock* stats_summary_heading{nullptr};
    UPROPERTY(meta = (BindWidget))
    UTextBlock* stats_label_elapsed_time{nullptr};
    UPROPERTY(meta = (BindWidget))
    UTextBlock* stats_label_entities_spawned{nullptr};
    UPROPERTY(meta = (BindWidget))
    UTextBlock* stats_label_entities_active{nullptr};
    UPROPERTY(meta = (BindWidget))
    UTextBlock* stats_label_entities_destroyed{nullptr};
    UPROPERTY(meta = (BindWidget))
    UTextBlock* stats_label_kills{nullptr};
    UPROPERTY(meta = (BindWidget))
    UTextBlock* stats_label_lasers_fired{nullptr};
    UPROPERTY(meta = (BindWidget))
    UTextBlock* stats_label_lasers_active{nullptr};
    UPROPERTY(meta = (BindWidget))
    UTextBlock* elapsed_time_value{nullptr};
    UPROPERTY(meta = (BindWidget))
    UTextBlock* entities_spawned_value{nullptr};
    UPROPERTY(meta = (BindWidget))
    UTextBlock* entities_active_value{nullptr};
    UPROPERTY(meta = (BindWidget))
    UTextBlock* entities_destroyed_value{nullptr};
    UPROPERTY(meta = (BindWidget))
    UTextBlock* kills_value{nullptr};
    UPROPERTY(meta = (BindWidget))
    UTextBlock* lasers_fired_value{nullptr};
    UPROPERTY(meta = (BindWidget))
    UTextBlock* lasers_active_value{nullptr};
    UPROPERTY(meta = (BindWidget))
    UTextBlock* stats_graph_heading{nullptr};
    UPROPERTY(meta = (BindWidget))
    UTextBlock* stats_graph_description{nullptr};
    UPROPERTY(meta = (BindWidget))
    UNativeWidgetHost* stats_graph_host{nullptr};
  private:
    void handle_resume();
    void handle_overview();
    void handle_stats();
    void handle_options();
    void handle_return_to_level_select();
    void handle_quit();
    void handle_toggle_action();

    void set_active_tab(EPauseMenuTab tab);
    void apply_ui_style();
    void update_stats_view();
    void update_stats_graph();
    static auto format_elapsed_time(double elapsed_seconds) -> FText;

    EPauseMenuTab active_tab{EPauseMenuTab::Overview};
    FLevelTelemetrySnapshot stats_snapshot_;
    TSharedPtr<SGraphPlot> stats_graph_;
    FLinearColor active_entity_series_color_{0.15f, 0.75f, 1.0f, 1.0f};
    FLinearColor kills_series_color_{1.0f, 0.35f, 0.15f, 1.0f};
    TWeakObjectPtr<UInputAction> toggle_action_;
    FUIActionBindingHandle toggle_action_binding_;
    bool terminal_action_requested_{false};
};
}
