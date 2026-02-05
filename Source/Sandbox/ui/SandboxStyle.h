#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateStyle.h"

namespace ml {

/**
 * Minimal style registry using UE5's FSlateStyleSet.
 * Provides centralized text styling for Slate widgets in the Sandbox module.
 */
class SANDBOX_API SandboxStyle {
  public:
    static void initialize();
    static void shutdown();
    static ISlateStyle const& get() { return *style_set_.Get(); }
    static FName get_style_set_name();
  private:
    static TUniquePtr<FSlateStyleSet> style_set_;

    static void initialize_text_styles();
};

} // namespace ml
