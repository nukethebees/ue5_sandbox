#include "Sandbox/ui/utilities/ui.h"

#include "Engine/UserInterfaceSettings.h"

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
}
