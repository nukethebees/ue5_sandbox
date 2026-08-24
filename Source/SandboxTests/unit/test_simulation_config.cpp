#include <SandboxTests/support/SimulationTestAssets.h>

#include <SpaceGame/simulation/SimulationConfig.h>
#include <SpaceGame/ships/fighters/TestCapitalShipFightersConfig.h>
#include <SpaceGame/ships/capital/TestCapitalShipsConfig.h>
#include <SpaceGame/combat/lasers/TestLasersConfig.h>
#include <SpaceGame/ships/player/TestSpaceShipData.h>
#include <SpaceGame/defences/turrets/TestStaticTurretsConfig.h>
#include <SpaceGame/defences/spinners/TestTubeSpinnersConfig.h>

#include <CQTest.h>

TEST_CLASS(SimulationConfig, "Sandbox.UnitTests")
{
    TEST_METHOD(DeepCopy)
    {
        auto const* const source{ml::load_default_simulation_config()};
        if (!TestRunner->TestNotNull(TEXT("Default simulation config loads"), source)) {
            return;
        }
        if (!TestRunner->TestTrue(TEXT("Default simulation config is valid"), source->is_valid())) {
            return;
        }

        auto* const copy{source->deep_copy()};
        if (!TestRunner->TestNotNull(TEXT("Deep copy is created"), copy)) {
            return;
        }

        TestRunner->TestTrue(TEXT("Bundle has a different address"), source != copy);
        TestRunner->TestTrue(TEXT("Copied bundle is valid"), copy->is_valid());

        auto verify_child{[this, copy](auto const* const source_child,
                                       auto const* const copied_child,
                                       TCHAR const* const child_name) {
            if (!TestRunner->TestNotNull(*FString::Printf(TEXT("%s source exists"), child_name),
                                         source_child)) {
                return;
            }
            if (!TestRunner->TestNotNull(*FString::Printf(TEXT("%s copy exists"), child_name),
                                         copied_child)) {
                return;
            }

            TestRunner->TestTrue(*FString::Printf(TEXT("%s has a different address"), child_name),
                                 source_child != copied_child);
            TestRunner->TestTrue(
                *FString::Printf(TEXT("%s has the copied bundle as its outer"), child_name),
                copied_child->GetOuter() == copy);
            TestRunner->TestTrue(*FString::Printf(TEXT("%s retains its class"), child_name),
                                 copied_child->GetClass() == source_child->GetClass());
        }};

        verify_child(source->player_ship_config.Get(),
                     copy->player_ship_config.Get(),
                     TEXT("Player ship config"));
        verify_child(source->lasers_config.Get(), copy->lasers_config.Get(), TEXT("Lasers config"));
        verify_child(source->capital_ships_config.Get(),
                     copy->capital_ships_config.Get(),
                     TEXT("Capital ships config"));
        verify_child(source->capital_ship_fighters_config.Get(),
                     copy->capital_ship_fighters_config.Get(),
                     TEXT("Capital ship fighters config"));
        verify_child(source->static_turrets_config.Get(),
                     copy->static_turrets_config.Get(),
                     TEXT("Static turrets config"));
        verify_child(source->tube_spinners_config.Get(),
                     copy->tube_spinners_config.Get(),
                     TEXT("Tube spinners config"));

        TestRunner->TestTrue(TEXT("Player ship nested assets remain shared"),
                             source->player_ship_config->team_visual_data ==
                                 copy->player_ship_config->team_visual_data);
        TestRunner->TestTrue(TEXT("Lasers nested assets remain shared"),
                             source->lasers_config->mesh == copy->lasers_config->mesh);
        TestRunner->TestTrue(TEXT("Capital ships nested assets remain shared"),
                             source->capital_ships_config->mesh ==
                                 copy->capital_ships_config->mesh);
        TestRunner->TestTrue(TEXT("Fighter nested assets remain shared"),
                             source->capital_ship_fighters_config->mesh ==
                                 copy->capital_ship_fighters_config->mesh);
        TestRunner->TestTrue(TEXT("Turret nested assets remain shared"),
                             source->static_turrets_config->mesh ==
                                 copy->static_turrets_config->mesh);
        TestRunner->TestTrue(TEXT("Spinner nested assets remain shared"),
                             source->tube_spinners_config->mesh ==
                                 copy->tube_spinners_config->mesh);
    }
};
