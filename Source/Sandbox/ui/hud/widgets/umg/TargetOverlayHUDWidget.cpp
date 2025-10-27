#include "Sandbox/ui/hud/widgets/umg/TargetOverlayHUDWidget.h"

#include "Blueprint/SlateBlueprintLibrary.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Kismet/GameplayStatics.h"

#include "Sandbox/ui/data/ScreenBounds.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

void UTargetOverlayHUDWidget::update_target_screen_bounds(FScreenBounds const& bounds) {
    constexpr auto logger{NestedLogger<"update_target_screen_bounds">()};

    SetVisibility(ESlateVisibility::SelfHitTestInvisible);

    TRY_INIT_PTR(canvas_slot, Cast<UCanvasPanelSlot>(Slot));

    auto* pc{UGameplayStatics::GetPlayerController(GetWorld(), 0)};
    FVector2D viewport_position_2d;
    FVector2D viewport_size_2d;

    auto const padded_size{bounds.size + outline_padding};

    USlateBlueprintLibrary::ScreenToViewport(pc, bounds.get_centre(), viewport_position_2d);
    USlateBlueprintLibrary::ScreenToViewport(pc, bounds.size + outline_padding, viewport_size_2d);

    canvas_slot->SetPosition(viewport_position_2d);
    canvas_slot->SetSize(viewport_size_2d);
}

void UTargetOverlayHUDWidget::NativeOnInitialized() {
    Super::NativeOnInitialized();

    corners_ = {outline_top_left, outline_top_right, outline_bottom_left, outline_bottom_right};
}
