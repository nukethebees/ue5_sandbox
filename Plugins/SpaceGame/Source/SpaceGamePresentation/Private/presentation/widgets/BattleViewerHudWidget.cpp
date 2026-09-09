#include <SpaceGamePresentation/presentation/widgets/BattleViewerHudWidget.h>

#include <SandboxGameShared/ui/widgets/ValueWidget.h>
#include <SpaceGamePresentation/ui/style/GameUiStyle.h>

#include <Components/TextBlock.h>

void UBattleViewerHudWidget::NativeOnInitialized() {
    Super::NativeOnInitialized();
    if (IsValid(controls_label)) {
        controls_label->SetText(INVTEXT(
            "WASD move  //  Q/E vertical  //  RMB toggle look  //  wheel speed  //  Shift boost"));
    }
    if (IsValid(camera_speed_widget)) {
        camera_speed_widget->set_format_spec(TEXT("CAMERA SPEED // {0}"));
    }
}

void UBattleViewerHudWidget::apply_ui_style(ml::ioj::FGameUiStyle const& style) {
    Super::apply_ui_style(style);
    auto const& hud_style{style.hud()};
    if (IsValid(controls_label)) {
        ml::ioj::apply_text_style(*controls_label, hud_style.caption_text);
    }
    if (IsValid(camera_speed_widget)) {
        camera_speed_widget->set_text_style(hud_style.secondary_text);
    }
}

void UBattleViewerHudWidget::set_movement_speed(float const speed) {
    if (IsValid(camera_speed_widget)) {
        camera_speed_widget->update(FMath::RoundToInt(speed));
    }
}
