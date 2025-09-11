#include "Sandbox/widgets/MainHUDWidget.h"

#define WIDGET_CHECK(widget, name_string)            \
    do {                                             \
        if (!widget) {                               \
            missing_widget_error(TEXT(name_string)); \
            return;                                  \
        }                                            \
    } while (0)

void UMainHUDWidget::update_health(FHealthData health_data) {
    WIDGET_CHECK(health_widget, "update_health_percent");
    health_widget->set_health(health_data);
}
void UMainHUDWidget::missing_widget_error(FStringView method) {
    UE_LOGFMT(LogTemp, Error, "UMainHUDWidget: {Method} no widget.", ("Method", method));
}
