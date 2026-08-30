#include "SpaceGame/system/GameSubsystem.h"

#include "persistence/ExistingSaveGameBrowserSource.h"

#include "SpaceGame/persistence/SpaceSaveSubsystem.h"
#include "SpaceGame/support/logging/SandboxLogCategories.h"

#include <Engine/GameInstance.h>
#include <Subsystems/SubsystemCollection.h>

#if PLATFORM_WINDOWS
#include "Windows/game_capabilities_windows.h"
#endif

namespace ml::ioj {
void UGameSubsystem::Initialize(FSubsystemCollectionBase& collection) {
    Super::Initialize(collection);

    collection.InitializeDependency(USpaceSaveSubsystem::StaticClass());
    auto* const save_subsystem{GetGameInstance()->GetSubsystem<USpaceSaveSubsystem>()};
    if (!IsValid(save_subsystem)) {
        UE_LOG(LogSandboxSubsystem,
               Error,
               TEXT("UGameSubsystem::Initialize: Space save subsystem is invalid."));
        return;
    }

    save_game_browser_ = FSaveGameBrowser{
        [save_subsystem] { return detail::discover_existing_save_profiles(*save_subsystem); },
        [save_subsystem](FString const& profile_id) {
            return detail::load_existing_save_profile(*save_subsystem, profile_id);
        }};

#if PLATFORM_WINDOWS
    platform_capabilities_ = detail::query_windows_platform_capabilities();
#endif

    save_game_browser_.refresh();
}

auto UGameSubsystem::get_platform_capabilities() const -> FGameCapabilities const& {
    return platform_capabilities_;
}

auto UGameSubsystem::get_save_game_browser() -> FSaveGameBrowser& {
    return save_game_browser_;
}
}
