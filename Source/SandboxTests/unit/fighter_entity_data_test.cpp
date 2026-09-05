#include "SpaceGame/ships/fighters/TestCapitalShipFightersSimulation.h"

#include "Misc/AutomationTest.h"

#include <type_traits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFighterEntityBiasPackedDataTest,
                                 "Sandbox.fighters.entity_bias_packed_data",
                                 EAutomationTestFlags::EditorContext |
                                     EAutomationTestFlags::EngineFilter)

auto FFighterEntityBiasPackedDataTest::RunTest(FString const&) -> bool {
    using EntityData = ml::test_capital_ship_fighters::EntityData;

    EntityData source;
    source.add_defaulted(3);
    source.entity_handles = {
        FRegistryEntityHandle{10, 1},
        FRegistryEntityHandle{20, 2},
        FRegistryEntityHandle{30, 3},
    };
    source.integral_biases = {100u, 200u, 300u};
    source.float_biases = {0.1f, 0.2f, 0.3f};
    source.validate_array_sizes();

    auto view{source.get_view(1, 2)};
    static_assert(std::is_same_v<decltype(view.integral_biases), TArrayView<uint32>>);
    static_assert(std::is_same_v<decltype(view.float_biases), TArrayView<float>>);
    TestEqual(TEXT("Mutable view integral bias"), view.integral_biases[0], 200u);
    TestEqual(TEXT("Mutable view float bias"), view.float_biases[1], 0.3f);

    EntityData const& const_source{source};
    auto const_view{const_source.get_const_view(1, 2)};
    static_assert(std::is_same_v<decltype(const_view.integral_biases), TConstArrayView<uint32>>);
    static_assert(std::is_same_v<decltype(const_view.float_biases), TConstArrayView<float>>);
    TestEqual(TEXT("Const view integral bias"), const_view.integral_biases[1], 300u);
    TestEqual(TEXT("Const view float bias"), const_view.float_biases[0], 0.2f);

    ml::MultiBuffer<EntityData, 2> buffers;
    buffers.current().add_defaulted(3);
    buffers.current().copy_element(0, source, 0);
    buffers.current().copy_element(1, source, 1);
    buffers.current().copy_element(2, source, 2);
    buffers.cycle();
    buffers.current().add_defaulted(3);
    buffers.current().copy_element(0, buffers.previous(), 2);
    buffers.current().copy_element(1, buffers.previous(), 0);
    buffers.current().copy_element(2, buffers.previous(), 1);

    auto& reordered{buffers.current()};
    reordered.validate_array_sizes();
    TestTrue(TEXT("Buffered copy keeps handle paired with integral bias"),
             reordered.entity_handles[0] == FRegistryEntityHandle{30, 3});
    TestEqual(TEXT("Buffered copy integral bias"), reordered.integral_biases[0], 300u);
    TestEqual(TEXT("Buffered copy float bias"), reordered.float_biases[0], 0.3f);

    reordered.remove_at_swap(0, 1, EAllowShrinking::No);
    reordered.validate_array_sizes();
    TestTrue(TEXT("Swap removal keeps handle paired with integral bias"),
             reordered.entity_handles[0] == FRegistryEntityHandle{20, 2});
    TestEqual(TEXT("Swap removal integral bias"), reordered.integral_biases[0], 200u);
    TestEqual(TEXT("Swap removal float bias"), reordered.float_biases[0], 0.2f);

    return true;
}
