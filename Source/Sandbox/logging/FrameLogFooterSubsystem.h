#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "Sandbox/logging/FrameLogTracker.h"
#include "Sandbox/logging/LogMsgMixin.hpp"
#include "Sandbox/logging/SandboxLogCategories.h"

#include "FrameLogFooterSubsystem.generated.h"

/**
 * Manages a frame log tracker to print a footer at the end of each frame if logs were emitted.
 */
UCLASS(Config = Game)
class SANDBOX_API UFrameLogFooterSubsystem
    : public UGameInstanceSubsystem
    , public ml::LogMsgMixin<"UFrameLogFooterSubsystem", LogSandboxSubsystem> {
    GENERATED_BODY()
  public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    virtual bool ShouldCreateSubsystem(UObject* Outer) const { return enable_subsystem; }

    UPROPERTY(Config, EditAnywhere, Category = "Config")
    bool enable_subsystem{true};
  private:
    void on_end_frame();

    TUniquePtr<FFrameLogTracker> frame_log_tracker;
};
