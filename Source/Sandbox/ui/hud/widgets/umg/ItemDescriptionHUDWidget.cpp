#include "Sandbox/ui/hud/widgets/umg/ItemDescriptionHUDWidget.h"

#include "Components/TextBlock.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

void UItemDescriptionHUDWidget::NativeConstruct() {
    Super::NativeConstruct();
}

void UItemDescriptionHUDWidget::set_description(FText const& description) {
    RETURN_IF_NULLPTR(description_text);

    description_text->SetText(FText::Format(FText::FromName(TEXT("> {0}")), description));
}
