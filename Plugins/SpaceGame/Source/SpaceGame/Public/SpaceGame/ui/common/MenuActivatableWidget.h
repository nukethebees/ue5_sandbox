#pragma once

#include <Blueprint/UserWidget.h>

#include "MenuActivatableWidget.generated.h"

namespace ioj {
UCLASS(Abstract)
class SPACEGAME_API UMenuActivatableWidget : public UUserWidget {
    GENERATED_BODY()
  public:
    FSimpleMulticastDelegate closed;
    void close();
    virtual auto focus_target() const
        -> UWidget* PURE_VIRTUAL(UMenuActivatableWidget::focus_target, return nullptr;);
  protected:
    virtual auto handle_back()
        -> bool PURE_VIRTUAL(UMenuActivatableWidget::handle_back, return false;);

    FReply NativeOnKeyDown(FGeometry const& geometry, FKeyEvent const& event) override;
};
}
