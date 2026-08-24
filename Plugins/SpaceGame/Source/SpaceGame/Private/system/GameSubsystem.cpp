#include "SpaceGame/system/GameSubsystem.h"

#if PLATFORM_WINDOWS
#include "Windows/game_capabilities_windows.h"
#endif

namespace ml::ioj {
void UGameSubsystem::Initialize(FSubsystemCollectionBase& collection) {
    Super::Initialize(collection);

#if PLATFORM_WINDOWS
    platform_capabilities_ = detail::query_windows_platform_capabilities();
#endif
}

auto UGameSubsystem::get_platform_capabilities() const -> FGameCapabilities const& {
    return platform_capabilities_;
}
}
