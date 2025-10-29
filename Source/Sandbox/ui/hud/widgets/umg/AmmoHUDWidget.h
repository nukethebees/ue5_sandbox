#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "Sandbox/logging/mixins/LogMsgMixin.hpp"
#include "Sandbox/logging/SandboxLogCategories.h"

#include "AmmoHUDWidget.generated.h"

UCLASS()
class SANDBOX_API UAmmoHUDWidget
    : public UUserWidget
    , public ml::LogMsgMixin<"AmmoHUDWidget", LogSandboxUI> {
    GENERATED_BODY()
};
