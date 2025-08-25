#include "Sandbox/widgets/MainHUDWidget.h"

#define WIDGET_CHECK(widget, name_string)            \
    do {                                             \
        if (!widget) {                               \
            missing_widget_error(TEXT(name_string)); \
            return;                                  \
        }                                            \
    } while (0)

void UMainHUDWidget::update_fuel(float new_fuel) {
    WIDGET_CHECK(fuel_widget, "update_fuel");
    fuel_widget->update_fuel(new_fuel);
}

void UMainHUDWidget::update_jump(int32 new_jump) {
    WIDGET_CHECK(jump_widget, "update_jump");
    jump_widget->update_jump(new_jump);
}
void UMainHUDWidget::update_coin(int32 new_coin_count) {
    WIDGET_CHECK(coin_widget, "update_coin");
    coin_widget->update_coin(new_coin_count);
}
void UMainHUDWidget::update_health(FHealthData health_data) {
    WIDGET_CHECK(health_widget, "update_health_percent");
    health_widget->set_health(health_data);
}
void UMainHUDWidget::missing_widget_error(FStringView method) {
    UE_LOGFMT(LogTemp, Error, "UMainHUDWidget: {Method} no widget.", ("Method", method));
}
