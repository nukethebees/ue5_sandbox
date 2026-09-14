#include "ioj/sim/fighters/sim.h"
#include "support/simulation_test_support.h"

#include <sandbox/core/multi_buffer.h>

namespace ioj::sim::tests {

TEST(NativeSimulation, FighterEntityBiasPackedDataTest) {
    using EntityData = ioj::sim::FighterEntityData;

    EntityData source;
    source.add_defaulted(3);
    source.entity_handles = {
        RegistryEntityHandle{10, 1},
        RegistryEntityHandle{20, 2},
        RegistryEntityHandle{30, 3},
    };
    source.integral_biases = {100u, 200u, 300u};
    source.float_biases = {0.1f, 0.2f, 0.3f};
    source.validate_array_sizes();

    auto view{source.get_view(1, 2)};
    static_assert(std::is_same_v<decltype(view.integral_biases), std::span<std::uint32_t>>);
    static_assert(std::is_same_v<decltype(view.float_biases), std::span<float>>);
    ioj::sim::tests::expect_equal(view.integral_biases[0], 200u, "Mutable view integral bias");
    ioj::sim::tests::expect_equal(view.float_biases[1], 0.3f, "Mutable view float bias");

    EntityData const& const_source{source};
    auto const_view{const_source.get_const_view(1, 2)};
    static_assert(
        std::is_same_v<decltype(const_view.integral_biases), std::span<std::uint32_t const>>);
    static_assert(std::is_same_v<decltype(const_view.float_biases), std::span<float const>>);
    ioj::sim::tests::expect_equal(const_view.integral_biases[1], 300u, "Const view integral bias");
    ioj::sim::tests::expect_equal(const_view.float_biases[0], 0.2f, "Const view float bias");

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
    ioj::sim::tests::expect_true(reordered.entity_handles[0] == RegistryEntityHandle{30, 3},
                                 "Buffered copy keeps handle paired with integral bias");
    ioj::sim::tests::expect_equal(
        reordered.integral_biases[0], 300u, "Buffered copy integral bias");
    ioj::sim::tests::expect_equal(reordered.float_biases[0], 0.3f, "Buffered copy float bias");

    reordered.remove_at_swap(0, 1);
    reordered.validate_array_sizes();
    ioj::sim::tests::expect_true(reordered.entity_handles[0] == RegistryEntityHandle{20, 2},
                                 "Swap removal keeps handle paired with integral bias");
    ioj::sim::tests::expect_equal(reordered.integral_biases[0], 200u, "Swap removal integral bias");
    ioj::sim::tests::expect_equal(reordered.float_biases[0], 0.2f, "Swap removal float bias");

    return;
}

} // namespace ioj::sim::tests
