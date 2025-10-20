#include "Sandbox/ui/hud/widgets/umg/MainHUDWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

#include "Sandbox/ui/hud/widgets/umg/HealthWidget.h"
#include "Sandbox/ui/widgets/slate/NumWidget.h"
#include "Sandbox/ui/widgets/umg/ValueWidget.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

void UMainHUDWidget::NativeConstruct() {
    Super::NativeConstruct();

    ammo_widget = WidgetTree->ConstructWidget<UIntNumWidget>(UIntNumWidget::StaticClass(),
                                                             TEXT("ammo_widget"));
    RETURN_IF_FALSE(ammo_widget);
    RETURN_IF_FALSE(current_stat_box);

    // You must add the widget first before setters will work
    current_stat_box->AddChild(ammo_widget);
    ammo_widget->set_label(FText::FromName(TEXT("Ammo")));
}

void UMainHUDWidget::update_health(FHealthData health_data) {
    RETURN_IF_NULLPTR(health_widget);
    health_widget->set_health(health_data);
}
