#pragma once

#include <CommonActivatableWidget.h>

#include "MenuActivatableWidget.generated.h"

namespace ioj {
UCLASS(Abstract)
class SPACEGAME_API UMenuActivatableWidget : public UCommonActivatableWidget {
    GENERATED_BODY()
  public:
    UMenuActivatableWidget();

    FReply NativeOnKeyDown(FGeometry const& geometry, FKeyEvent const& event) override;
    TOptional<FUIInputConfig> GetDesiredInputConfig() const override;
};
}
