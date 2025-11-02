#include "Sandbox/environment/interactive/actors/PlayerUpgradeStation.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"

#include "Sandbox/players/playable/characters/MyCharacter.h"
#include "Sandbox/utilities/enums.h"

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

    logger.log_verbose(TEXT("Start"));
}
void APlayerUpgradeStation::load_tech_skills_window(AMyCharacter& character) {
    constexpr auto logger{NestedLogger<"load_tech_skills_window">()};
    logger.log_verbose(TEXT("Start"));
}
void APlayerUpgradeStation::load_weapon_skills_window(AMyCharacter& character) {
    constexpr auto logger{NestedLogger<"load_weapon_skills_window">()};
    logger.log_verbose(TEXT("Start"));
}
void APlayerUpgradeStation::load_psi_window(AMyCharacter& character) {
    constexpr auto logger{NestedLogger<"load_psi_window">()};
    logger.log_verbose(TEXT("Start"));
}
