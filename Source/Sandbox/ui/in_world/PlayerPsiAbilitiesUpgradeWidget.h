#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "Sandbox/ui/CommonMenuDelegates.h"

#include "PlayerPsiAbilitiesUpgradeWidget.generated.h"

class UButton;
class UTextBlock;
class UGridPanel;

UCLASS()
class SANDBOX_API UPlayerPsiAbilitiesUpgradeWidget : public UUserWidget {
    GENERATED_BODY()
  protected:
    void NativeOnInitialized() override;
    void NativeConstruct() override;
    void NativeDestruct() override;
};
