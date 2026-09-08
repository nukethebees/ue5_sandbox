#include <SandboxTests/support/SimulationTestAssets.h>
#include <SpaceGame/defences/spinners/TestTubeSpinnerProxy.h>
#include <SpaceGame/defences/turrets/TestStaticTurretsProxy.h>
#include <SpaceGame/ships/capital/TestCapitalShipProxy.h>
#include <SpaceGame/simulation/TestBatchOrchestrator.h>

#include <Components/ArrowComponent.h>
#include <Components/SphereComponent.h>
#include <CQTest.h>
#include <Editor.h>
#include <Engine/World.h>
#include <UObject/UnrealType.h>

TEST_CLASS(ProxyConfiguration, "Sandbox.UnitTests")
{
    UWorld* world{};
    USpaceGameLevelConfig* config{};
    ATestBatchOrchestrator* orchestrator{};

    BEFORE_EACH()
    {
        world = UWorld::CreateWorld(EWorldType::Editor, false);
        auto* const source{ml::load_default_level_config()};
        if (!TestRunner->TestNotNull(TEXT("Default config loads"), source)) {
            return;
        }
        config = DuplicateObject<USpaceGameLevelConfig>(source, world);
        orchestrator = world->SpawnActor<ATestBatchOrchestrator>();
        orchestrator->set_level_config(*config);
    }

    AFTER_EACH()
    {
        world->DestroyWorld(false);
        world = nullptr;
    }

    void invoke(AActor & actor, FName const function_name) {
        auto* const function{actor.FindFunction(function_name)};
        if (!TestRunner->TestNotNull(TEXT("Editor action exists"), function)) {
            return;
        }
        TestRunner->TestTrue(TEXT("Action remains available in Details"),
                             function->HasMetaData(TEXT("CallInEditor")));
        actor.ProcessEvent(function, nullptr);
    }

    auto arrows(AActor & actor, FName const property_name) -> TArray<UArrowComponent*> {
        auto* const property{FindFProperty<FArrayProperty>(actor.GetClass(), property_name)};
        check(property);
        auto* const inner{CastFieldChecked<FObjectPropertyBase>(property->Inner)};
        FScriptArrayHelper values{property, property->ContainerPtrToValuePtr<void>(&actor)};
        TArray<UArrowComponent*> result;
        auto const count{values.Num()};
        for (int32 i{}; i < count; ++i) {
            result.Add(
                CastChecked<UArrowComponent>(inner->GetObjectPropertyValue(values.GetRawPtr(i))));
        }
        return result;
    }

    TEST_METHOD(CapitalApplyAllUsesCurrentAssetAndSaveRoundTripsRotatedArrows)
    {
        auto* const first{world->SpawnActor<ATestCapitalShipProxy>()};
        auto* const second{world->SpawnActor<ATestCapitalShipProxy>()};
        first->SetActorLocationAndRotation(FVector{1200., -500., 300.}, FRotator{15., 125., 5.});
        second->SetActorLocationAndRotation(FVector{-3400., 1500., 200.}, FRotator{0., -45., 0.});
        auto* const stale{DuplicateObject<USpaceGameLevelConfig>(config, world)};
        first->set_level_config_asset(stale);
        second->set_actor_config(nullptr);
        config->capital_ships.fighter_spawn_slots = 2;
        config->capital_ships.fighter_spawn_slots_relative_transforms = {
            FTransform{FVector{10000., -3000., 0.}}, FTransform{FVector{10000., 3000., 0.}}};
        invoke(*first, TEXT("apply_asset_configuration_to_all_instances"));
        for (auto* const proxy : {first, second}) {
            auto const points{arrows(*proxy, TEXT("fighter_spawn_slots"))};
            if (!TestRunner->TestEqual(TEXT("Two capital slots"), points.Num(), 2)) {
                return;
            }
            FTransform const frame{
                proxy->GetActorRotation(), proxy->GetActorLocation(), FVector::OneVector};
            for (int32 i{}; i < 2; ++i) {
                TestRunner->TestTrue(
                    TEXT("Every capital uses current saved actor-space slots"),
                    points[i]->GetComponentTransform().Equals(
                        config->capital_ships.fighter_spawn_slots_relative_transforms[i] * frame,
                        0.01));
            }
        }
        auto const points{arrows(*first, TEXT("fighter_spawn_slots"))};
        auto const moved{points[0]->GetComponentLocation() + FVector{500., -750., 200.}};
        points[0]->SetWorldLocation(moved);
        first->set_level_config_asset(stale);
        invoke(*first, TEXT("save_configuration_to_asset"));
        invoke(*first, TEXT("apply_asset_configuration"));
        TestRunner->TestTrue(TEXT("Save and Apply round-trip world position"),
                             points[0]->GetComponentLocation().Equals(moved, 0.01));
        TestRunner->TestTrue(TEXT("Apply preserves authored component identity"),
                             arrows(*first, TEXT("fighter_spawn_slots"))[0] == points[0]);
        TestRunner->TestEqual(TEXT("Save does not modify stale asset"),
                              stale->capital_ships.fighter_spawn_slots,
                              ml::load_default_level_config()->capital_ships.fighter_spawn_slots);
    }

    TEST_METHOD(CapitalBlueprintApplySurvivesConstructionRerun)
    {
        auto* const proxy{
            world->SpawnActor<ATestCapitalShipProxy>(config->classes.capital_ship_proxy_class)};
        if (!TestRunner->TestNotNull(TEXT("Authored capital Blueprint spawns"), proxy)) {
            return;
        }
        proxy->SetActorLocationAndRotation(FVector{1000., 2000., 3000.}, FRotator{0., 75., 0.});
        invoke(*proxy, TEXT("apply_asset_configuration"));
        proxy->RerunConstructionScripts();
        auto const points{arrows(*proxy, TEXT("fighter_spawn_slots"))};
        if (!TestRunner->TestEqual(TEXT("Construction preserves slot count"),
                                   points.Num(),
                                   config->capital_ships.fighter_spawn_slots)) {
            return;
        }
        FTransform const frame{
            proxy->GetActorRotation(), proxy->GetActorLocation(), FVector::OneVector};
        auto const count{points.Num()};
        for (int32 i{}; i < count; ++i) {
            TestRunner->TestTrue(
                TEXT("Construction preserves applied actor-space layout"),
                points[i]->GetComponentTransform().Equals(
                    config->capital_ships.fighter_spawn_slots_relative_transforms[i] * frame,
                    0.01));
        }
    }

    TEST_METHOD(CapitalApplyAllSupportsUndoRedo)
    {
        auto* const first{world->SpawnActor<ATestCapitalShipProxy>()};
        auto* const second{world->SpawnActor<ATestCapitalShipProxy>()};
        invoke(*first, TEXT("apply_asset_configuration_to_all_instances"));
        auto const first_points{arrows(*first, TEXT("fighter_spawn_slots"))};
        auto const second_points{arrows(*second, TEXT("fighter_spawn_slots"))};
        if (!TestRunner->TestTrue(TEXT("Capital has slots"), !first_points.IsEmpty())) {
            return;
        }
        auto const before{first_points[0]->GetComponentTransform()};
        config->capital_ships.fighter_spawn_slots_relative_transforms[0].AddToTranslation(
            FVector{900., 0., 0.});
        invoke(*first, TEXT("apply_asset_configuration_to_all_instances"));
        auto const after{first_points[0]->GetComponentTransform()};
        GEditor->UndoTransaction();
        TestRunner->TestTrue(TEXT("Undo restores first capital"),
                             first_points[0]->GetComponentTransform().Equals(before));
        TestRunner->TestTrue(TEXT("One undo restores all capitals"),
                             second_points[0]->GetComponentTransform().Equals(before));
        GEditor->RedoTransaction();
        TestRunner->TestTrue(TEXT("Redo restores applied layout"),
                             first_points[0]->GetComponentTransform().Equals(after));
    }

    TEST_METHOD(TurretApplyAllIgnoresStaleAndMissingBindings)
    {
        auto* const first{world->SpawnActor<ATestStaticTurretsProxy>()};
        auto* const second{world->SpawnActor<ATestStaticTurretsProxy>()};
        auto* const stale{DuplicateObject<USpaceGameLevelConfig>(config, world)};
        first->set_actor_config(&stale->turrets);
        second->set_actor_config(nullptr);
        config->turrets.detection_radius = 4321.f;
        config->turrets.fire_point_offset = FTransform{FVector{100., 200., 300.}};
        invoke(*first, TEXT("apply_asset_configuration_to_all_instances"));
        for (auto* const proxy : {first, second}) {
            TestRunner->TestEqual(
                TEXT("Current turret detection radius applied"),
                proxy->FindComponentByClass<USphereComponent>()->GetUnscaledSphereRadius(),
                4321.f);
            TestRunner->TestTrue(
                TEXT("Current turret fire point applied"),
                proxy->FindComponentByClass<UArrowComponent>()->GetRelativeTransform().Equals(
                    config->turrets.fire_point_offset));
        }
    }

    TEST_METHOD(SpinnerApplyAllIgnoresStaleBindingsAndDoesNotDuplicateComponents)
    {
        auto* const first{world->SpawnActor<ATestTubeSpinnerProxy>()};
        auto* const second{world->SpawnActor<ATestTubeSpinnerProxy>()};
        auto* const stale{DuplicateObject<USpaceGameLevelConfig>(config, world)};
        first->set_actor_config(&stale->tube_spinners);
        second->set_actor_config(nullptr);
        config->tube_spinners.fire_point_offsets = {FTransform{FVector{200., 0., 0.}},
                                                    FTransform{FVector{-200., 0., 0.}}};
        invoke(*first, TEXT("apply_asset_configuration_to_all_instances"));
        auto const original{arrows(*first, TEXT("fire_points"))};
        invoke(*first, TEXT("apply_asset_configuration_to_all_instances"));
        for (auto* const proxy : {first, second}) {
            auto const points{arrows(*proxy, TEXT("fire_points"))};
            if (!TestRunner->TestEqual(TEXT("Two spinner points"), points.Num(), 2)) {
                return;
            }
            TArray<UArrowComponent*> components;
            proxy->GetComponents(components);
            TestRunner->TestEqual(
                TEXT("Repeated apply does not leave extra arrows"), components.Num(), 2);
            for (int32 i{}; i < 2; ++i) {
                TestRunner->TestTrue(TEXT("Current spinner fire points applied"),
                                     points[i]->GetRelativeTransform().Equals(
                                         config->tube_spinners.fire_point_offsets[i]));
            }
        }
        TestRunner->TestTrue(TEXT("Spinner apply preserves component identity"),
                             arrows(*first, TEXT("fire_points"))[0] == original[0]);
    }
};
