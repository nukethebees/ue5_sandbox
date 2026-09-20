#include <sandbox/core/native_soa/vector_storage_ops.h>

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace ml::native_soa {
namespace {

struct SyntheticSoa {
    using size_type = std::int32_t;

    Vector<std::int32_t> ids;
    Vector<float> values;

    [[nodiscard]] auto num() const noexcept -> size_type {
        return static_cast<size_type>(ids.size());
    }
    void validate_array_sizes() const { vector_storage_ops::validate_array_sizes(*this); }

    template <typename Fn>
    void each_column(Fn&& fn) {
        fn(ids);
        fn(values);
    }
    template <typename Fn>
    void each_column(Fn&& fn) const {
        fn(ids);
        fn(values);
    }
};

struct MissingStorageContract {};

static_assert(vector_storage_ops::VectorStorageSoa<SyntheticSoa>);
static_assert(!vector_storage_ops::VectorStorageSoa<MissingStorageContract>);

void expect_aligned(SyntheticSoa const& soa) {
    soa.validate_array_sizes();
    auto const count{soa.num()};
    for (std::int32_t index{}; index < count; ++index) {
        auto const offset{static_cast<std::size_t>(index)};
        EXPECT_FLOAT_EQ(soa.values[offset], static_cast<float>(soa.ids[offset] * 10));
    }
}

TEST(NativeSoaVectorStorageOps, MutationsKeepColumnsAligned) {
    SyntheticSoa soa;

    vector_storage_ops::reserve(soa, 8);
    EXPECT_GE(soa.ids.capacity(), 8);
    EXPECT_GE(soa.values.capacity(), 8);

    vector_storage_ops::add_uninitialised(soa, 3);
    EXPECT_EQ(soa.ids, (Vector<std::int32_t>{0, 0, 0}));
    EXPECT_EQ(soa.values, (Vector<float>{0.0f, 0.0f, 0.0f}));

    soa.ids = {30, 10, 20};
    soa.values = {300.0f, 100.0f, 200.0f};
    expect_aligned(soa);

    vector_storage_ops::add_defaulted(soa, 1);
    EXPECT_EQ(soa.ids.back(), 0);
    EXPECT_FLOAT_EQ(soa.values.back(), 0.0f);
    vector_storage_ops::set_num(soa, 3);

    std::array<std::int32_t, 3> permutation{1, 2, 0};
    vector_storage_ops::apply_permutation(soa, permutation);
    EXPECT_EQ(soa.ids, (Vector<std::int32_t>{10, 20, 30}));
    expect_aligned(soa);

    std::array<std::int32_t, 3> scratch_indices;
    vector_storage_ops::sort(
        soa,
        [](SyntheticSoa const& value, std::int32_t const lhs, std::int32_t const rhs) {
            return value.ids[static_cast<std::size_t>(lhs)] >
                   value.ids[static_cast<std::size_t>(rhs)];
        },
        scratch_indices);
    EXPECT_EQ(soa.ids, (Vector<std::int32_t>{30, 20, 10}));
    expect_aligned(soa);

    vector_storage_ops::remove_at_swap(soa, 1, 1);
    EXPECT_EQ(soa.ids, (Vector<std::int32_t>{30, 10}));
    expect_aligned(soa);

    auto const ids_capacity{soa.ids.capacity()};
    auto const values_capacity{soa.values.capacity()};
    vector_storage_ops::reset(soa);
    EXPECT_EQ(soa.num(), 0);
    EXPECT_EQ(soa.ids.capacity(), ids_capacity);
    EXPECT_EQ(soa.values.capacity(), values_capacity);
}

} // namespace
} // namespace ml::native_soa
