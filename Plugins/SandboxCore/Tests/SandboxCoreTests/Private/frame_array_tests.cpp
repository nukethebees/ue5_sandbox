#include <SandboxCore/frame_array.h>

#include "CoreMinimal.h"
#include "TestHarness.h"

#include <limits>
#include <memory_resource>
#include <type_traits>
#include <utility>

namespace {
struct FTrackedValue {
    explicit FTrackedValue(int32 const in_value = 0)
        : value{in_value} {
        ++direct_constructor_count;
        ++live_count;
    }

    FTrackedValue(FTrackedValue const& other)
        : value{other.value} {
        ++copy_constructor_count;
        ++live_count;
    }

    FTrackedValue(FTrackedValue&& other) noexcept
        : value{other.value} {
        ++move_constructor_count;
        ++live_count;
        other.value = -1;
    }

    auto operator=(FTrackedValue const& other) -> FTrackedValue& {
        value = other.value;
        return *this;
    }

    auto operator=(FTrackedValue&& other) noexcept -> FTrackedValue& {
        value = other.value;
        other.value = -1;
        return *this;
    }

    ~FTrackedValue() {
        ++destructor_count;
        --live_count;
    }

    static void reset_counts() {
        check(live_count == 0);
        direct_constructor_count = 0;
        copy_constructor_count = 0;
        move_constructor_count = 0;
        destructor_count = 0;
    }

    int32 value{0};
    inline static int32 direct_constructor_count{0};
    inline static int32 copy_constructor_count{0};
    inline static int32 move_constructor_count{0};
    inline static int32 destructor_count{0};
    inline static int32 live_count{0};
};

struct FEmplacedValue {
    FEmplacedValue(int32 const in_id, FString in_name)
        : id{in_id}
        , name{MoveTemp(in_name)} {}

