#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

#include "SandboxGameShared/logging/LogMsgMixin.hpp"
#include "ShooterGame/logging/ShooterGameLogCategories.h"

class SStatsTabWidget
    : public SCompoundWidget
    , public ml::LogMsgMixin<"SStatsTabWidget", LogShooterGameUI> {
  public:
    // clang-format off
    SLATE_BEGIN_ARGS(SStatsTabWidget) {}
    SLATE_END_ARGS()
    // clang-format on

    void Construct(FArguments const& InArgs);
};
