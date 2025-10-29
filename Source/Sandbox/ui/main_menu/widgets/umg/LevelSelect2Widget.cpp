#include "Sandbox/ui/main_menu/widgets/umg/LevelSelect2Widget.h"

#include "Components/GridSlot.h"
#include "Sandbox/core/levels/levels.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

void ULevelSelect2Widget::NativeConstruct() {
    Super::NativeConstruct();
    constexpr auto logger{NestedLogger<"NativeConstruct">()};

    if (back_button) {
        back_button->on_clicked.AddUObject(this, &ULevelSelect2Widget::handle_back);
    }

    RETURN_IF_NULLPTR(level_select_grid);
}
void ULevelSelect2Widget::handle_back() {
    back_requested.Broadcast();
}
void ULevelSelect2Widget::populate_level_buttons(TArray<FName> const& level_names) {
    constexpr auto logger{NestedLogger<"populate_level_buttons">()};
    RETURN_IF_NULLPTR(level_select_grid);
    RETURN_IF_NULLPTR(load_level_button_class);
    logger.log_verbose(TEXT("Populating main menu with %d levels."), level_names.Num());

    int32 row{0};
    int32 col{0};

    for (auto const& level_name : level_names) {
        auto* button{CreateWidget<ULoadLevelButtonWidget>(GetWorld(), load_level_button_class)};
        CONTINUE_IF_FALSE(button);

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
