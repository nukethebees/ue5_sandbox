#include "Sandbox/ui/hud/widgets/umg/TargetOverlayHUDWidget.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"

#include "Sandbox/ui/data/ScreenBounds.h"
#include "Sandbox/ui/utilities/ui.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

void UTargetOverlayHUDWidget::update_target_screen_bounds(APlayerController& pc,
                                                          FActorCorners const& corners) {
    constexpr auto logger{NestedLogger<"update_target_screen_bounds">()};

    SetVisibility(ESlateVisibility::SelfHitTestInvisible);

    TRY_INIT_PTR(canvas_slot, Cast<UCanvasPanelSlot>(Slot));

    constexpr auto update_bounds{
        [](APlayerController& pc, FVector world_pos, FVector2D& pos_2d) -> void {
            constexpr bool bPlayerViewportRelative{false};
            UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(
                &pc, world_pos, pos_2d, bPlayerViewportRelative);
        }};

    auto bounds{ml::get_screen_bounds<update_bounds>(pc, corners)};

    canvas_slot->SetPosition(bounds.get_centre());
    canvas_slot->SetSize(bounds.size + outline_padding);
}
void UTargetOverlayHUDWidget::hide() {
    SetVisibility(ESlateVisibility::Collapsed);
}

void UTargetOverlayHUDWidget::NativeOnInitialized() {
    Super::NativeOnInitialized();

    corners_ = {outline_top_left, outline_top_right, outline_bottom_left, outline_bottom_right};
}
