#include "Sandbox/widgets/LevelSelect2Widget.h"

#include "Components/GridSlot.h"

void ULevelSelect2Widget::NativeConstruct() {
    Super::NativeConstruct();

    if (back_button) {
        back_button->on_clicked.AddDynamic(this, &ULevelSelect2Widget::handle_back);
    }

    if (!level_select_grid) {
        return;
    }
}
void ULevelSelect2Widget::handle_back() {
    back_requested.Broadcast();
}
void ULevelSelect2Widget::populate_level_buttons(TArray<FName> const& level_names) {   
    if (!level_select_grid) {
        UE_LOGFMT(LogTemp, Warning, "No grid.");
        return;
    }
    if (!load_level_button_class) {
        UE_LOGFMT(LogTemp, Warning, "No load_level_button_class.");
        return;
    }

    int32 row{0};
    int32 col{0};

    for (auto const& level_name : level_names) {
        auto* button{CreateWidget<ULoadLevelButtonWidget>(GetWorld(), load_level_button_class)};

        if (!button) {
            continue;
        }

        button->set_level_path(level_name);
        button->set_level_display_name(FText::FromName(level_name));

        auto* slot{level_select_grid->AddChildToGrid(button, row, col)};
        slot->SetHorizontalAlignment(EHorizontalAlignment::HAlign_Fill);
        slot->SetVerticalAlignment(EVerticalAlignment::VAlign_Center);
        ++row;
    }
}
