#include "Sandbox/ui/utilities/ui.h"

#include "Engine/UserInterfaceSettings.h"
#include "GameFramework/PlayerController.h"

#include "Sandbox/environment/utilities/actor_utils.h"
#include "Sandbox/logging/SandboxLogCategories.h"

namespace ml {
int32 scale_font_to_umg(int32 font_size) {
    constexpr float slate_dpi{96.0f};
    constexpr float default_umg_dpi{72.0f};

    if (auto* ui_settings{GetDefault<UUserInterfaceSettings>()}) {
        auto umg_dpi{72.0f};
        if (GEngine && GEngine->GameViewport->Viewport) {
            auto const size{GEngine->GameViewport->Viewport->GetSizeXY()};
            umg_dpi = ui_settings->GetDPIScaleBasedOnSize(size);
        }

        auto const dpi_scale_factor{umg_dpi / slate_dpi};
        return FMath::RoundToInt(static_cast<float>(font_size) * dpi_scale_factor);
    }

    return font_size;
}

auto get_screen_bounds(APlayerController const& pc, AActor const& actor) -> FScreenBounds {
    auto const corners{ml::get_actor_corners(actor)};

    UE_LOG(LogSandboxUI, Verbose, TEXT("Corner data: %s"), *corners.to_string());

    auto corner_view{TArrayView<FVector const>(corners.data)};

    FVector2D top_left{1e9, 1e9};
    FVector2D bottom_right{0, 0};
    FVector2D screen_location{};

    for (auto const& corner : corners.data) {
        pc.ProjectWorldLocationToScreen(corner, screen_location);

        top_left.X = std::min(top_left.X, screen_location.X);
        top_left.Y = std::min(top_left.Y, screen_location.Y);

        bottom_right.X = std::max(bottom_right.X, screen_location.X);
        bottom_right.Y = std::max(bottom_right.Y, screen_location.Y);

        UE_LOG(LogSandboxUI,
               Verbose,
               TEXT("TL: %s, BR: %s"),
               *top_left.ToString(),
               *bottom_right.ToString());
    }

    return {top_left, bottom_right - top_left};
}
}
