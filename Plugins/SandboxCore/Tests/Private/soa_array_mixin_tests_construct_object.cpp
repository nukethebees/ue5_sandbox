#include <SandboxCore/countdown_timers.h>
#include <SandboxCore/soa_array_mixin.h>
#include <SandboxCore/soa_vectors.h>

#include "CoreMinimal.h"
#include "TestHarness.h"

#include <type_traits>

namespace {
template <bool is_const>
struct TConstructObjectTestView : public ml::FSoAViewMixin {
    template <typename T>
    using TView = std::conditional_t<is_const, TConstArrayView<T>, TArrayView<T>>;
    using VectorsView = std::conditional_t<is_const, FVectors3f::ConstView, FVectors3f::View>;
    using View = TConstructObjectTestView<false>;
    using ConstView = TConstructObjectTestView<true>;

    TView<int32> values;
    VectorsView vectors;
    TView<float> timers;

    template <typename TFunc>
    auto apply_arrays(this auto&& self, TFunc&& func) -> decltype(auto) {
        return std::forward<TFunc>(func)(self.values, self.vectors, self.timers);
    }
};

struct FConstructObjectTestData : public ml::FSoAArrayMixin {
    using View = TConstructObjectTestView<false>;
    using ConstView = TConstructObjectTestView<true>;

    TArray<int32> values;
    FVectors3f vectors;
    FCountdownTimers timers;

    template <typename TFunc>
    auto apply_arrays(this auto&& self, TFunc&& func) -> decltype(auto) {
        return std::forward<TFunc>(func)(self.values, self.vectors, self.timers);
    }

    template <typename Self, typename Other, typename TFunc>
        requires std::is_same_v<std::remove_cvref_t<Self>, std::remove_cvref_t<Other>>
    auto apply_array_pairs(this Self&& self, Other&& other, TFunc&& func) -> decltype(auto) {
        return std::forward<TFunc>(func)(self.values,
                                         other.values,
                                         self.vectors,
                                         other.vectors,
                                         self.timers,
                                         other.timers);
    }
};

struct FNoApplyArrays {};

struct FApplyArraysOnly {
    template <typename TFunc>
    auto apply_arrays(this auto&&, TFunc&& func) -> decltype(auto) {
        return std::forward<TFunc>(func)();
    }
};

static_assert(ml::SupportsApplyArrays<FConstructObjectTestData>);
static_assert(ml::SupportsApplyArrays<TConstructObjectTestView<false>>);
static_assert(ml::SupportsApplyArrays<TConstructObjectTestView<true>>);
static_assert(!ml::SupportsApplyArrays<FNoApplyArrays>);
static_assert(ml::SupportsApplyArrayPairs<FConstructObjectTestData>);
static_assert(!ml::SupportsApplyArrayPairs<FApplyArraysOnly>);
static_assert(!ml::SupportsApplyArrayPairs<FNoApplyArrays>);

auto make_test_data() -> FConstructObjectTestData {
    FConstructObjectTestData data;
    data.values = {10, 20, 30, 40};
    data.vectors.xs = {1.f, 2.f, 3.f, 4.f};
    data.vectors.ys = {5.f, 6.f, 7.f, 8.f};
    data.vectors.zs = {9.f, 10.f, 11.f, 12.f};
    data.timers.remaining_times = {0.1f, 0.2f, 0.3f, 0.4f};
    return data;
}
}

TEST_CASE("SandboxCore.SoaArrayMixin.ConstructObject.MutableSlice") {
    auto data{make_test_data()};

    auto view{data.get_view(1, 2)};

    static_assert(std::is_same_v<decltype(view.values), TArrayView<int32>>);
    static_assert(std::is_same_v<decltype(view.vectors), FVectors3f::View>);
    static_assert(std::is_same_v<decltype(view.timers), TArrayView<float>>);
    REQUIRE(view.values.Num() == 2);
    REQUIRE(view.vectors.num() == 2);
    REQUIRE(view.timers.Num() == 2);
    CHECK(view.values[0] == 20);
    CHECK(view.vectors.ys[1] == 7.f);
    CHECK(view.timers[1] == 0.3f);

    view.values[0] = 200;
    view.vectors.zs[1] = 110.f;
    view.timers[1] = 3.f;

    CHECK(data.values[1] == 200);
    CHECK(data.vectors.zs[2] == 110.f);
    CHECK(data.timers.remaining_times[2] == 3.f);

    auto nested_view{view.get_view(1, 1)};
    nested_view.values[0] = 300;

    CHECK(nested_view.values.Num() == 1);
    CHECK(data.values[2] == 300);
}

TEST_CASE("SandboxCore.SoaArrayMixin.ConstructObject.ConstSlice") {
    auto const data{make_test_data()};

    auto view{data.get_const_view(1, 2)};

    static_assert(std::is_same_v<decltype(view.values), TConstArrayView<int32>>);
    static_assert(std::is_same_v<decltype(view.vectors), FVectors3f::ConstView>);
    static_assert(std::is_same_v<decltype(view.timers), TConstArrayView<float>>);
    REQUIRE(view.values.Num() == 2);
    REQUIRE(view.vectors.num() == 2);
    REQUIRE(view.timers.Num() == 2);
    CHECK(view.values[0] == 20);
    CHECK(view.vectors.xs[1] == 3.f);
    CHECK(view.timers[1] == 0.3f);

    auto nested_view{view.get_const_view(1, 1)};

    CHECK(nested_view.values.Num() == 1);
    CHECK(nested_view.values[0] == 30);
    CHECK(nested_view.vectors.zs[0] == 11.f);
    CHECK(nested_view.timers[0] == 0.3f);
}

TEST_CASE("SandboxCore.SoaArrayMixin.ConstructObject.MutableViewToConstView") {
    auto data{make_test_data()};
    auto view{data.get_view()};

    auto const_view{view.get_const_view(1, 2)};

    static_assert(std::is_same_v<decltype(const_view.values), TConstArrayView<int32>>);
    static_assert(std::is_same_v<decltype(const_view.vectors), FVectors3f::ConstView>);
    static_assert(std::is_same_v<decltype(const_view.timers), TConstArrayView<float>>);
    CHECK(const_view.values[0] == 20);
    CHECK(const_view.vectors.ys[1] == 7.f);
    CHECK(const_view.timers[1] == 0.3f);
}
