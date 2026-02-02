#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

#include "Sandbox/logging/LogMsgMixin.hpp"
#include "Sandbox/logging/SandboxLogCategories.h"

class SStatsTabWidget
    : public SCompoundWidget
    , public ml::LogMsgMixin<"SStatsTabWidget", LogSandboxUI> {
  public:
    // clang-format off
    SLATE_BEGIN_ARGS(SStatsTabWidget) {}
    SLATE_END_ARGS()
    // clang-format on

    void Construct(FArguments const& InArgs);
};
