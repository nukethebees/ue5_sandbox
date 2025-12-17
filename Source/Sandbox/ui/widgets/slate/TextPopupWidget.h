#pragma once

#include "CoreMinimal.h"
#include "Components/Widget.h"
#include "Framework/SlateDelegates.h"
#include "Widgets/SCompoundWidget.h"

#include "Sandbox/logging/mixins/LogMsgMixin.hpp"
#include "Sandbox/logging/SandboxLogCategories.h"

class SBox;

class STextPopupWidget
    : public SCompoundWidget
    , public ml::LogMsgMixin<"STextPopupWidget", LogSandboxUI> {
  public:
    // clang-format off
    SLATE_BEGIN_ARGS(STextPopupWidget) 
        {}
        SLATE_ARGUMENT(FText, msg)
        SLATE_EVENT(FOnClicked, OnDismissed)
    SLATE_END_ARGS()
    // clang-format on

    void Construct(FArguments const& InArgs);
  protected:
    virtual FReply OnMouseButtonDown(FGeometry const& MyGeometry,
                                     FPointerEvent const& MouseEvent) override;
    virtual FReply OnMouseMove(FGeometry const& MyGeometry,
                               FPointerEvent const& MouseEvent) override;
    virtual FReply OnMouseButtonUp(FGeometry const& MyGeometry,
                                   FPointerEvent const& MouseEvent) override;
  private:
    FText msg_;
    FOnClicked on_dismissed_;
    FVector2D drag_mouse_pos_begin_;
    FVector2D drag_translation_begin_;
};
