#include "Sandbox/environment/interactive/PlayerUpgradeStation.h"

#include "Sandbox/inventory/InventoryComponent.h"
#include "Sandbox/players/playable/MyCharacter.h"
#include "Sandbox/ui/in_world/PlayerAttributesUpgradeWidget.h"
#include "Sandbox/ui/in_world/PlayerPsiAbilitiesUpgradeWidget.h"
#include "Sandbox/ui/in_world/PlayerTechSkillsUpgradeWidget.h"
#include "Sandbox/ui/in_world/PlayerWeaponSkillsUpgradeWidget.h"
#include "Sandbox/utilities/enums.h"

#include "Blueprint/UserWidget.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

APlayerUpgradeStation::APlayerUpgradeStation()
    : display_mesh{CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DisplayMesh"))}
    , collision_box{CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionBox"))} {
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

    display_mesh->SetupAttachment(RootComponent);
    collision_box->SetupAttachment(RootComponent);
}

void APlayerUpgradeStation::on_interacted(AActor& instigator) {
    constexpr auto logger{NestedLogger<"on_interacted">()};
    TRY_INIT_PTR(character, Cast<AMyCharacter>(&instigator));

    switch (station_mode) {
        using enum EPlayerUpgradeStationMode;

        case Attributes: {
            load_attribute_window(*character);
            break;
        }
        case TechSkills: {
            load_tech_skills_window(*character);
            break;
        }
        case WeaponSkills: {
            load_weapon_skills_window(*character);
            break;
        }
        case Psi: {
            load_psi_window(*character);
            break;
        }
        default: {
            logger.log_warning(TEXT("%s"), *ml::make_unhandled_enum_case_warning(station_mode));
            break;
        }
    }
}
void APlayerUpgradeStation::load_attribute_window(AMyCharacter& character) {
    constexpr auto logger{NestedLogger<"load_attribute_window">()};

    load_window(character, attributes_widget_class, logger);
}
void APlayerUpgradeStation::load_tech_skills_window(AMyCharacter& character) {
    constexpr auto logger{NestedLogger<"load_tech_skills_window">()};
}
void APlayerUpgradeStation::load_weapon_skills_window(AMyCharacter& character) {
    constexpr auto logger{NestedLogger<"load_weapon_skills_window">()};
}
void APlayerUpgradeStation::load_psi_window(AMyCharacter& character) {
    constexpr auto logger{NestedLogger<"load_psi_window">()};
}

template <typename WidgetSubClass>
void APlayerUpgradeStation::load_window(AMyCharacter& character,
                                        WidgetSubClass const& widget_class,
                                        auto const& logger) {
    RETURN_IF_NULLPTR(widget_class);
    RETURN_IF_NULLPTR(character.inventory);

    TRY_INIT_PTR(world, GetWorld());
    TRY_INIT_PTR(widget, CreateWidget<typename WidgetSubClass::ElementType>(world, widget_class));

    widget->set_character(character);
    widget->AddToViewport();

    TRY_INIT_PTR(pc, world->GetFirstPlayerController());
    FInputModeUIOnly input_mode;
    pc->SetInputMode(input_mode);
    pc->bShowMouseCursor = true;
    pc->SetPause(true);
}
