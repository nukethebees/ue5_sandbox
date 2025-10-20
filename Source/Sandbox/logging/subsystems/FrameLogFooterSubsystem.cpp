#include "Sandbox/logging/subsystems/FrameLogFooterSubsystem.h"

#include "Misc/CoreDelegates.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

void UFrameLogFooterSubsystem::Initialize(FSubsystemCollectionBase& Collection) {
    Super::Initialize(Collection);

    frame_log_tracker = MakeUnique<FFrameLogTracker>();
    RETURN_IF_NULLPTR(GLog);
    GLog->AddOutputDevice(frame_log_tracker.Get());

    FCoreDelegates::OnEndFrame.AddUObject(this, &UFrameLogFooterSubsystem::on_end_frame);

    log_display(TEXT("Frame Log Footer Subsystem Initialized."));
}

void UFrameLogFooterSubsystem::Deinitialize() {
    Super::Deinitialize();

    FCoreDelegates::OnEndFrame.RemoveAll(this);

    RETURN_IF_INVALID(frame_log_tracker);
    RETURN_IF_NULLPTR(GLog);

    GLog->RemoveOutputDevice(frame_log_tracker.Get());
}

void UFrameLogFooterSubsystem::on_end_frame() {
    if (frame_log_tracker.IsValid()) {
        if (auto const log_count{frame_log_tracker->get_and_reset_log_count() > 0}) {
            // Using GFrameCounter is generally safe here as this runs on the game thread.
            UE_LOG(LogSandboxFrameCount,
                   Display,
                   TEXT("********************** End frame %llu ********************************"),
                   GFrameCounter);
        }
    }
}
