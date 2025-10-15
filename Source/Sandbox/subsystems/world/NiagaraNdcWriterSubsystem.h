#pragma once

#include "CoreMinimal.h"

#include "Sandbox/mixins/log_msg_mixin.hpp"
#include "Sandbox/SandboxLogCategories.h"

#include "NiagaraNdcWriterSubsystem.generated.h"

// Subsystem for writing to niagara data channels
UCLASS()
class SANDBOX_API UNiagaraNdcWriterSubsystem
    : public UWorldSubsystem
    , public ml::LogMsgMixin<"UNiagaraNdcWriterSubsystem", LogSandboxSubsystem> {
    GENERATED_BODY()
};
