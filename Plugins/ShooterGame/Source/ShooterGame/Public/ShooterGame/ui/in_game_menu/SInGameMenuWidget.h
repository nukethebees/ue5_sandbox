#pragma once

#include "CoreMinimal.h"
#include "Framework/SlateDelegates.h"
#include "Widgets/SCompoundWidget.h"

#include "SandboxGameShared/logging/LogMsgMixin.hpp"
#include "ShooterGame/logging/ShooterGameLogCategories.h"
#include "ShooterGame/ui/in_game_menu/InGameMenuTab.h"

class SWidgetSwitcher;
class UInventoryComponent;

class SInGameMenuWidget
    : public SCompoundWidget
    , public ml::LogMsgMixin<"SInGameMenuWidget", LogShooterGameUI> {
  public:
    // clang-format off
    SLATE_BEGIN_ARGS(SInGameMenuWidget) {}
        SLATE_EVENT(FOnClicked, OnExitClicked)
        SLATE_ARGUMENT(UInventoryComponent*, InventoryComponent)
    SLATE_END_ARGS()
    // clang-format on

    void Construct(FArguments const& InArgs);
  private:
    TSharedPtr<SWidgetSwitcher> widget_switcher;
    FOnClicked on_exit_clicked_;
    UInventoryComponent* inventory_component_{nullptr};

    FReply on_stats_tab_clicked();
    FReply on_inventory_tab_clicked();
    void switch_to_tab(EInGameMenuTab tab);
};
