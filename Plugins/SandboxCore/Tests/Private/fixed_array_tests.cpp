#include <SandboxCore/fixed_array.h>

#include "CoreMinimal.h"
#include "TestHarness.h"

#include <cstdint>
#include <type_traits>

namespace {
struct FLifetimeTrackedValue {
    explicit FLifetimeTrackedValue(int32 const in_value = 0)
        : value{in_value} {
        ++live_count;
    }

    FLifetimeTrackedValue(FLifetimeTrackedValue const& other)
        : value{other.value} {
        ++live_count;
    }

    FLifetimeTrackedValue(FLifetimeTrackedValue&& other) noexcept
        : value{other.value} {
        ++live_count;
        other.value = -1;
    }

    ~FLifetimeTrackedValue() { --live_count; }

    int32 value{0};
    inline static int32 live_count{0};
};

struct FMoveOnlyValue {
    explicit FMoveOnlyValue(int32 const in_value)
        : value{in_value} {}

    FMoveOnlyValue(FMoveOnlyValue const&) = delete;
    auto operator=(FMoveOnlyValue const&) -> FMoveOnlyValue& = delete;

    FMoveOnlyValue(FMoveOnlyValue&& other) noexcept
        : value{other.value} {
        other.value = -1;
    }

    auto operator=(FMoveOnlyValue&& other) noexcept -> FMoveOnlyValue& {
        value = other.value;
        other.value = -1;
        return *this;
    }

    int32 value{0};
};

struct alignas(64) FOverAlignedValue {
    explicit FOverAlignedValue(int32 const in_value)
        : value{in_value} {}

    int32 value{0};
};

struct FTestRegistryEntityHandle {
    using index_type = int32;
    using generation_type = int32;

    static constexpr index_type index_none{-1};

    FTestRegistryEntityHandle() = default;
    FTestRegistryEntityHandle(index_type const in_index, generation_type const in_generation)
        : index{in_index}
        , generation{in_generation} {}

    index_type index{index_none};
    generation_type generation{index_none};
};

auto write_handle_results(TArrayView<FTestRegistryEntityHandle> const out_handles,
                          int32 const result_count,
                          int32 const generation) -> int32 {
    check(result_count >= 0);
    check(result_count <= out_handles.Num());

    for (int32 i{}; i < result_count; ++i) {
        out_handles[i] = FTestRegistryEntityHandle{i, generation};
    }

    return result_count;
}
}

TEST_CASE("SandboxCore.TFixedArray.Default construction has fixed capacity and no elements") {
    ml::TFixedArray<int32, 4> values{};

    static_assert(ml::TFixedArray<int32, 4>::capacity() == 4);
    static_assert(ml::TFixedArray<int32, 0>::capacity() == 0);
    static_assert(std::is_same_v<decltype(values.num()), int32>);

    CHECK(values.num() == 0);
    CHECK(values.is_empty());
    CHECK(!values.is_full());
    CHECK(values.capacity() == 4);
}

TEST_CASE("SandboxCore.TFixedArray.Zero capacity is empty and full") {
    ml::TFixedArray<int32, 0> values{};

    values.reserve(0);

    CHECK(values.num() == 0);
    CHECK(values.is_empty());
    CHECK(values.is_full());
}

TEST_CASE("SandboxCore.TFixedArray.Initialiser list stores only the supplied elements") {
    ml::TFixedArray<int32, 4> values{10, 20, 30};

    CHECK(values.num() == 3);
    CHECK(values.first() == 10);
    CHECK(values[1] == 20);
    CHECK(values.last() == 30);
}

TEST_CASE("SandboxCore.TFixedArray.ArrayView operators expose the active range") {
    ml::TFixedArray<int32, 4> values{10, 20};
    auto const& const_values{values};

    static_assert(std::is_same_v<decltype(static_cast<TArrayView<int32>>(values)), TArrayView<int32>>);
    static_assert(std::is_same_v<decltype(static_cast<TConstArrayView<int32>>(const_values)), TConstArrayView<int32>>);
    static_assert(!std::is_convertible_v<decltype(const_values), TArrayView<int32>>);

    TArrayView<int32> mutable_view{values};
    TConstArrayView<int32> const_view{const_values};

    CHECK(mutable_view.Num() == 2);
    CHECK(mutable_view.GetData() == values.data());
    CHECK(const_view.Num() == 2);
    CHECK(const_view.GetData() == values.data());

    mutable_view[0] = 100;

    CHECK(values[0] == 100);
    CHECK(const_view[0] == 100);
    CHECK(const_view[1] == 20);
}

