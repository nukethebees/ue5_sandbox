#include "Sandbox/ui/main_menu/widgets/umg/LoadLevelButtonWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Kismet/GameplayStatics.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

void ULoadLevelButtonWidget::NativeConstruct() {
    Super::NativeConstruct();

    load_level_button->OnClicked.AddDynamic(this, &ULoadLevelButtonWidget::load_level);

    if (set_level_display_name_to_path_if_unset() == DisplayNameChanged::no) {
        update_display_name_text_box();
    }

    if (level_text_block) {
        level_text_block->SetVisibility(ESlateVisibility::HitTestInvisible);
    }
}
void ULoadLevelButtonWidget::set_level_path(FName value) {
    level_path_ = value;
    set_level_display_name_to_path_if_unset();
}
void ULoadLevelButtonWidget::set_level_display_name(FText value) {
    level_display_name_ = value;
    update_display_name_text_box();
}
void ULoadLevelButtonWidget::load_level() {
    RETURN_IF_TRUE(level_path_.IsNone());
    TRY_INIT_PTR(world, GetWorld());
    UGameplayStatics::OpenLevel(world, level_path_);
}
DisplayNameChanged ULoadLevelButtonWidget::set_level_display_name_to_path_if_unset() {
    if (level_display_name_.IsEmpty()) {
        set_level_display_name(FText::FromName(level_path_));
        return DisplayNameChanged::yes;
    }
    return DisplayNameChanged::no;
}
void ULoadLevelButtonWidget::update_display_name_text_box() {
    RETURN_IF_NULLPTR(level_text_block);
    level_text_block->SetText(level_display_name_);
}
