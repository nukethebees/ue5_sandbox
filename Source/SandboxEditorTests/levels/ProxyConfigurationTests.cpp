#include <SpaceGame/defences/spinners/TestTubeSpinnerProxy.h>
#include <SpaceGame/defences/turrets/TestStaticTurretsProxy.h>
#include <SpaceGame/ships/capital/TestCapitalShipProxy.h>
#include <SpaceGame/simulation/SpaceGameLevelConfig.h>

#include <Components/ArrowComponent.h>
#include <Components/SphereComponent.h>
#include <CQTest.h>
#include <Editor.h>
#include <Engine/World.h>
#include <Misc/ScopeExit.h>
#include <UObject/Package.h>
#include <UObject/UnrealType.h>

TEST_CLASS(ProxyConfiguration, "SandboxEditor.Authoring")
{
    UWorld* world{};
    ioj::USpaceGameLevelConfig* config{};

    BEFORE_EACH()
    {
        world = UWorld::CreateWorld(EWorldType::Editor, false);
        auto* const source{ioj::s7_level_config::load_canonical()};
        if (!TestRunner->TestNotNull(TEXT("Default config loads"), source)) {
            return;
        }
        config = DuplicateObject<ioj::USpaceGameLevelConfig>(source, world);
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

    void check_arrow_layout(TArray<UArrowComponent*> const& points,
                            TArray<FTransform> const& expected,
                            TCHAR const* description,
                            AActor const* world_frame = nullptr) {
        auto const frame{world_frame ? FTransform{world_frame->GetActorRotation(),
                                                  world_frame->GetActorLocation(),
                                                  FVector::OneVector}
                                     : FTransform::Identity};
        auto const count{points.Num()};
        for (int32 i{}; i < count; ++i) {
            TestRunner->TestTrue(
                FString::Printf(TEXT("%s [%d]"), description, i),
                world_frame ? points[i]->GetComponentTransform().Equals(expected[i] * frame, 0.01)
                            : points[i]->GetRelativeTransform().Equals(expected[i]));
        }
    }

    TEST_METHOD(CapitalApplyAllUsesCurrentAssetAndSaveRoundTripsRotatedArrows)
    {
        auto* const first{world->SpawnActor<ioj::ATestCapitalShipProxy>()};
        auto* const second{world->SpawnActor<ioj::ATestCapitalShipProxy>()};
        first->SetActorLocationAndRotation(FVector{1200., -500., 300.}, FRotator{15., 125., 5.});
        second->SetActorLocationAndRotation(FVector{-3400., 1500., 200.}, FRotator{0., -45., 0.});
        auto* const shared{ioj::s7_level_config::load_canonical()};
        auto& slots{shared->capital_ships.fighter_spawn_slots_relative_transforms};
        auto const original{slots};
        auto* const package{shared->GetPackage()};
        bool const was_dirty{package->IsDirty()};
        ON_SCOPE_EXIT {
            slots = original;
            package->SetDirtyFlag(was_dirty);
        };
        invoke(*first, TEXT("apply_asset_configuration_to_all_instances"));
        for (auto* const proxy : {first, second}) {
            auto const points{arrows(*proxy, TEXT("fighter_spawn_slots"))};
            if (!TestRunner->TestEqual(TEXT("Shared capital slots"), points.Num(), slots.Num()) ||
                !TestRunner->TestTrue(TEXT("At least one shared capital slot"),
                                      !points.IsEmpty())) {
                return;
            }
            check_arrow_layout(
                points, slots, TEXT("Every capital uses current saved actor-space slots"), proxy);
        }
        auto const points{arrows(*first, TEXT("fighter_spawn_slots"))};
        auto const moved{points[0]->GetComponentLocation() + FVector{500., -750., 200.}};
        points[0]->SetWorldLocation(moved);
        invoke(*first, TEXT("save_configuration_to_asset"));
        invoke(*first, TEXT("apply_asset_configuration"));
        TestRunner->TestTrue(TEXT("Save and Apply round-trip world position"),
                             points[0]->GetComponentLocation().Equals(moved, 0.01));
        TestRunner->TestTrue(TEXT("Apply preserves authored component identity"),
                             arrows(*first, TEXT("fighter_spawn_slots"))[0] == points[0]);
    }

    TEST_METHOD(CapitalBlueprintApplySurvivesConstructionRerun)
    {
        auto* const proxy{world->SpawnActor<ioj::ATestCapitalShipProxy>(
            config->classes.capital_ship_proxy_class)};
        if (!TestRunner->TestNotNull(TEXT("Authored capital Blueprint spawns"), proxy)) {
            return;
        }
        proxy->SetActorLocationAndRotation(FVector{1000., 2000., 3000.}, FRotator{0., 75., 0.});
        invoke(*proxy, TEXT("apply_asset_configuration"));
        proxy->RerunConstructionScripts();
        auto const points{arrows(*proxy, TEXT("fighter_spawn_slots"))};
        auto const& slots{ioj::s7_level_config::load_canonical()
                              ->capital_ships.fighter_spawn_slots_relative_transforms};
        if (!TestRunner->TestEqual(
                TEXT("Construction preserves slot count"), points.Num(), slots.Num())) {
            return;
        }
        check_arrow_layout(
            points, slots, TEXT("Construction preserves applied actor-space layout"), proxy);
    }

    TEST_METHOD(CapitalApplyAllSupportsUndoRedo)
    {
        auto* const first{world->SpawnActor<ioj::ATestCapitalShipProxy>()};
        auto* const second{world->SpawnActor<ioj::ATestCapitalShipProxy>()};
        invoke(*first, TEXT("apply_asset_configuration_to_all_instances"));
        auto const first_points{arrows(*first, TEXT("fighter_spawn_slots"))};
        auto const second_points{arrows(*second, TEXT("fighter_spawn_slots"))};
        if (!TestRunner->TestTrue(TEXT("Capital has slots"), !first_points.IsEmpty())) {
            return;
        }
        first_points[0]->AddWorldOffset(FVector{900., 0., 0.});
        second_points[0]->AddWorldOffset(FVector{900., 0., 0.});
        auto const before{first_points[0]->GetComponentTransform()};
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

    TEST_METHOD(TurretApplyAllUsesAssignedResources)
    {
        auto* const first{world->SpawnActor<ioj::ATestStaticTurretsProxy>()};
        auto* const second{world->SpawnActor<ioj::ATestStaticTurretsProxy>()};
        first->set_level_config_asset(config);
        first->set_preview_radius(4321.f);
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

    TEST_METHOD(SpinnerApplyAllUsesAssignedResourcesWithoutDuplicatingComponents)
    {
        auto* const first{world->SpawnActor<ioj::ATestTubeSpinnerProxy>()};
        auto* const second{world->SpawnActor<ioj::ATestTubeSpinnerProxy>()};
        first->set_level_config_asset(config);
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
            check_arrow_layout(points,
                               config->tube_spinners.fire_point_offsets,
                               TEXT("Current spinner fire points applied"));
        }
        TestRunner->TestTrue(TEXT("Spinner apply preserves component identity"),
                             arrows(*first, TEXT("fire_points"))[0] == original[0]);
    }
};