TEST_CASE("SandboxCore.TFixedArray.capacity_view exposes the full capacity") {
    ml::TFixedArray<int32, 4> values{10, 20};

    static_assert(std::is_same_v<decltype(values.capacity_view()), TArrayView<int32>>);

    auto capacity_view{values.capacity_view()};

    CHECK(values.num() == 2);
    CHECK(capacity_view.Num() == values.capacity());
    CHECK(capacity_view.GetData() == values.data());
    CHECK(capacity_view[0] == 10);
    CHECK(capacity_view[1] == 20);

    capacity_view[2] = 30;
    capacity_view[3] = 40;

    values.set_num_uninitialised(4);

    CHECK(values[2] == 30);
    CHECK(values[3] == 40);
}

TEST_CASE("SandboxCore.TFixedArray.Uninitialised handle output exposes only written results") {
    static_assert(std::is_trivially_copyable_v<FTestRegistryEntityHandle>);
    static_assert(std::is_trivially_destructible_v<FTestRegistryEntityHandle>);

    ml::TFixedArray<FTestRegistryEntityHandle, 4> handles{};
    auto const result_count{write_handle_results(handles.capacity_view(), 3, 17)};
    handles.set_num_uninitialised(result_count);

    CHECK(handles.num() == 3);
    for (int32 i{}; i < handles.num(); ++i) {
        CHECK(handles[i].index == i);
        CHECK(handles[i].generation == 17);
    }
}

TEST_CASE("SandboxCore.TFixedArray.Uninitialised handle output supports no results") {
    ml::TFixedArray<FTestRegistryEntityHandle, 4> handles{};
    auto const result_count{write_handle_results(handles.capacity_view(), 0, 17)};
    handles.set_num_uninitialised(result_count);

    CHECK(handles.is_empty());
    CHECK(handles.begin() == handles.end());
}

TEST_CASE("SandboxCore.TFixedArray.Uninitialised handle output survives repeated stack reuse") {
    static constexpr int32 capacity{8};
    static constexpr int32 iteration_count{1024};

    for (int32 iteration{}; iteration < iteration_count; ++iteration) {
        ml::TFixedArray<FTestRegistryEntityHandle, capacity> handles{};
        auto const result_count{iteration % (capacity + 1)};
        auto const generation{1000 + iteration};

        handles.set_num_uninitialised(
            write_handle_results(handles.capacity_view(), result_count, generation));

        CHECK(handles.num() == result_count);
        for (int32 i{}; i < handles.num(); ++i) {
            CHECK(handles[i].index == i);
            CHECK(handles[i].generation == generation);
        }
    }
}

TEST_CASE("SandboxCore.TFixedArray.Uninitialised handle output supports repeated grow and shrink") {
    ml::TFixedArray<FTestRegistryEntityHandle, 4> handles{};

    handles.set_num_uninitialised(write_handle_results(handles.capacity_view(), 4, 1));
    handles.set_num_uninitialised(1);
    handles.set_num_uninitialised(write_handle_results(handles.capacity_view(), 2, 2));

    CHECK(handles.num() == 2);
    CHECK(handles[0].index == 0);
    CHECK(handles[0].generation == 2);
    CHECK(handles[1].index == 1);
    CHECK(handles[1].generation == 2);
}

TEST_CASE("SandboxCore.TFixedArray.Add and emplace_back append contiguously") {
    ml::TFixedArray<FString, 3> values{};

    CHECK(values.add(TEXT("first")) == 0);
    auto& second{values.emplace_back(TEXT("second"))};
    CHECK(values.add(FString{TEXT("third")}) == 2);

    CHECK(values.is_full());
    CHECK(second == TEXT("second"));
    CHECK(values.data() + 1 == &second);

    FString combined{};
    for (FString const& value : values) {
        combined += value;
    }
    CHECK(combined == TEXT("firstsecondthird"));
}

TEST_CASE("SandboxCore.TFixedArray.add_defaulted and set_num manage its active range") {
    ml::TFixedArray<int32, 4> values{};

    values.add_defaulted(3);
    values[1] = 42;
    values.set_num(2);

    CHECK(values.num() == 2);
    CHECK(values[0] == 0);
    CHECK(values[1] == 42);

    values.set_num(4);
    CHECK(values.num() == 4);
    CHECK(values[2] == 0);
    CHECK(values[3] == 0);
}

