#include "Sandbox/mass_entity/MassArchetypeSubsystem.h"

#include "MassEntityManager.h"
#include "MassEntitySubsystem.h"
#include "MassEntityTypes.h"

#include "Sandbox/combat/bullets/MassBulletSubsystemData.h"
#include "Sandbox/combat/bullets/MassBulletVisualizationActor.h"
#include "Sandbox/combat/bullets/BulletDataAsset.h"
#include "Sandbox/combat/bullets/MassBulletFragments.h"
#include "Sandbox/environment/effects/subsystems/NiagaraNdcWriterSubsystem.h"
#include "Sandbox/environment/utilities/world.h"
#include "Sandbox/mass_entity/mass_utils.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

void UMassArchetypeSubsystem::Initialize(FSubsystemCollectionBase& collection) {
    Super::Initialize(collection);
    constexpr auto logger{NestedLogger<"Initialize">()};

    collection.InitializeDependency(UMassEntitySubsystem::StaticClass());
    collection.InitializeDependency(UNiagaraNdcWriterSubsystem::StaticClass());

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
        ml::add_fragments<FMassBulletTransformFragment,
                          FMassBulletVelocityFragment,
                          FMassBulletLastPositionFragment,
                          FMassBulletHitInfoFragment,
                          FMassBulletStateFragment,
                          FMassBulletDamageFragment,
                          FMassBulletSourceFragment>(descriptor.GetFragments());
        ml::add_fragments<FMassBulletImpactEffectFragment,
                          FMassBulletVisualizationActorFragment,
                          FMassBulletDataFragment>(descriptor.GetConstSharedFragments());

        auto creation_params{FMassArchetypeCreationParams{}};
        creation_params.DebugName = FName(TEXT("bullet_archetype"));

        bullet_archetype = entity_manager.CreateArchetype(descriptor, creation_params);
    }
}
void UMassArchetypeSubsystem::build_definitions(FMassEntityManager& entity_manager) {
    constexpr auto logger{NestedLogger<"build_definitions">()};

    TRY_INIT_PTR(world, GetWorld());
    TRY_INIT_PTR(ndc_subsystem, world->GetSubsystem<UNiagaraNdcWriterSubsystem>());
    TRY_INIT_PTR(visualization_actor, ml::get_first_actor<AMassBulletVisualizationActor>(*world));
    TRY_INIT_PTR(data_actor, ml::get_first_actor<AMassBulletSubsystemData>(*world));

    RETURN_IF_TRUE(data_actor->bullet_types.IsEmpty());

    for (int32 i{0}; i < data_actor->bullet_types.Num(); ++i) {
        auto const& bullet_data{data_actor->bullet_types[i]};
        CONTINUE_IF_NULLPTR(bullet_data);
        CONTINUE_IF_NULLPTR(bullet_data->impact_effect);
        CONTINUE_IF_NULLPTR(bullet_data->ndc_asset);

        FPrimaryAssetId const asset_id{bullet_data->GetPrimaryAssetId()};
        FMassArchetypeSharedFragmentValues shared_values{};

        auto ndc_idx{ndc_subsystem->register_asset(*bullet_data->ndc_asset, 50000)};

        auto impact_effect_handle{
            entity_manager.GetOrCreateConstSharedFragment<FMassBulletImpactEffectFragment>(
                ndc_idx)};

        auto viz_actor_handle{
            entity_manager.GetOrCreateConstSharedFragment<FMassBulletVisualizationActorFragment>(
                visualization_actor)};

        auto data_handle{entity_manager.GetOrCreateConstSharedFragment<FMassBulletDataFragment>(
            asset_id, FBulletTypeIndex{i})};

        ml::add_values(shared_values, impact_effect_handle, viz_actor_handle, data_handle);

        add_definition({bullet_archetype, shared_values}, asset_id);
        logger.log_display(
            TEXT("Created definition for bullet type: %s (index %d)"), *asset_id.ToString(), i);
    }
}
int32 UMassArchetypeSubsystem::add_definition(FEntityDefinition definition, FPrimaryAssetId id) {
    auto const i{definitions.Add(definition)};
    definition_indexes.Add(id, i);
    return i;
}
