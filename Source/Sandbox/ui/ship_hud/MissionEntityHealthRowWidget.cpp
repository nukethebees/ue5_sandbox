#include "Sandbox/ui/ship_hud/MissionEntityHealthRowWidget.h"

#include <Sandbox/logging/SandboxLogCategories.h>
#include <Sandbox/ui/ship_hud/ShipHealthWidget.h>
#include <SandboxCore/error_msg.h>
#include <SandboxCoreEngine/uobject_utils.h>

#include <Components/HorizontalBox.h>
#include <Components/TextBlock.h>

void UMissionEntityHealthRowWidget::NativeConstruct() {
    Super::NativeConstruct();

    check(check_widget_bindings());
}

auto UMissionEntityHealthRowWidget::check_widget_bindings() const -> bool {
    ml::FErrorMsg error_msg;
    if (ml::report_invalid_uobject_ptrs(
            {
                SANDBOX_NAMED_UOBJECT_PTR(row_box),
                SANDBOX_NAMED_UOBJECT_PTR(entity_name),
                SANDBOX_NAMED_UOBJECT_PTR(health_widget),
            },
            error_msg)) {
        UE_LOG(LogSandboxUI,
               Error,
               TEXT("UMissionEntityHealthRowWidget: Invalid widget bindings: %s"),
               *error_msg.message);
        return false;
    }

    return true;
}

void UMissionEntityHealthRowWidget::set_entity(TestEntityUniqueId const unique_id,
                                               ETestEntityType const entity_type) {
    entity_name->SetText(FText::Format(INVTEXT("{0} {1}"),
                                       FText::FromString(ml::get_entity_class_name(entity_type)),
                                       unique_id.id));
}

void UMissionEntityHealthRowWidget::set_health(FShipHealth const health) {
    health_widget->set_health(health);
}

void UMissionEntityHealthRowWidget::set_font_size(int32 const new_font_size) {
    font_size = new_font_size;

    auto font{entity_name->GetFont()};
    font.Size = font_size;
    entity_name->SetFont(font);
    health_widget->set_font_size(font_size);
}
