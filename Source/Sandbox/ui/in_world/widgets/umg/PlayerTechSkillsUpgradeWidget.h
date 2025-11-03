#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "Sandbox/ui/delegates/CommonMenuDelegates.h"

#include "PlayerTechSkillsUpgradeWidget.generated.h"

class UButton;
class UTextBlock;
class UGridPanel;

UCLASS()
class SANDBOX_API UPlayerTechSkillsUpgradeWidget : public UUserWidget {
    GENERATED_BODY()
  protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
};
