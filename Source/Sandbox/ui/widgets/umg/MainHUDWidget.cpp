#include "Sandbox/ui/widgets/umg/MainHUDWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/VerticalBoxSlot.h"

#define WIDGET_CHECK(widget, name_string)            \
    do {                                             \
        if (!widget) {                               \
            missing_widget_error(TEXT(name_string)); \
            return;                                  \
        }                                            \
    } while (0)

void UMainHUDWidget::NativeConstruct() {
    Super::NativeConstruct();

    ammo_widget = WidgetTree->ConstructWidget<UIntNumWidget>(UIntNumWidget::StaticClass(),
                                                             TEXT("ammo_widget"));
    if (ammo_widget && current_stat_box) {
        // You must add the widget first before setters will work
        current_stat_box->AddChild(ammo_widget);
        ammo_widget->set_label(FText::FromName(TEXT("Ammo")));
    } else {
        log_warning(TEXT("Failed to add the child."));
    }
}

void UMainHUDWidget::update_health(FHealthData health_data) {
    WIDGET_CHECK(health_widget, "update_health_percent");
    health_widget->set_health(health_data);
}
void UMainHUDWidget::missing_widget_error(FStringView method) {
    UE_LOGFMT(LogTemp, Error, "UMainHUDWidget: {Method} no widget.", ("Method", method));
}
