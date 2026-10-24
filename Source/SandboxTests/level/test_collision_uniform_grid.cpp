#include <SandboxTests/support/SoftTestAssertions.h>
#include <SandboxTests/support/TestCollisionActor.h>
#include <SpaceGame/simulation/LevelCollisionHost.h>
#include <SpaceGame/simulation/SpaceGameLevelConfig.h>
#include <SpaceGamePresentation/integration/VectorConversion.h>

#include <Components/BoxComponent.h>
#include <Components/InstancedStaticMeshComponent.h>
#include <CQTest.h>
#include <Engine/World.h>
#include <Misc/ScopeExit.h>

TEST_CLASS(CollisionUniformGrid, "Sandbox.UnitTests")
{
    TEST_METHOD(StaticHarvesting)
    {
        ioj::FSoftTestAssertions checks;
        checks.test_runner = TestRunner;

        auto* const world{UWorld::CreateWorld(EWorldType::Editor, false)};
        if (!checks.is_true(world != nullptr, TEXT("Harvest test world is created"))) {
            return;
        }
        ON_SCOPE_EXIT {
            world->DestroyWorld(false);
        };

        auto* const harvested_actor{world->SpawnActor<ASandboxTestDerivedCollisionActor>(
            ASandboxTestDerivedCollisionActor::StaticClass(), FTransform{FVector::ZeroVector})};
        auto* const omitted_actor{world->SpawnActor<ASandboxTestOmittedCollisionActor>(
            ASandboxTestOmittedCollisionActor::StaticClass(),
            FTransform{FVector{300.f, 0.f, 0.f}})};
        if (!checks.is_valid(harvested_actor, TEXT("Harvest test actor is spawned")) ||
            !checks.is_valid(omitted_actor, TEXT("Omitted test actor is spawned"))) {
            return;
        }

        auto* const unsupported_component{
            NewObject<UInstancedStaticMeshComponent>(harvested_actor)};
        unsupported_component->SetMobility(EComponentMobility::Static);
        unsupported_component->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        harvested_actor->AddInstanceComponent(unsupported_component);
        unsupported_component->RegisterComponent();

        FCollisionGridConfig config;
        config.grid_size = FVector3f{4000.f, 4000.f, 4000.f};
        config.cell_size = FVector3f{100.f, 100.f, 100.f};
        config.harvested_collision_actor_classes.Add(ASandboxTestCollisionActor::StaticClass());
        config.omitted_collision_actor_classes.Add(
            ASandboxTestOmittedCollisionActor::StaticClass());

        auto const configured_dims{config.calculate_grid_dimensions()};
        auto const grid_geometry{ioj::sim::collision::GridGeometry{
            {configured_dims.X, configured_dims.Y, configured_dims.Z},
            ioj::to_native(config.cell_size)}};
        ioj::FLevelCollisionHost collision_host;
        auto const bounds{collision_host.initialise_static_geometry(*world, config, grid_geometry)};

        auto const sources{collision_host.get_static_collision_sources()};
        auto const source_count{sources.Num()};
        checks.are_equal(static_cast<std::size_t>(source_count),
                         bounds.size(),
                         TEXT("Each harvested source produces native bounds"));
        int32 source_index{INDEX_NONE};
        int32 harvested_actor_source_count{};
        for (int32 i{}; i < source_count; ++i) {
            auto* const component{sources[i].component.Get()};
            if (IsValid(component) && component->GetOwner() == harvested_actor) {
                ++harvested_actor_source_count;
            }
            if (component == harvested_actor->get_collision_component()) {
                source_index = i;
            }
        }
        checks.is_true(source_index != INDEX_NONE,
                       TEXT("Configured base class harvests subclass actor"));
        checks.are_equal(1,
                         harvested_actor_source_count,
                         TEXT("Unsupported component is not added to static geometry"));
        checks.is_true(harvested_actor->get_collision_component()->GetCollisionEnabled() ==
                           ECollisionEnabled::NoCollision,
                       TEXT("Successful harvest disables Unreal collision"));
        checks.is_true(omitted_actor->get_collision_component()->GetCollisionEnabled() ==
                           ECollisionEnabled::QueryOnly,
                       TEXT("Omitted actor keeps Unreal collision"));
        checks.is_true(unsupported_component->GetCollisionEnabled() == ECollisionEnabled::QueryOnly,
                       TEXT("Failed harvest leaves Unreal collision enabled"));

        auto const reharvested_bounds{
            collision_host.initialise_static_geometry(*world, config, grid_geometry)};
        checks.are_equal(bounds.size(),
                         reharvested_bounds.size(),
                         TEXT("Reinitialization preserves native bounds count"));
        checks.are_equal(source_count,
                         collision_host.get_static_collision_sources().Num(),
                         TEXT("Reinitialization restores and reharvests static geometry"));
        checks.is_true(harvested_actor->get_collision_component()->GetCollisionEnabled() ==
                           ECollisionEnabled::NoCollision,
                       TEXT("Reharvested component remains owned by custom collision"));
    }
};
