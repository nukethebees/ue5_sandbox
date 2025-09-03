#include "Sandbox/widgets/LoadLevelButtonWidget.h"

#include "Kismet/GameplayStatics.h"

void ULoadLevelButtonWidget::NativeConstruct() {
    Super::NativeConstruct();

    load_level_button->OnClicked.AddDynamic(this, &ULoadLevelButtonWidget::load_level);
}
void ULoadLevelButtonWidget::load_level() {
    UGameplayStatics::OpenLevel(GetWorld(), TEXT("MainMenu"));
}
