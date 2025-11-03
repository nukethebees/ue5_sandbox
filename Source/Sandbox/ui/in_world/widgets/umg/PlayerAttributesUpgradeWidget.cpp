#include "Sandbox/ui/in_world/widgets/umg/PlayerAttributesUpgradeWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Kismet/GameplayStatics.h"

#include "Sandbox/logging/SandboxLogCategories.h"

void UPlayerAttributesUpgradeWidget::NativeOnInitialized() {
    Super::NativeOnInitialized();

#define BIND_BUTTON(WIDGET_NAME, METHOD_NAME) \
    check(WIDGET_NAME);                       \
    WIDGET_NAME->OnClicked.AddDynamic(this, &UPlayerAttributesUpgradeWidget::METHOD_NAME);

    BIND_BUTTON(close_button, on_close_requested);

    BIND_BUTTON(strength_upgrade_button, on_strength_upgrade_requested);
    BIND_BUTTON(endurance_upgrade_button, on_endurance_upgrade_requested);
    BIND_BUTTON(agility_upgrade_button, on_agility_upgrade_requested);
    BIND_BUTTON(cyber_upgrade_button, on_cyber_upgrade_requested);
    BIND_BUTTON(psi_upgrade_button, on_psi_upgrade_requested);

#undef BIND_BUTTON
}
void UPlayerAttributesUpgradeWidget::NativeConstruct() {
    Super::NativeConstruct();
}
void UPlayerAttributesUpgradeWidget::NativeDestruct() {
    Super::NativeDestruct();
}

void UPlayerAttributesUpgradeWidget::on_close_requested() {
    auto* pc{UGameplayStatics::GetPlayerController(this, 0)};
    if (pc) {
        pc->SetPause(false);
        FInputModeGameOnly const input_mode{};
        pc->SetInputMode(input_mode);
        pc->bShowMouseCursor = false;
    } else {
        UE_LOG(LogSandboxUI, Warning, TEXT("pc is nullptr"));
    }

    RemoveFromParent();
}
void UPlayerAttributesUpgradeWidget::on_strength_upgrade_requested() {}
void UPlayerAttributesUpgradeWidget::on_endurance_upgrade_requested() {}
void UPlayerAttributesUpgradeWidget::on_agility_upgrade_requested() {}
void UPlayerAttributesUpgradeWidget::on_cyber_upgrade_requested() {}
void UPlayerAttributesUpgradeWidget::on_psi_upgrade_requested() {}
