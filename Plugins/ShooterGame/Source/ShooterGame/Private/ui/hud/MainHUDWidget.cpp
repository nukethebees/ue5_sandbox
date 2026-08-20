#include "ShooterGame/ui/hud/MainHUDWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

#include "ShooterGame/ui/hud/HealthWidget.h"
#include "ShooterGame/ui/widgets/NumWidget.h"
#include "SandboxGameShared/ui/widgets/ValueWidget.h"

#include "SandboxGameShared/utilities/macros/null_checks.hpp"

void UMainHUDWidget::NativeConstruct() {
    Super::NativeConstruct();
}

void UMainHUDWidget::update_health(FHealthData health_data) {
    RETURN_IF_NULLPTR(health_widget);
    health_widget->set_health(health_data);
}
