#include <SandboxCore/time_series_data.h>

#include "CoreMinimal.h"
#include "TestHarness.h"

#include <memory>

TEST_CASE("SandboxCore.TimeSeriesData.Empty series reports no entries") {
    ml::TimeSeriesData<int32> data;

    CHECK(data.is_empty());
    CHECK(data.num() == 0);
    CHECK(data.last_index() == INDEX_NONE);
    CHECK(data.nearest_index(10.0) == INDEX_NONE);
}

TEST_CASE("SandboxCore.TimeSeriesData.Add stores times and values in order") {
    ml::TimeSeriesData<FString> data;
    FString const first_value{TEXT("first")};

    data.add(1.5, first_value);
    data.add(3.0, FString{TEXT("second")});

    CHECK_FALSE(data.is_empty());
    CHECK(data.num() == 2);
    CHECK(data.time_at(0) == 1.5);
    CHECK(data.value_at(0) == TEXT("first"));
    CHECK(data.time_at(1) == 3.0);
    CHECK(data.value_at(1) == TEXT("second"));
    CHECK(data.last_index() == 1);
    CHECK(data.last_time() == 3.0);
    CHECK(data.last_value() == TEXT("second"));
}

TEST_CASE("SandboxCore.TimeSeriesData.Add forwards move-only values") {
    ml::TimeSeriesData<std::unique_ptr<int32>> data;
    auto value{std::make_unique<int32>(42)};

    data.add(1.0, std::move(value));

    CHECK(value == nullptr);
    REQUIRE(data.value_at(0) != nullptr);
    CHECK(*data.value_at(0) == 42);
}

TEST_CASE("SandboxCore.TimeSeriesData.Nearest lookup finds closest entry") {
    ml::TimeSeriesData<int32> data;
    data.add(1.0, 10);
    data.add(4.0, 40);
    data.add(10.0, 100);

    SECTION("before the first entry") {
        CHECK(data.nearest_index(-5.0) == 0);
        CHECK(data.nearest_time(-5.0) == 1.0);
        CHECK(data.nearest_value(-5.0) == 10);
    }

    SECTION("at an entry") {
        CHECK(data.nearest_index(4.0) == 1);
        CHECK(data.nearest_time(4.0) == 4.0);
        CHECK(data.nearest_value(4.0) == 40);
    }

    SECTION("between entries") {
        CHECK(data.nearest_index(8.0) == 2);
        CHECK(data.nearest_time(8.0) == 10.0);
        CHECK(data.nearest_value(8.0) == 100);
    }

    SECTION("after the last entry") {
        CHECK(data.nearest_index(20.0) == 2);
        CHECK(data.nearest_time(20.0) == 10.0);
        CHECK(data.nearest_value(20.0) == 100);
    }
}

TEST_CASE("SandboxCore.TimeSeriesData.Nearest lookup resolves ties to earlier entry") {
    ml::TimeSeriesData<int32> data;
    data.add(2.0, 20);
    data.add(6.0, 60);

    CHECK(data.nearest_index(4.0) == 0);
    CHECK(data.nearest_time(4.0) == 2.0);
    CHECK(data.nearest_value(4.0) == 20);
}
