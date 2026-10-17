#pragma once

#include <CommonActivatableWidget.h>

#include "MenuActivatableWidget.generated.h"

namespace ioj {
UCLASS(Abstract)
class SPACEGAME_API UMenuActivatableWidget : public UCommonActivatableWidget {
    GENERATED_BODY()
  public:
    UMenuActivatableWidget();

    TOptional<FUIInputConfig> GetDesiredInputConfig() const override;
};
}
