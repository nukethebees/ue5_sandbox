#include "Sandbox/ui/hud/MainHUDWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

#include "Sandbox/ui/hud/HealthWidget.h"
#include "Sandbox/ui/widgets/NumWidget.h"
#include "Sandbox/ui/widgets/ValueWidget.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

void UMainHUDWidget::NativeConstruct() {
    Super::NativeConstruct();
}

void UMainHUDWidget::update_health(FHealthData health_data) {
    RETURN_IF_NULLPTR(health_widget);
    health_widget->set_health(health_data);
}
