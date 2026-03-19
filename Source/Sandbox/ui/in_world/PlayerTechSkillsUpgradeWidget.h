#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "Sandbox/ui/CommonMenuDelegates.h"

#include "PlayerTechSkillsUpgradeWidget.generated.h"

class UButton;
class UTextBlock;
class UGridPanel;

UCLASS()
class SANDBOX_API UPlayerTechSkillsUpgradeWidget : public UUserWidget {
    GENERATED_BODY()
  protected:
    void NativeOnInitialized() override;
    void NativeConstruct() override;
    void NativeDestruct() override;
};
