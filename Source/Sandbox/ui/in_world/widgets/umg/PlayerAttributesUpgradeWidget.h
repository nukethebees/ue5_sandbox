#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "Sandbox/ui/delegates/CommonMenuDelegates.h"

#include "PlayerAttributesUpgradeWidget.generated.h"

class UButton;
class UTextBlock;
class UGridPanel;

UCLASS()
class SANDBOX_API UPlayerAttributesUpgradeWidget : public UUserWidget {
    GENERATED_BODY()
  protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    UFUNCTION()
    void on_close_requested();
    UFUNCTION()
    void on_strength_upgrade_requested();
    UFUNCTION()
    void on_endurance_upgrade_requested();
    UFUNCTION()
    void on_agility_upgrade_requested();
    UFUNCTION()
    void on_cyber_upgrade_requested();
    UFUNCTION()
    void on_psi_upgrade_requested();

    UPROPERTY(meta = (BindWidget))
    UButton* close_button{nullptr};

    UPROPERTY(meta = (BindWidget))
    UTextBlock* strength_level{nullptr};
    UPROPERTY(meta = (BindWidget))
    UTextBlock* strength_cost{nullptr};
    UPROPERTY(meta = (BindWidget))
    UButton* strength_upgrade_button{nullptr};

    UPROPERTY(meta = (BindWidget))
    UTextBlock* endurance_level{nullptr};
    UPROPERTY(meta = (BindWidget))
    UTextBlock* endurance_cost{nullptr};
    UPROPERTY(meta = (BindWidget))
    UButton* endurance_upgrade_button{nullptr};

    UPROPERTY(meta = (BindWidget))
    UTextBlock* agility_level{nullptr};
    UPROPERTY(meta = (BindWidget))
    UTextBlock* agility_cost{nullptr};
    UPROPERTY(meta = (BindWidget))
    UButton* agility_upgrade_button{nullptr};

    UPROPERTY(meta = (BindWidget))
    UTextBlock* cyber_level{nullptr};
    UPROPERTY(meta = (BindWidget))
    UTextBlock* cyber_cost{nullptr};
    UPROPERTY(meta = (BindWidget))
    UButton* cyber_upgrade_button{nullptr};

    UPROPERTY(meta = (BindWidget))
    UTextBlock* psi_level{nullptr};
    UPROPERTY(meta = (BindWidget))
    UTextBlock* psi_cost{nullptr};
    UPROPERTY(meta = (BindWidget))
    UButton* psi_upgrade_button{nullptr};
};
