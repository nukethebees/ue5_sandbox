#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/WidgetSwitcher.h"

#include "Sandbox/ui/in_game_menu/widgets/umg/InventoryMenuWidget.h"
#include "Sandbox/ui/in_game_menu/widgets/umg/LogsMenuWidget.h"
#include "Sandbox/ui/in_game_menu/widgets/umg/MapMenuWidget.h"
#include "Sandbox/ui/in_game_menu/widgets/umg/ObjectivesMenuWidget.h"
#include "Sandbox/ui/in_game_menu/widgets/umg/PowersMenuWidget.h"
#include "Sandbox/ui/in_game_menu/widgets/umg/ResearchMenuWidget.h"
#include "Sandbox/ui/in_game_menu/widgets/umg/StatsMenuWidget.h"
#include "Sandbox/ui/main_menu/delegates/CommonMenuDelegates.h"
#include "Sandbox/ui/widgets/umg/TextButtonWidget.h"

#include "InGamePlayerMenu.generated.h"

UENUM(BlueprintType)
enum class EInGameMenuTab : uint8 {
    Powers = 0,
    Stats = 1,
    Logs = 2,
    Objectives = 3,
    Map = 4,
    Research = 5,
    Inventory = 6
};

UCLASS()
class SANDBOX_API UInGamePlayerMenu : public UUserWidget {
    GENERATED_BODY()
  protected:
    virtual void NativeConstruct() override;

    UPROPERTY(meta = (BindWidget))
    UWidgetSwitcher* tab_switcher{nullptr};

    // Tab buttons
    UPROPERTY(meta = (BindWidget))
    UTextButtonWidget* powers_tab_button{nullptr};
    UPROPERTY(meta = (BindWidget))
    UTextButtonWidget* stats_tab_button{nullptr};
    UPROPERTY(meta = (BindWidget))
    UTextButtonWidget* logs_tab_button{nullptr};
    UPROPERTY(meta = (BindWidget))
    UTextButtonWidget* objectives_tab_button{nullptr};
    UPROPERTY(meta = (BindWidget))
    UTextButtonWidget* map_tab_button{nullptr};
    UPROPERTY(meta = (BindWidget))
    UTextButtonWidget* research_tab_button{nullptr};
    UPROPERTY(meta = (BindWidget))
    UTextButtonWidget* inventory_tab_button{nullptr};

    // Close button
    UPROPERTY(meta = (BindWidget))
    UTextButtonWidget* close_button{nullptr};

    // Tab content widgets
    UPROPERTY(meta = (BindWidget))
    UPowersMenuWidget* powers_tab{nullptr};
    UPROPERTY(meta = (BindWidget))
    UStatsMenuWidget* stats_tab{nullptr};
    UPROPERTY(meta = (BindWidget))
    ULogsMenuWidget* logs_tab{nullptr};
    UPROPERTY(meta = (BindWidget))
    UObjectivesMenuWidget* objectives_tab{nullptr};
    UPROPERTY(meta = (BindWidget))
    UMapMenuWidget* map_tab{nullptr};
    UPROPERTY(meta = (BindWidget))
    UResearchMenuWidget* research_tab{nullptr};
    UPROPERTY(meta = (BindWidget))
    UInventoryMenuWidget* inventory_tab{nullptr};
  public:
    // Close navigation event
    UPROPERTY(BlueprintAssignable)
    FBackRequested back_requested;
  private:
    UFUNCTION()
    void handle_powers_tab();
    UFUNCTION()
    void handle_stats_tab();
    UFUNCTION()
    void handle_logs_tab();
    UFUNCTION()
    void handle_objectives_tab();
    UFUNCTION()
    void handle_map_tab();
    UFUNCTION()
    void handle_research_tab();
    UFUNCTION()
    void handle_inventory_tab();
    UFUNCTION()
    void handle_close();

    void set_active_tab(EInGameMenuTab tab);
    void update_tab_button_states(EInGameMenuTab active_tab);

    EInGameMenuTab current_tab{EInGameMenuTab::Powers};
};
