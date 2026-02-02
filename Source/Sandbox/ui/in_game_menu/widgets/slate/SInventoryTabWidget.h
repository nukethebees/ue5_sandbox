#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

#include "Sandbox/logging/LogMsgMixin.hpp"
#include "Sandbox/logging/SandboxLogCategories.h"

class SInventoryTabWidget
    : public SCompoundWidget
    , public ml::LogMsgMixin<"SInventoryTabWidget", LogSandboxUI> {
  public:
    // clang-format off
    SLATE_BEGIN_ARGS(SInventoryTabWidget) {}
        SLATE_ATTRIBUTE(int32, Money)
    SLATE_END_ARGS()
    // clang-format on

    void Construct(FArguments const& InArgs);
  private:
    TAttribute<int32> money_;
};
