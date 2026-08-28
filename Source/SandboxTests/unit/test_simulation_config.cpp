#include <SandboxTests/support/SimulationTestAssets.h>

#include <SpaceGame/combat/lasers/TestLasersConfig.h>
#include <SpaceGame/defences/spinners/TestTubeSpinnersConfig.h>
#include <SpaceGame/defences/turrets/TestStaticTurretsConfig.h>
#include <SpaceGame/ships/capital/TestCapitalShipsConfig.h>
#include <SpaceGame/ships/fighters/TestCapitalShipFightersConfig.h>
#include <SpaceGame/ships/player/TestSpaceShipData.h>
#include <SpaceGame/simulation/SimulationConfig.h>
#include <SpaceGame/simulation/SpaceGameLevelConfig.h>
#include <SpaceGame/simulation/TestSimulationConfig.h>

#include <CQTest.h>
#include <UObject/Package.h>
#include <UObject/SoftObjectPtr.h>

TEST_CLASS(SpaceGameLevelConfig, "Sandbox.UnitTests")
{
    TEST_METHOD(DuplicatePreservesNestedValuesAndAssetReferences)
    {
        auto const* const source{ml::load_default_level_config()};
        if (!TestRunner->TestNotNull(TEXT("Default level config loads"), source)) {
            return;
        }
        if (!TestRunner->TestTrue(TEXT("Default level config is valid"), source->is_valid())) {
            return;
        }

        auto* const copy{DuplicateObject<USpaceGameLevelConfig>(source, GetTransientPackage())};
        if (!TestRunner->TestNotNull(TEXT("Level config copy is created"), copy)) {
            return;
        }

        TestRunner->TestTrue(TEXT("Copy has a different address"), source != copy);
        TestRunner->TestTrue(TEXT("Copied level config is valid"), copy->is_valid());
        TestRunner->TestEqual(TEXT("Player laser damage is preserved"),
                              copy->player_ship.laser.damage,
                              source->player_ship.laser.damage);
        TestRunner->TestEqual(TEXT("Fighter spawn transforms are preserved"),
                              copy->capital_ships.fighter_spawn_slots_relative_transforms.Num(),
                              source->capital_ships.fighter_spawn_slots_relative_transforms.Num());
        TestRunner->TestEqual(TEXT("Turret projectile speed is preserved"),
                              copy->turrets.laser.projectile_speed,
                              source->turrets.laser.projectile_speed);
        TestRunner->TestTrue(TEXT("Player team visual remains shared"),
                             copy->player_ship.team_visual_data ==
                                 source->player_ship.team_visual_data);
        TestRunner->TestTrue(TEXT("Projectile mesh remains shared"),
                             copy->laser_projectiles.mesh == source->laser_projectiles.mesh);
        TestRunner->TestTrue(TEXT("Capital mesh remains shared"),
                             copy->capital_ships.mesh == source->capital_ships.mesh);
        TestRunner->TestTrue(TEXT("Fighter mesh remains shared"),
                             copy->fighters.mesh == source->fighters.mesh);
        TestRunner->TestTrue(TEXT("Turret mesh remains shared"),
                             copy->turrets.mesh == source->turrets.mesh);
        TestRunner->TestTrue(TEXT("Spinner mesh remains shared"),
                             copy->tube_spinners.mesh == source->tube_spinners.mesh);
    }

    TEST_METHOD(LegacyDefaultValuesWerePreserved)
    {
        auto const* const migrated{ml::load_default_level_config()};
        static TSoftObjectPtr<UTestSimulationConfig> const legacy_path{
            FSoftObjectPath{TEXT("/Game/Levels/FeatureTests/FT_soa_turrets/"
                                 "DA_default_test_simulation_config."
                                 "DA_default_test_simulation_config")}};
        auto const* const legacy{legacy_path.LoadSynchronous()};
        if (!TestRunner->TestNotNull(TEXT("Migrated level config loads"), migrated) ||
            !TestRunner->TestNotNull(TEXT("Legacy test config loads"), legacy) ||
            !TestRunner->TestNotNull(TEXT("Legacy simulation bundle exists"),
                                     legacy ? legacy->simulation_config.Get() : nullptr)) {
            return;
        }

        auto const& bundle{*legacy->simulation_config};
        auto const* const player{bundle.player_ship_config.Get()};
        auto const* const projectiles{bundle.lasers_config.Get()};
        auto const* const capitals{bundle.capital_ships_config.Get()};
        auto const* const fighters{bundle.capital_ship_fighters_config.Get()};
        auto const* const turrets{bundle.static_turrets_config.Get()};
        auto const* const spinners{bundle.tube_spinners_config.Get()};
        if (!player || !projectiles || !capitals || !fighters || !turrets || !spinners) {
            TestRunner->AddError(TEXT("Legacy bundle contains a null domain config"));
            return;
        }

        TestRunner->TestTrue(TEXT("Player controller class was preserved"),
                             migrated->classes.player_controller_class ==
                                 legacy->player_controller_class);
        TestRunner->TestEqual(TEXT("Player laser damage was preserved"),
                              migrated->player_ship.laser.damage,
                              player->laser_damage);
        TestRunner->TestEqual(TEXT("Player laser cooldown was preserved"),
                              migrated->player_ship.laser.fire_cooldown,
                              player->laser_firing_period);
        TestRunner->TestTrue(TEXT("Player team visual was preserved"),
                             migrated->player_ship.team_visual_data == player->team_visual_data);
        TestRunner->TestTrue(TEXT("Projectile mesh was preserved"),
                             migrated->laser_projectiles.mesh == projectiles->mesh);
        TestRunner->TestTrue(TEXT("Projectile material was preserved"),
                             migrated->laser_projectiles.material == projectiles->material);
        TestRunner->TestEqual(TEXT("Capital health was preserved"),
                              migrated->capital_ships.max_health,
                              capitals->max_health);
        TestRunner->TestEqual(TEXT("Capital spawn slots were preserved"),
                              migrated->capital_ships.fighter_spawn_slots_relative_transforms.Num(),
                              capitals->fighter_spawn_slots_relative_transforms.Num());
        TestRunner->TestEqual(TEXT("Fighter laser speed was preserved"),
                              migrated->fighters.laser.projectile_speed,
                              fighters->laser_speed);
        TestRunner->TestTrue(TEXT("Fighter mesh was preserved"),
                             migrated->fighters.mesh == fighters->mesh);
        TestRunner->TestEqual(TEXT("Turret cooldown was preserved"),
                              migrated->turrets.laser.fire_cooldown,
                              turrets->attack_cooldown);
        TestRunner->TestTrue(TEXT("Turret team visual was preserved"),
                             migrated->turrets.team_visual_data == turrets->team_visual_data);
        TestRunner->TestEqual(TEXT("Spinner laser distance was preserved"),
                              migrated->tube_spinners.laser.max_distance,
                              spinners->laser_max_distance);
        TestRunner->TestTrue(TEXT("Spinner mesh was preserved"),
                             migrated->tube_spinners.mesh == spinners->mesh);
    }
};
