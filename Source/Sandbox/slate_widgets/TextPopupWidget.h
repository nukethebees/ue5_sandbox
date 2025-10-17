#pragma once

#include "CoreMinimal.h"
#include "Components/Widget.h"

#include "Sandbox/mixins/LogMsgMixin.hpp"
#include "Sandbox/SandboxLogCategories.h"

class STextPopupWidget
    : public SCompoundWidget
    , public ml::LogMsgMixin<"STextPopupWidget", LogSandboxUI> {
  public:
    // clang-format off
    SLATE_BEGIN_ARGS(STextPopupWidget) {}
        SLATE_ARGUMENT(FText, msg)
    SLATE_END_ARGS()
    // clang-format on

    void Construct(FArguments const& InArgs);
  private:
    FText msg_;
};
