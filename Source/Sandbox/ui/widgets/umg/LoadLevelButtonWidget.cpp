#include "Sandbox/ui/widgets/umg/LoadLevelButtonWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Kismet/GameplayStatics.h"

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
    if (!level_path_.IsNone()) {
        UGameplayStatics::OpenLevel(GetWorld(), level_path_);
    } else {
        UE_LOGFMT(LogTemp, Warning, "Level path is empty.");
    }
}
DisplayNameChanged ULoadLevelButtonWidget::set_level_display_name_to_path_if_unset() {
    if (level_display_name_.IsEmpty()) {
        set_level_display_name(FText::FromName(level_path_));
        return DisplayNameChanged::yes;
    }
    return DisplayNameChanged::no;
}
void ULoadLevelButtonWidget::update_display_name_text_box() {
    if (level_text_block) {
        level_text_block->SetText(level_display_name_);
    }
}
