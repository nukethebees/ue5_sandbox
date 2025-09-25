#include "Sandbox/utilities/ui.h"

#include "Engine/UserInterfaceSettings.h"

namespace ml {
int32 scale_font_to_umg(int32 font_size) {
    constexpr float slate_dpi{96.0f};

    if (auto* ui_settings{GetDefault<UUserInterfaceSettings>()}) {
        auto const umg_api{static_cast<float>(ui_settings->GetFontDisplayDPI())};
        auto const dpi_scale_factor{umg_api / slate_dpi};
        return FMath::RoundToInt(static_cast<float>(font_size) * dpi_scale_factor);
    }

    return font_size;
}
}
