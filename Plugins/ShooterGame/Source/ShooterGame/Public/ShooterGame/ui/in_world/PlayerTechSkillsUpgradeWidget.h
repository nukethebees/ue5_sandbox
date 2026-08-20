#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "SandboxGameShared/ui/CommonMenuDelegates.h"

#include "PlayerTechSkillsUpgradeWidget.generated.h"

class UButton;
class UTextBlock;
class UGridPanel;

UCLASS()
class SHOOTERGAME_API UPlayerTechSkillsUpgradeWidget : public UUserWidget {
    GENERATED_BODY()
  protected:
    void NativeOnInitialized() override;
    void NativeConstruct() override;
    void NativeDestruct() override;
};
