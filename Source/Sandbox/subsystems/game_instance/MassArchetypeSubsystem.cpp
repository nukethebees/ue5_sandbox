#include "Sandbox/subsystems/game_instance/MassArchetypeSubsystem.h"

#include "MassEntityManager.h"
#include "MassEntitySubsystem.h"
#include "MassEntityTypes.h"

#include "Sandbox/mass_entity/fragments/MassBulletFragments.h"

void UMassArchetypeSubsystem::Initialize(FSubsystemCollectionBase& collection) {
    Super::Initialize(collection);
    constexpr auto logger{NestedLogger<"Initialize">()};

    FWorldDelegates::OnPostWorldInitialization.AddUObject(
        this, &UMassArchetypeSubsystem::on_post_world_initialisation);
}

void UMassArchetypeSubsystem::Deinitialize() {
    Super::Deinitialize();

    bullet_archetype = {};
}

auto UMassArchetypeSubsystem::get_bullet_archetype() const -> FMassArchetypeHandle {
    return bullet_archetype;
}
void UMassArchetypeSubsystem::on_post_world_initialisation(
    UWorld* world, UWorld::InitializationValues const initialisation_values) {
    constexpr auto logger{NestedLogger<"on_post_world_initialisation">()};

    auto* entity_subsystem{world->GetSubsystem<UMassEntitySubsystem>()};
    if (!entity_subsystem) {
        logger.log_error(TEXT("entity_subsystem is nullptr"));
        return;
    }

    build_archetypes(entity_subsystem->GetMutableEntityManager());
}
void UMassArchetypeSubsystem::build_archetypes(FMassEntityManager& entity_manager) {
    {
        auto descriptor{FMassArchetypeCompositionDescriptor{}};
        descriptor.Fragments.Add(*FMassBulletTransformFragment::StaticStruct());
        descriptor.Fragments.Add(*FMassBulletVelocityFragment::StaticStruct());
        descriptor.Fragments.Add(*FMassBulletInstanceIndexFragment::StaticStruct());
        descriptor.Fragments.Add(*FMassBulletVisualizationComponentFragment::StaticStruct());

        auto creation_params{FMassArchetypeCreationParams{}};
        creation_params.DebugName = FName(TEXT("bullet_archetype"));

        bullet_archetype = entity_manager.CreateArchetype(descriptor, creation_params);
    }
}
