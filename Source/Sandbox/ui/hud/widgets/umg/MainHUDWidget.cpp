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
}

void UMainHUDWidget::update_health(FHealthData health_data) {
    RETURN_IF_NULLPTR(health_widget);
    health_widget->set_health(health_data);
}
