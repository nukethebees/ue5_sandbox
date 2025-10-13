#include "Sandbox/subsystems/world/MassArchetypeSubsystem.h"

#include "EngineUtils.h"
#include "MassEntityManager.h"
#include "MassEntitySubsystem.h"
#include "MassEntityTypes.h"

#include "Sandbox/actors/MassBulletSubsystemData.h"
#include "Sandbox/actors/MassBulletVisualizationActor.h"
#include "Sandbox/data_assets/BulletDataAsset.h"
#include "Sandbox/mass_entity/fragments/MassBulletFragments.h"
#include "Sandbox/utilities/world.h"

#include "Sandbox/macros/null_checks.hpp"

void UMassArchetypeSubsystem::Initialize(FSubsystemCollectionBase& collection) {
    Super::Initialize(collection);
    constexpr auto logger{NestedLogger<"Initialize">()};

    collection.InitializeDependency(UMassEntitySubsystem::StaticClass());

    TRY_INIT_PTR(world, GetWorld());
    TRY_INIT_PTR(entity_subsystem, world->GetSubsystem<UMassEntitySubsystem>());
    auto& entity_manager{entity_subsystem->GetMutableEntityManager()};

    build_archetypes(entity_manager);

    logger.log_display(TEXT("Finished."));
}
void UMassArchetypeSubsystem::OnWorldBeginPlay(UWorld& in_world) {
    Super::OnWorldBeginPlay(in_world);
    constexpr auto logger{NestedLogger<"OnWorldBeginPlay">()};

    TRY_INIT_PTR(world, GetWorld());
    TRY_INIT_PTR(entity_subsystem, world->GetSubsystem<UMassEntitySubsystem>());
    auto& entity_manager{entity_subsystem->GetMutableEntityManager()};

    build_definitions(entity_manager);

    on_mass_archetype_subsystem_ready.Broadcast();

    logger.log_display(TEXT("Ready."));
}
void UMassArchetypeSubsystem::Deinitialize() {
    Super::Deinitialize();

    bullet_archetype = {};
}

auto UMassArchetypeSubsystem::get_bullet_archetype() const -> FMassArchetypeHandle {
    return bullet_archetype;
}
auto UMassArchetypeSubsystem::get_definition(FPrimaryAssetId id)
    -> std::optional<FEntityDefinition> {
    if (auto i{definition_indexes.Find(id)}) {
        return definitions[*i];
    }
    return {};
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
        descriptor.ConstSharedFragments.Add(*FMassBulletDataFragment::StaticStruct());

        auto creation_params{FMassArchetypeCreationParams{}};
        creation_params.DebugName = FName(TEXT("bullet_archetype"));

        bullet_archetype = entity_manager.CreateArchetype(descriptor, creation_params);
    }
}
void UMassArchetypeSubsystem::build_definitions(FMassEntityManager& entity_manager) {
    constexpr auto logger{NestedLogger<"build_definitions">()};

    TRY_INIT_PTR(world, GetWorld());
    TRY_INIT_PTR(visualization_actor, ml::get_first_actor<AMassBulletVisualizationActor>(*world));
    TRY_INIT_PTR(data_actor, ml::get_first_actor<AMassBulletSubsystemData>(*world));

    RETURN_IF_TRUE(data_actor->bullet_types.IsEmpty());

    for (auto const& bullet_data : data_actor->bullet_types) {
        CONTINUE_IF_NULLPTR(bullet_data);
        CONTINUE_IF_NULLPTR(bullet_data->impact_effect);

        FPrimaryAssetId const asset_id{bullet_data->GetPrimaryAssetId()};
        FMassArchetypeSharedFragmentValues shared_values{};

        auto impact_effect_handle{
            entity_manager.GetOrCreateConstSharedFragment<FMassBulletImpactEffectFragment>(
                bullet_data->impact_effect)};

        auto viz_actor_handle{
            entity_manager.GetOrCreateConstSharedFragment<FMassBulletVisualizationActorFragment>(
                visualization_actor)};

        auto damage_handle{entity_manager.GetOrCreateConstSharedFragment<FMassBulletDamageFragment>(
            bullet_data->damage)};

        auto data_handle{
            entity_manager.GetOrCreateConstSharedFragment<FMassBulletDataFragment>(asset_id)};

        shared_values.Add(impact_effect_handle);
        shared_values.Add(viz_actor_handle);
        shared_values.Add(damage_handle);
        shared_values.Add(data_handle);
        shared_values.Sort();

        add_definition({bullet_archetype, shared_values}, asset_id);
        logger.log_display(TEXT("Created definition for bullet type: %s"), *asset_id.ToString());
    }
}
int32 UMassArchetypeSubsystem::add_definition(FEntityDefinition definition, FPrimaryAssetId id) {
    auto const i{definitions.Add(definition)};
    definition_indexes.Add(id, i);
    return i;
}
