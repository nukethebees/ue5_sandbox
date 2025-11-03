#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "Sandbox/ui/delegates/CommonMenuDelegates.h"

#include "PlayerAttributesUpgradeWidget.generated.h"

class UButton;
class UTextBlock;
class UGridPanel;

class UTextButtonWidget;
class AMyCharacter;

UCLASS()
class SANDBOX_API UPlayerAttributesUpgradeWidget : public UUserWidget {
    GENERATED_BODY()
  public:
    void set_skill_points(int32 sp);
    void set_character(AMyCharacter& my_char);
    void refresh_values();
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
    UTextBlock* skill_points_display{nullptr};

    UPROPERTY(meta = (BindWidget))
    UTextButtonWidget* close_button{nullptr};

    UPROPERTY(meta = (BindWidget))
    UTextBlock* strength_level{nullptr};
    UPROPERTY(meta = (BindWidget))
    UTextBlock* strength_cost{nullptr};
    UPROPERTY(meta = (BindWidget))
    UTextButtonWidget* strength_upgrade_button{nullptr};

    UPROPERTY(meta = (BindWidget))
    UTextBlock* endurance_level{nullptr};
    UPROPERTY(meta = (BindWidget))
    UTextBlock* endurance_cost{nullptr};
    UPROPERTY(meta = (BindWidget))
    UTextButtonWidget* endurance_upgrade_button{nullptr};

    UPROPERTY(meta = (BindWidget))
    UTextBlock* agility_level{nullptr};
    UPROPERTY(meta = (BindWidget))
    UTextBlock* agility_cost{nullptr};
    UPROPERTY(meta = (BindWidget))
    UTextButtonWidget* agility_upgrade_button{nullptr};

    UPROPERTY(meta = (BindWidget))
    UTextBlock* cyber_level{nullptr};
    UPROPERTY(meta = (BindWidget))
    UTextBlock* cyber_cost{nullptr};
    UPROPERTY(meta = (BindWidget))
    UTextButtonWidget* cyber_upgrade_button{nullptr};

    UPROPERTY(meta = (BindWidget))
    UTextBlock* psi_level{nullptr};
    UPROPERTY(meta = (BindWidget))
    UTextBlock* psi_cost{nullptr};
    UPROPERTY(meta = (BindWidget))
    UTextButtonWidget* psi_upgrade_button{nullptr};

    AMyCharacter* character{nullptr};
};
