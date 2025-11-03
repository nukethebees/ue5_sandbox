#pragma once
#pragma once

#include "CoreMinimal.h"

#include "Sandbox/environment/interactive/enums/PlayerUpgradeStationMode.h"
#include "Sandbox/interaction/interfaces/Interactable.h"
#include "Sandbox/logging/mixins/LogMsgMixin.hpp"
#include "Sandbox/logging/SandboxLogCategories.h"

#include "PlayerUpgradeStation.generated.h"

class UStaticMeshComponent;
class UBoxComponent;

class UPlayerAttributesUpgradeWidget;
class UPlayerTechSkillsUpgradeWidget;
class UPlayerWeaponSkillsUpgradeWidget;
class UPlayerPsiAbilitiesUpgradeWidget;

class AMyCharacter;

UCLASS()
class APlayerUpgradeStation
    : public AActor
    , public IInteractable
    , public ml::LogMsgMixin<"APlayerUpgradeStation", LogSandboxActor> {
    GENERATED_BODY()
  public:
    APlayerUpgradeStation();

    virtual void on_interacted(AActor& instigator) override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player")
    TSubclassOf<UPlayerAttributesUpgradeWidget> attributes_widget_class;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player")
    TSubclassOf<UPlayerTechSkillsUpgradeWidget> tech_skills_widget_class;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player")
    TSubclassOf<UPlayerWeaponSkillsUpgradeWidget> weapon_skills_widget_class;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player")
    TSubclassOf<UPlayerPsiAbilitiesUpgradeWidget> psi_abilities_widget_class;
  protected:
    void load_attribute_window(AMyCharacter& character);
    void load_tech_skills_window(AMyCharacter& character);
    void load_weapon_skills_window(AMyCharacter& character);
    void load_psi_window(AMyCharacter& character);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player")
    EPlayerUpgradeStationMode station_mode{EPlayerUpgradeStationMode::Attributes};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player")
    UStaticMeshComponent* display_mesh{nullptr};

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Player")
    UBoxComponent* collision_box{nullptr};
  private:
    template <typename WidgetSubClass>
    void load_window(AMyCharacter& character,
                     WidgetSubClass const& widget_class,
                     auto const& logger);
};
