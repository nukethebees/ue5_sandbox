#pragma once

#include <ioj/levels/diagnostics.h>
#include <ioj/settings/settings.h>

#include <CoreMinimal.h>

#include <expected>

class USpaceGameLevelConfig;

// Apply presentation overrides to the runtime configuration after its Unreal resources are copied.
[[nodiscard]] SPACEGAME_API auto
    apply_presentation_settings(ioj::settings::Settings const& settings,
                                USpaceGameLevelConfig& target)
        -> std::expected<void, ioj::levels::Diagnostics>;
