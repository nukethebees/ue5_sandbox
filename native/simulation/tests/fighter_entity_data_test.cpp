#include "ioj/sim/fighters/sim.h"
#include "support/simulation_test_support.h"

#include <sandbox/core/multi_buffer.h>

namespace ioj::sim::tests {

TEST(NativeSimulation, FighterEntityBiasPackedDataTest) {
    using EntityData = SingleAllocationFighterEntityData;

    EntityData source;
    source.add_defaulted(3);
    auto source_columns{source.get_view().columns()};
    source_columns.entity_ids[0] = EntityUniqueId{10};
    source_columns.entity_ids[1] = EntityUniqueId{20};
    source_columns.entity_ids[2] = EntityUniqueId{30};
    source_columns.integral_biases[0] = 100u;
    source_columns.integral_biases[1] = 200u;
    source_columns.integral_biases[2] = 300u;
    source_columns.float_biases[0] = 0.1f;
    source_columns.float_biases[1] = 0.2f;
    source_columns.float_biases[2] = 0.3f;
    source_columns.validate_array_sizes();

    auto view{source.get_view(1, 2).columns()};
    static_assert(std::is_same_v<decltype(view.integral_biases), std::span<std::uint32_t>>);
    static_assert(std::is_same_v<decltype(view.float_biases), std::span<float>>);
    tests::expect_equal(view.integral_biases[0], 200u, "Mutable view integral bias");
    tests::expect_equal(view.float_biases[1], 0.3f, "Mutable view float bias");

    EntityData const& const_source{source};
    auto const_view{const_source.get_const_view(1, 2).columns()};
    static_assert(
        std::is_same_v<decltype(const_view.integral_biases), std::span<std::uint32_t const>>);
    static_assert(std::is_same_v<decltype(const_view.float_biases), std::span<float const>>);
    tests::expect_equal(const_view.integral_biases[1], 300u, "Const view integral bias");
    tests::expect_equal(const_view.float_biases[0], 0.2f, "Const view float bias");

    ml::MultiBuffer<EntityData, 2> buffers;
    buffers.current().append_from(source.get_const_view());
    buffers.cycle();
    buffers.current().append_from(buffers.previous().slice(2, 1));
    buffers.current().append_from(buffers.previous().slice(0, 2));

    auto& reordered{buffers.current()};
    auto reordered_columns{reordered.get_view().columns()};
    reordered_columns.validate_array_sizes();
    tests::expect_true(reordered_columns.entity_ids[0] == EntityUniqueId{30},
                       "Buffered copy keeps ID paired with integral bias");
    tests::expect_equal(reordered_columns.integral_biases[0], 300u, "Buffered copy integral bias");
    tests::expect_equal(reordered_columns.float_biases[0], 0.3f, "Buffered copy float bias");

    reordered.remove_at_swap(0, 1);
    reordered_columns = reordered.get_view().columns();
    reordered_columns.validate_array_sizes();
    tests::expect_true(reordered_columns.entity_ids[0] == EntityUniqueId{20},
                       "Swap removal keeps ID paired with integral bias");
    tests::expect_equal(reordered_columns.integral_biases[0], 200u, "Swap removal integral bias");
    tests::expect_equal(reordered_columns.float_biases[0], 0.2f, "Swap removal float bias");

    return;
}

} // namespace tests