TEST_CASE("SandboxCore.TFixedArray.set_num_uninitialised grows and shrinks its active range") {
    ml::TFixedArray<int32, 4> values{10, 20};

    values.set_num_uninitialised(4);

    CHECK(values.num() == 4);
    CHECK(values[0] == 10);
    CHECK(values[1] == 20);

    values[2] = 30;
    values[3] = 40;

    CHECK(values[2] == 30);
    CHECK(values[3] == 40);

    values.set_num_uninitialised(1);

    CHECK(values.num() == 1);
    CHECK(values[0] == 10);
}

TEST_CASE("SandboxCore.TFixedArray.Copy and move preserve active elements") {
    ml::TFixedArray<FString, 3> source{TEXT("alpha"), TEXT("beta")};
    ml::TFixedArray<FString, 3> copied{source};
    ml::TFixedArray<FString, 3> moved{MoveTemp(source)};
    ml::TFixedArray<FString, 3> assigned{};

    CHECK(copied.num() == 2);
    CHECK(copied[0] == TEXT("alpha"));
    CHECK(copied[1] == TEXT("beta"));
    CHECK(source.is_empty());
    CHECK(moved.num() == 2);
    CHECK(moved[0] == TEXT("alpha"));
    CHECK(moved[1] == TEXT("beta"));

    assigned = copied;
    CHECK(assigned.num() == 2);
    CHECK(assigned[0] == TEXT("alpha"));

    assigned = assigned;
    CHECK(assigned.num() == 2);
    CHECK(assigned[1] == TEXT("beta"));

    assigned = MoveTemp(moved);
    CHECK(moved.is_empty());
    CHECK(assigned.num() == 2);
    CHECK(assigned[1] == TEXT("beta"));

    assigned = MoveTemp(assigned);
    CHECK(assigned.num() == 2);
    CHECK(assigned[0] == TEXT("alpha"));
}

TEST_CASE("SandboxCore.TFixedArray.Supports move-only values") {
    ml::TFixedArray<FMoveOnlyValue, 2> values{};
    values.emplace_back(10);
    values.emplace_back(20);

    ml::TFixedArray<FMoveOnlyValue, 2> moved{MoveTemp(values)};
    ml::TFixedArray<FMoveOnlyValue, 2> assigned{};
    assigned = MoveTemp(moved);

    CHECK(values.is_empty());
    CHECK(moved.is_empty());
    CHECK(assigned.num() == 2);
    CHECK(assigned[0].value == 10);
    CHECK(assigned[1].value == 20);
}

TEST_CASE("SandboxCore.TFixedArray.Pop and reset destroy removed elements") {
    CHECK(FLifetimeTrackedValue::live_count == 0);

    {
        ml::TFixedArray<FLifetimeTrackedValue, 3> values{};
        values.emplace_back(10);
        values.emplace_back(20);
        values.emplace_back(30);

        CHECK(FLifetimeTrackedValue::live_count == 3);
        values.pop();
        CHECK(values.num() == 2);
        CHECK(FLifetimeTrackedValue::live_count == 2);

        values.reset();
        CHECK(values.is_empty());
        CHECK(FLifetimeTrackedValue::live_count == 0);
    }

    CHECK(FLifetimeTrackedValue::live_count == 0);
}

TEST_CASE("SandboxCore.TFixedArray.set_num destroys removed elements") {
    CHECK(FLifetimeTrackedValue::live_count == 0);

    {
        ml::TFixedArray<FLifetimeTrackedValue, 3> values{};
        values.set_num(3);
        CHECK(FLifetimeTrackedValue::live_count == 3);

        values.set_num(1);
        CHECK(values.num() == 1);
        CHECK(FLifetimeTrackedValue::live_count == 1);

        values.set_num(0);
        CHECK(values.is_empty());
        CHECK(FLifetimeTrackedValue::live_count == 0);
    }

    CHECK(FLifetimeTrackedValue::live_count == 0);
}

TEST_CASE("SandboxCore.TFixedArray.Storage is aligned for its value type") {
    ml::TFixedArray<FOverAlignedValue, 1> values{};
    values.emplace_back(10);

    auto const data_address{reinterpret_cast<uintptr_t>(values.data())};
    CHECK(data_address % alignof(FOverAlignedValue) == 0);
    CHECK(values[0].value == 10);
}

TEST_CASE("SandboxCore.TFixedArray.Reserve validates a capacity without allocating") {
    ml::TFixedArray<int32, 2> values{};

    values.reserve(2);
    values.add(10);

    CHECK(values.num() == 1);
    CHECK(values[0] == 10);
}
