#include "Sandbox/subsystems/world/MassArchetypeSubsystem.h"

#include "MassEntityManager.h"
#include "MassEntitySubsystem.h"
#include "MassEntityTypes.h"

#include "Sandbox/mass_entity/fragments/MassBulletFragments.h"

#include "Sandbox/macros/null_checks.hpp"

void UMassArchetypeSubsystem::Initialize(FSubsystemCollectionBase& collection) {
    Super::Initialize(collection);
    constexpr auto logger{NestedLogger<"Initialize">()};

    collection.InitializeDependency(UMassEntitySubsystem::StaticClass());

    TRY_INIT_PTR(world, GetWorld());
    TRY_INIT_PTR(entity_subsystem, world->GetSubsystem<UMassEntitySubsystem>());

    build_archetypes(entity_subsystem->GetMutableEntityManager());
    build_definitions();
}

void UMassArchetypeSubsystem::Deinitialize() {
    Super::Deinitialize();

    bullet_archetype = {};
}

auto UMassArchetypeSubsystem::get_bullet_archetype() const -> FMassArchetypeHandle {
    return bullet_archetype;
}
void UMassArchetypeSubsystem::build_archetypes(FMassEntityManager& entity_manager) {
    constexpr auto logger{NestedLogger<"build_archetypes">()};

    {
        auto descriptor{FMassArchetypeCompositionDescriptor{}};
        descriptor.Fragments.Add(*FMassBulletTransformFragment::StaticStruct());
        descriptor.Fragments.Add(*FMassBulletVelocityFragment::StaticStruct());
        descriptor.Fragments.Add(*FMassBulletLastPositionFragment::StaticStruct());
        descriptor.Fragments.Add(*FMassBulletHitInfoFragment::StaticStruct());
        descriptor.Fragments.Add(*FMassBulletStateFragment::StaticStruct());

        descriptor.ConstSharedFragments.Add(*FMassBulletImpactEffectFragment::StaticStruct());
        descriptor.ConstSharedFragments.Add(*FMassBulletVisualizationActorFragment::StaticStruct());
        descriptor.ConstSharedFragments.Add(*FMassBulletDamageFragment::StaticStruct());

        auto creation_params{FMassArchetypeCreationParams{}};
        creation_params.DebugName = FName(TEXT("bullet_archetype"));

        bullet_archetype = entity_manager.CreateArchetype(descriptor, creation_params);
    }
}
void UMassArchetypeSubsystem::build_definitions() {}
