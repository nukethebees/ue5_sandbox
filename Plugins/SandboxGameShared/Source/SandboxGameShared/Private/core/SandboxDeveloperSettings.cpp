#include "SandboxGameShared/core/SandboxDeveloperSettings.h"

USandboxDeveloperSettings::USandboxDeveloperSettings() {
    CategoryName = "Sandbox";
    SectionName = "Developer Settings";
}

auto USandboxDeveloperSettings::get_effective_max_live_fighters() const noexcept -> int32 {
    return FMath::Max(minimum_max_live_fighters, max_live_fighters);
}