    int32 id{0};
    FString name;
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
}

TEST_CASE("SandboxCore.TFrameArray has fixed identity and an int32 size API") {
    using FArray = ml::TFrameArray<int32>;

    static_assert(std::is_default_constructible_v<FArray>);
    static_assert(std::is_constructible_v<FArray, std::pmr::memory_resource*>);
    static_assert(!std::is_copy_constructible_v<FArray>);
    static_assert(!std::is_copy_assignable_v<FArray>);
    static_assert(!std::is_move_constructible_v<FArray>);
    static_assert(!std::is_move_assignable_v<FArray>);
    static_assert(std::is_same_v<decltype(std::declval<FArray const&>().num()), int32>);
    static_assert(std::is_same_v<decltype(&FArray::reserve), void (FArray::*)(int32)>);
}

TEST_CASE("SandboxCore.TFrameArray default construction exposes an empty active range") {
    ml::TFrameArray<int32> values{};
    auto const& const_values{values};

    CHECK(values.num() == 0);
    CHECK(values.is_empty());
    CHECK(values.begin() == values.end());
    CHECK(const_values.begin() == const_values.end());
    CHECK(values.view().Num() == 0);
    CHECK(values.view().GetData() == values.data());
    CHECK(const_values.view().Num() == 0);
    CHECK(const_values.view().GetData() == const_values.data());
}

TEST_CASE("SandboxCore.TFrameArray add copies and moves values") {
    FTrackedValue::reset_counts();

    {
        ml::TFrameArray<FTrackedValue> values{};
        values.reserve(2);
        FTrackedValue copied_source{10};
        FTrackedValue moved_source{20};

        auto& copied{values.add(copied_source)};
        auto& moved{values.add(std::move(moved_source))};

        CHECK(values.num() == 2);
        CHECK(&copied == values.data());
        CHECK(&moved == values.data() + 1);
        CHECK(copied.value == 10);
        CHECK(moved.value == 20);
        CHECK(copied_source.value == 10);
        CHECK(moved_source.value == -1);
        CHECK(FTrackedValue::copy_constructor_count == 1);
        CHECK(FTrackedValue::move_constructor_count == 1);
        CHECK(FTrackedValue::live_count == 4);
    }

    CHECK(FTrackedValue::live_count == 0);
    CHECK(FTrackedValue::destructor_count == 4);
}

TEST_CASE("SandboxCore.TFrameArray emplace constructs non-trivial values in place") {
    ml::TFrameArray<FEmplacedValue> values{};
    values.reserve(2);

    auto& first{values.emplace(7, TEXT("alpha"))};
    auto& second{values.emplace(11, TEXT("beta"))};

    CHECK(&first == values.data());
    CHECK(&second == values.data() + 1);
    CHECK(first.id == 7);
    CHECK(first.name == TEXT("alpha"));
    CHECK(second.id == 11);
    CHECK(second.name == TEXT("beta"));
}

TEST_CASE("SandboxCore.TFrameArray indexing preserves order and supports mutation") {
    ml::TFrameArray<int32> values{};
    values.add(10);
    values.add(20);
    values.add(30);

    values[1] = 200;
    auto const& const_values{values};

    CHECK(values[0] == 10);
    CHECK(values[1] == 200);
    CHECK(values[2] == 30);
    CHECK(const_values[0] == 10);
    CHECK(const_values[1] == 200);
    CHECK(const_values[2] == 30);
}

TEST_CASE("SandboxCore.TFrameArray reserve stabilises storage and clear permits reuse") {
    ml::TFrameArray<int32> values{};
    values.reserve(4);

    values.add(10);
    auto* const reserved_data{values.data()};
    values.add(20);
    values.add(30);
    values.add(40);

    CHECK(values.data() == reserved_data);
    CHECK(values.num() == 4);

    values.clear();

    CHECK(values.is_empty());
    CHECK(values.data() == reserved_data);
    CHECK(values.view().Num() == 0);

    auto& reused{values.add(50)};

    CHECK(values.data() == reserved_data);
    CHECK(&reused == reserved_data);
    CHECK(values.num() == 1);
    CHECK(values[0] == 50);
}

TEST_CASE("SandboxCore.TFrameArray named and implicit views preserve constness") {
    ml::TFrameArray<int32> values{};
    values.add(10);
    values.add(20);
    auto const& const_values{values};

    static_assert(std::is_same_v<decltype(values.view()), TArrayView<int32>>);
    static_assert(std::is_same_v<decltype(const_values.view()), TConstArrayView<int32>>);
    static_assert(std::is_convertible_v<decltype(values), TArrayView<int32>>);
    static_assert(std::is_convertible_v<decltype(values), TConstArrayView<int32>>);
    static_assert(std::is_convertible_v<decltype(const_values), TConstArrayView<int32>>);
    static_assert(!std::is_convertible_v<decltype(const_values), TArrayView<int32>>);

    TArrayView<int32> mutable_view{values};
    TConstArrayView<int32> const_view{const_values};

    CHECK(mutable_view.GetData() == values.data());
    CHECK(const_view.GetData() == const_values.data());
    CHECK(mutable_view.Num() == values.num());
    CHECK(const_view.Num() == const_values.num());

    mutable_view[0] = 100;

    CHECK(values[0] == 100);
    CHECK(const_view[0] == 100);
    CHECK(const_view[1] == 20);
}

TEST_CASE("SandboxCore.TFrameArray range iteration is mutable and const-correct") {
    ml::TFrameArray<int32> values{};
    values.add(1);
    values.add(2);
    values.add(3);

    for (int32& value : values) {
        value *= 10;
    }

    auto const& const_values{values};
    int32 expected{10};
    int32 sum{0};
    for (int32 const& value : const_values) {
        CHECK(value == expected);
        expected += 10;
        sum += value;
    }

    CHECK(sum == 60);
}

TEST_CASE("SandboxCore.TFrameArray clear destroys every active element exactly once") {
    FTrackedValue::reset_counts();

    {
        ml::TFrameArray<FTrackedValue> values{};
        values.reserve(3);
        values.emplace(10);
        values.emplace(20);
        values.emplace(30);

        CHECK(FTrackedValue::live_count == 3);
        CHECK(FTrackedValue::destructor_count == 0);

        values.clear();

        CHECK(values.is_empty());
        CHECK(FTrackedValue::live_count == 0);
        CHECK(FTrackedValue::destructor_count == 3);
    }

    CHECK(FTrackedValue::live_count == 0);
    CHECK(FTrackedValue::destructor_count == 3);
}

TEST_CASE("SandboxCore.TFrameArray destruction destroys every active element exactly once") {
    FTrackedValue::reset_counts();

    {
        ml::TFrameArray<FTrackedValue> values{};
        values.reserve(3);
        values.emplace(10);
        values.emplace(20);
        values.emplace(30);

        CHECK(FTrackedValue::live_count == 3);
        CHECK(FTrackedValue::destructor_count == 0);
    }

    CHECK(FTrackedValue::live_count == 0);
    CHECK(FTrackedValue::destructor_count == 3);
}

TEST_CASE("SandboxCore.TFrameArray supports move-only element types") {
    ml::TFrameArray<FMoveOnlyValue> values{};
    values.reserve(2);
    FMoveOnlyValue source{10};

    auto& first{values.add(std::move(source))};
    auto& second{values.emplace(20)};

    CHECK(source.value == -1);
    CHECK(first.value == 10);
    CHECK(second.value == 20);
    CHECK(values.num() == 2);
}

TEST_CASE("SandboxCore.TFrameArray preserves values through repeated growth") {
    static constexpr int32 element_count{4096};
    FTrackedValue::reset_counts();

    int32 storage_change_count{0};
    {
        ml::TFrameArray<FTrackedValue> values{};
        FTrackedValue* previous_data{values.data()};

        for (int32 i{0}; i < element_count; ++i) {
            values.emplace(i);

            if (values.data() != previous_data) {
                ++storage_change_count;
                previous_data = values.data();
            }
        }

        CHECK(values.num() == element_count);
        CHECK(storage_change_count > 1);
        CHECK(FTrackedValue::live_count == element_count);

        for (int32 i{0}; i < element_count; ++i) {
            CHECK(values[i].value == i);
        }
    }

    auto const total_construction_count{FTrackedValue::direct_constructor_count + FTrackedValue::copy_constructor_count +
                                        FTrackedValue::move_constructor_count};
    CHECK(FTrackedValue::live_count == 0);
    CHECK(FTrackedValue::destructor_count == total_construction_count);
}

TEST_CASE("SandboxCore.TFrameArray accepts a borrowed standard memory resource") {
    std::pmr::unsynchronized_pool_resource resource{std::pmr::new_delete_resource()};
    ml::TFrameArray<FString> values{&resource};

    values.reserve(2);
    values.emplace(TEXT("alpha"));
    values.add(FString{TEXT("beta")});

    CHECK(values.num() == 2);
    CHECK(values[0] == TEXT("alpha"));
    CHECK(values[1] == TEXT("beta"));
}

TEST_CASE("SandboxCore.TFrameArray handles practical int32 count conversions") {
    static constexpr int32 element_count{static_cast<int32>(std::numeric_limits<uint16>::max()) + 1};
    ml::TFrameArray<int32> values{};

    values.reserve(0);
    values.reserve(element_count);

    for (int32 i{0}; i < element_count; ++i) {
        values.add(i);
    }

    CHECK(values.num() == element_count);
    CHECK(values[0] == 0);
    CHECK(values[element_count - 1] == element_count - 1);
}

// Unreal's check macro terminates the Catch2 process. Negative counts, invalid indices, null
// resources, and attempts to grow beyond MAX_int32 remain guarded by production checks rather
// than being invoked from this process-wide test target.
