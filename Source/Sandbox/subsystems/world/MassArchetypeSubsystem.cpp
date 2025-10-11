#include "Sandbox/subsystems/world/MassArchetypeSubsystem.h"

#include "EngineUtils.h"
#include "MassEntityManager.h"
#include "MassEntitySubsystem.h"
#include "MassEntityTypes.h"

#include "Sandbox/actors/MassBulletVisualizationActor.h"
#include "Sandbox/generated/data_asset_registries/BulletAssetRegistry.h"
#include "Sandbox/mass_entity/fragments/MassBulletFragments.h"

#include "Sandbox/macros/null_checks.hpp"

void UMassArchetypeSubsystem::Initialize(FSubsystemCollectionBase& collection) {
    Super::Initialize(collection);
    constexpr auto logger{NestedLogger<"Initialize">()};

    collection.InitializeDependency(UMassEntitySubsystem::StaticClass());

    TRY_INIT_PTR(world, GetWorld());
    TRY_INIT_PTR(entity_subsystem, world->GetSubsystem<UMassEntitySubsystem>());

    auto& entity_manager{entity_subsystem->GetMutableEntityManager()};
    build_archetypes(entity_manager);
    build_definitions(entity_manager);
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
void UMassArchetypeSubsystem::build_definitions(FMassEntityManager& entity_manager) {
    TRY_INIT_PTR(world, GetWorld());
    TRY_INIT_PTR(gi, world->GetGameInstance());
    TRY_INIT_PTR(ss, gi->GetSubsystem<UBulletAssetRegistry>());

    AMassBulletVisualizationActor* visualization_actor{nullptr};
    {
        for (auto it{TActorIterator<AMassBulletVisualizationActor>(world)}; it; ++it) {
            visualization_actor = *it;
            break;
        }
    }
    RETURN_IF_NULLPTR(visualization_actor);

    {
        TRY_INIT_PTR(bullet_data, ss->get_bullet());
        RETURN_IF_NULLPTR(bullet_data->impact_effect);

        FMassArchetypeSharedFragmentValues shared_values{};

        auto impact_effect_handle{
            entity_manager.GetOrCreateConstSharedFragment<FMassBulletImpactEffectFragment>(
                bullet_data->impact_effect)};

        auto viz_actor_handle{
            entity_manager.GetOrCreateConstSharedFragment<FMassBulletVisualizationActorFragment>(
                visualization_actor)};

        auto damage_handle{entity_manager.GetOrCreateConstSharedFragment<FMassBulletDamageFragment>(
            bullet_data->damage)};

        shared_values.Add(impact_effect_handle);
        shared_values.Add(viz_actor_handle);
        shared_values.Add(damage_handle);
        shared_values.Sort();

        add_definition({bullet_archetype, shared_values}, bullet_data->GetPrimaryAssetId());
    }
}
int32 UMassArchetypeSubsystem::add_definition(FEntityDefinition definition, FPrimaryAssetId id) {
    auto const i{definitions.Add(definition)};
    definition_indexes.Add(id, i);
    return i;
}
