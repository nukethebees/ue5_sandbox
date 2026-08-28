#include "SpaceGame/system/GameSubsystem.h"

#include "persistence/MockSaveGameSummarySource.h"

#if PLATFORM_WINDOWS
#include "Windows/game_capabilities_windows.h"
#endif

namespace ml::ioj {
UGameSubsystem::UGameSubsystem()
    : save_game_browser_{detail::discover_mock_save_game_summaries} {}

void UGameSubsystem::Initialize(FSubsystemCollectionBase& collection) {
    Super::Initialize(collection);

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
