#pragma once

#include <Input/CommonUIActionRouterBase.h>

#include "InputActionRouter.generated.h"

namespace ioj {
UCLASS()
class SPACEGAME_API UInputActionRouter : public UCommonUIActionRouterBase {
    GENERATED_BODY()
  protected:
    TSharedRef<FCommonAnalogCursor> MakeAnalogCursor() const override;
};
}
