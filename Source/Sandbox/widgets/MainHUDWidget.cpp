#include "Sandbox/widgets/MainHUDWidget.h"

void UMainHUDWidget::update_fuel(float new_fuel) {
    if (!fuel_widget) {
        missing_widget_error(TEXT("update_fuel"));
        return;
    }
    fuel_widget->update_fuel(new_fuel);
}

void UMainHUDWidget::update_jump(int32 new_jump) {
    if (!jump_widget) {
        missing_widget_error(TEXT("update_jump"));
        return;
    }
    jump_widget->update_jump(new_jump);
}
void UMainHUDWidget::update_coin(int32 new_coin_count) {
    if (!coin_widget) {
        missing_widget_error(TEXT("update_coin"));
        return;
    }
    coin_widget->update_coin(new_coin_count);
}
void UMainHUDWidget::missing_widget_error(FStringView method) {
    UE_LOGFMT(LogTemp, Error, "UMainHUDWidget: {Method} no widget.", ("Method", method));
}
