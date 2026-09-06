#pragma once

#include "SpaceGame/entities/TeamColours.h"
#include "SpaceGame/entities/TestEntityRegistry.h"
#include "SpaceGame/presentation/widgets/ShipHudKillData.h"
#include "SpaceGame/simulation/LevelTelemetrySnapshot.h"
#include "SpaceGame/ui/common/MenuActivatableWidget.h"

#include <Input/UIActionBindingHandle.h>

#include "PauseMenuWidget.generated.h"

class UBorder;
class UInputAction;
class UNativeWidgetHost;
class UOverlay;
class UTeamEntityTableWidget;
class UTextBlock;
class UTopKillersWidget;
class UWidgetSwitcher;
class SGraphPlot;

namespace ml::ioj {
class UMenuButtonWidget;

struct FPauseMenuData {
    FLevelTelemetrySnapshot telemetry;
    FTestEntityRegistry::EntityCounts alive_per_team_and_type{};
    ml::ship_hud::FTopKillerEntries top_killers;
    ml::ship_hud::FTeamKillMatrix team_kill_matrix;
    FTeamColours team_colours;
};

DECLARE_MULTICAST_DELEGATE(FPauseReturnToLevelSelectRequested);
DECLARE_MULTICAST_DELEGATE(FPauseQuitRequested);

enum class EPauseMenuTab : uint8 {
    Overview,
    Forces,
    Combat,
    Telemetry,
    Options,
};

UCLASS()
class SPACEGAME_API UPauseMenuWidget : public UMenuActivatableWidget {
    GENERATED_BODY()
  public:
    [[nodiscard]] auto get_active_tab() const noexcept -> EPauseMenuTab { return active_tab; }
    void prepare_for_open(UInputAction& toggle_action, FPauseMenuData data);

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
    UMenuButtonWidget* forces_button{nullptr};
    UPROPERTY(meta = (BindWidget))
    UMenuButtonWidget* combat_button{nullptr};
    UPROPERTY(meta = (BindWidget))
    UMenuButtonWidget* telemetry_button{nullptr};
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
    UTextBlock* options_placeholder{nullptr};

    UPROPERTY(meta = (BindWidget))
    UBorder* overview_summary_panel{nullptr};
    UPROPERTY(meta = (BindWidget))
    UTextBlock* overview_summary_heading{nullptr};
    UPROPERTY(meta = (BindWidget))
    UTextBlock* overview_label_elapsed_time{nullptr};
    UPROPERTY(meta = (BindWidget))
    UTextBlock* overview_label_entities_spawned{nullptr};
    UPROPERTY(meta = (BindWidget))
    UTextBlock* overview_label_entities_active{nullptr};
    UPROPERTY(meta = (BindWidget))
    UTextBlock* overview_label_entities_destroyed{nullptr};
    UPROPERTY(meta = (BindWidget))
    UTextBlock* overview_label_kills{nullptr};
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
    UTextBlock* forces_counts_heading{nullptr};
    UPROPERTY(meta = (BindWidget))
    UTeamEntityTableWidget* forces_table{nullptr};

    UPROPERTY(meta = (BindWidget))
    UTextBlock* combat_top_killers_heading{nullptr};
    UPROPERTY(meta = (BindWidget))
    UTopKillersWidget* combat_top_killers_table{nullptr};
    UPROPERTY(meta = (BindWidget))
    UTextBlock* combat_team_kills_heading{nullptr};
    UPROPERTY(meta = (BindWidget))
    UTeamEntityTableWidget* combat_team_kills_table{nullptr};

    UPROPERTY(meta = (BindWidget))
    UBorder* telemetry_summary_panel{nullptr};
    UPROPERTY(meta = (BindWidget))
    UTextBlock* telemetry_summary_heading{nullptr};
    UPROPERTY(meta = (BindWidget))
    UTextBlock* telemetry_label_lasers_fired{nullptr};
    UPROPERTY(meta = (BindWidget))
    UTextBlock* telemetry_label_lasers_active{nullptr};
    UPROPERTY(meta = (BindWidget))
    UTextBlock* lasers_fired_value{nullptr};
    UPROPERTY(meta = (BindWidget))
    UTextBlock* lasers_active_value{nullptr};
    UPROPERTY(meta = (BindWidget))
    UTextBlock* telemetry_graph_heading{nullptr};
    UPROPERTY(meta = (BindWidget))
    UTextBlock* telemetry_graph_description{nullptr};
    UPROPERTY(meta = (BindWidget))
    UNativeWidgetHost* telemetry_graph_host{nullptr};
  private:
    void handle_resume();
    void handle_overview();
    void handle_forces();
    void handle_combat();
    void handle_telemetry();
    void handle_options();
    void handle_return_to_level_select();
    void handle_quit();
    void handle_toggle_action();

    void set_active_tab(EPauseMenuTab tab);
    void apply_ui_style();
    void update_views();
    void update_telemetry_graph();

    EPauseMenuTab active_tab{EPauseMenuTab::Overview};
    FPauseMenuData data_;
    TSharedPtr<SGraphPlot> telemetry_graph_;
    FLinearColor active_entity_series_color_{0.15f, 0.75f, 1.0f, 1.0f};
    FLinearColor kills_series_color_{1.0f, 0.35f, 0.15f, 1.0f};
    TWeakObjectPtr<UInputAction> toggle_action_;
    FUIActionBindingHandle toggle_action_binding_;
    bool terminal_action_requested_{false};
};
}
