#include "Sandbox/widgets/LevelSelect2Widget.h"

#include "Components/GridSlot.h"
#include "Sandbox/utilities/levels.h"

void ULevelSelect2Widget::NativeConstruct() {
    Super::NativeConstruct();
    constexpr auto logger{NestedLogger<"NativeConstruct">()};

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
    constexpr auto logger{NestedLogger<"populate_level_buttons">()};
    if (!level_select_grid) {
        logger.log_warning(TEXT("No grid."));
        return;
    }
    if (!load_level_button_class) {
        logger.log_warning(TEXT("No load_level_button_class."));
        return;
    }
    logger.log_verbose(TEXT("Populating main menu with %d levels."), level_names.Num());

    int32 row{0};
    int32 col{0};

    for (auto const& level_name : level_names) {
        auto* button{CreateWidget<ULoadLevelButtonWidget>(GetWorld(), load_level_button_class)};

        if (!button) {
            continue;
        }

        button->set_level_path(level_name);
        button->set_level_display_name(
            FText::FromString(ml::format_level_display_name(level_name)));

        auto* slot{level_select_grid->AddChildToGrid(button, row, col)};
        slot->SetHorizontalAlignment(EHorizontalAlignment::HAlign_Fill);
        slot->SetVerticalAlignment(EVerticalAlignment::VAlign_Center);
        slot->SetPadding(FMargin(8.0f));
        ++row;
    }
}
