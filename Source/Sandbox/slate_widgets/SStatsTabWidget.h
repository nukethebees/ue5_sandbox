#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

#include "Sandbox/mixins/LogMsgMixin.hpp"
#include "Sandbox/SandboxLogCategories.h"

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
