#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "Sandbox/logging/mixins/LogMsgMixin.hpp"
#include "Sandbox/logging/SandboxLogCategories.h"
#include "Sandbox/ui/delegates/CommonMenuDelegates.h"
#include "Sandbox/ui/in_game_menu/enums/InGameMenuTab.h"

#include "InGamePlayerMenu.generated.h"

class UWidgetSwitcher;

class UInventoryComponent;

class UTextButtonWidget;
class UPowersMenuWidget;
class UStatsMenuWidget;
class ULogsMenuWidget;
class UObjectivesMenuWidget;
class UMapMenuWidget;
class UResearchMenuWidget;
class UInventoryMenuWidget;

class AMyCharacter;

UCLASS()
class SANDBOX_API UInGamePlayerMenu
    : public UUserWidget
    , public ml::LogMsgMixin<"UInGamePlayerMenu", LogSandboxUI> {
    GENERATED_BODY()
  public:
    void set_inventory(UInventoryComponent& inventory);
    void set_character(AMyCharacter& character);
    void refresh();

    // Close navigation event
    FBackRequested back_requested;
  protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

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
