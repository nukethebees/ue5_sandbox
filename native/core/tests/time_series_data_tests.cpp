#include "sandbox/core/time_series_data.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <string>

TEST(TimeSeriesData, StoresValuesAndResets) {
    ml::TimeSeriesData<std::string> data;
    std::string const first{"first"};

    data.add(1.5, first);
    data.add(3.0, std::string{"second"});

    ASSERT_EQ(data.num(), 2);
    EXPECT_EQ(data.time_at(0), 1.5);
    EXPECT_EQ(data.value_at(0), "first");
    EXPECT_EQ(data.last_time(), 3.0);
    EXPECT_EQ(data.last_value(), "second");

    data.reset();
    EXPECT_TRUE(data.is_empty());
    EXPECT_TRUE(data.times().empty());
    EXPECT_TRUE(data.values().empty());
}

TEST(TimeSeriesData, ForwardsMoveOnlyValues) {
    ml::TimeSeriesData<std::unique_ptr<std::int32_t>> data;
    auto value{std::make_unique<std::int32_t>(42)};

    data.add(1.0, std::move(value));

    EXPECT_EQ(value, nullptr);
    ASSERT_NE(data.value_at(0), nullptr);
    EXPECT_EQ(*data.value_at(0), 42);
}

TEST(TimeSeriesData, FindsNearestValueAndResolvesTiesEarlier) {
    ml::XYSeriesData<std::uint64_t, std::int32_t> data;
    data.add(10, 100);
    data.add(20, 200);
    data.add(40, 400);

    EXPECT_EQ(data.nearest_index(0), 0);
    EXPECT_EQ(data.nearest_index(13), 0);
    EXPECT_EQ(data.nearest_index(15), 0);
    EXPECT_EQ(data.nearest_index(18), 1);
    EXPECT_EQ(data.nearest_index(50), 2);
    EXPECT_EQ(data.nearest_value(18), 200);
    EXPECT_EQ(data.nearest_time(18), 20);
}

TEST(TimeSeriesData, ReportsCapacityAndAllocatedBytes) {
    ml::TimeSeriesData<std::int32_t> data;
    data.reserve(16);

    EXPECT_GE(data.time_capacity(), 16);
    EXPECT_GE(data.value_capacity(), 16);
    EXPECT_GE(data.allocated_bytes(), 16 * (sizeof(double) + sizeof(std::int32_t)));
}
