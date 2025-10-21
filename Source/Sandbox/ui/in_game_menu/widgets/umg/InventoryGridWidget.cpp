#include "Sandbox/ui/in_game_menu/widgets/umg/InventoryGridWidget.h"

#include "Components/UniformGridPanel.h"

void UInventoryGridWidget::NativeConstruct() {
    Super::NativeConstruct();
    log_verbose(TEXT("NativeConstruct."));

    OnNativeVisibilityChanged.AddUObject(this, &UInventoryGridWidget::on_visibility_changed);
}
void UInventoryGridWidget::NativeDestruct() {
    log_verbose(TEXT("NativeDestruct."));
    OnNativeVisibilityChanged.RemoveAll(this);

    Super::NativeDestruct();
}
void UInventoryGridWidget::on_visibility_changed(ESlateVisibility new_visibility) {
    switch (new_visibility) {
        case ESlateVisibility::Visible: {
            refresh_grid();
            break;
        }
        default: {
            break;
        }
    }
}
void UInventoryGridWidget::refresh_grid() {
    log_verbose(TEXT("Refreshing grid."));
}
