#include <SandboxTests/support/SoftTestAssertions.h>
#include <SandboxTests/support/TestCollisionActor.h>
#include <SpaceGame/simulation/LevelCollisionHost.h>
#include <SpaceGame/simulation/SpaceGameLevelConfig.h>

#include <SandboxCoreEngine/VectorConversion.h>

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
        auto const bounds{
            ioj::FLevelCollisionHost::extract_static_geometry(*world, config, grid_geometry)};

        checks.are_equal(
            std::size_t{1},
            bounds.size(),
            TEXT("Harvest configured subclass and exclude omitted/unsupported geometry"));
        checks.is_true(harvested_actor->get_collision_component()->GetCollisionEnabled() ==
                           ECollisionEnabled::QueryOnly,
                       TEXT("Geometry extraction leaves component settings unchanged"));
        checks.is_true(omitted_actor->get_collision_component()->GetCollisionEnabled() ==
                           ECollisionEnabled::QueryOnly,
                       TEXT("Omitted actor keeps Unreal collision"));
        checks.is_true(unsupported_component->GetCollisionEnabled() == ECollisionEnabled::QueryOnly,
                       TEXT("Failed harvest leaves Unreal collision enabled"));

        auto const reharvested_bounds{
            ioj::FLevelCollisionHost::extract_static_geometry(*world, config, grid_geometry)};
        checks.are_equal(bounds.size(),
                         reharvested_bounds.size(),
                         TEXT("Reinitialization preserves native bounds count"));
        checks.is_true(harvested_actor->get_collision_component()->GetCollisionEnabled() ==
                           ECollisionEnabled::QueryOnly,
                       TEXT("Repeated extraction leaves component settings unchanged"));
    }
};
