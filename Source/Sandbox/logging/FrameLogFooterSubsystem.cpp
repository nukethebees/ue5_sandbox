#include "Sandbox/logging/FrameLogFooterSubsystem.h"

#include "SandboxGameShared/utilities/macros/null_checks.hpp"
#include <SpaceGame/settings/GameSettingsSubsystem.h>

#include "Misc/CoreDelegates.h"
#include <Subsystems/SubsystemCollection.h>

namespace ioj {
void UFrameLogFooterSubsystem::Initialize(FSubsystemCollectionBase& Collection) {
    Super::Initialize(Collection);

    Collection.InitializeDependency<UGameSettingsSubsystem>();
    if (!UGameSettingsSubsystem::application_settings(GetGameInstance()).frame_log_footer) {
        return;
    }
    frame_log_tracker = MakeUnique<FFrameLogTracker>();
    RETURN_IF_NULLPTR(GLog);
    GLog->AddOutputDevice(frame_log_tracker.Get());

    FCoreDelegates::OnEndFrame.AddUObject(this, &UFrameLogFooterSubsystem::on_end_frame);

    UE_LOG(LogSandboxSubsystem, Display, TEXT("Frame Log Footer Subsystem Initialized."));
}

void UFrameLogFooterSubsystem::Deinitialize() {
    Super::Deinitialize();

    FCoreDelegates::OnEndFrame.RemoveAll(this);

    if (!frame_log_tracker) {
        return;
    }
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

} // namespace ioj
