#pragma once

#include <Containers/ArrayView.h>
#include <CoreMinimal.h>
#include <Curves/SimpleCurve.h>
#include <Engine/CurveTable.h>

#include <ranges>
#include <type_traits>

class UObject;
class UClass;
class FAutomationTestBase;

namespace ml {
template <std::ranges::random_access_range Values, std::ranges::random_access_range Times>
    requires std::is_convertible_v<std::ranges::range_value_t<Values>, float> &&
             std::is_convertible_v<std::ranges::range_value_t<Times>, float>
void add_simple_curve_row(UCurveTable& curve_table,
                          FName const row_name,
                          Values const values,
                          Times const times) {
    check(std::ranges::size(values) == std::ranges::size(times));

    auto& curve{curve_table.AddSimpleCurve(row_name)};
    auto const count{static_cast<int32>(std::ranges::size(values))};
    for (int32 i{0}; i < count; ++i) {
        curve.AddKey(static_cast<float>(times[i]), static_cast<float>(values[i]));
    }
}

struct FTestResultAsset {
    FName package_name_prefix;
    FName asset_name_prefix;
    FAutomationTestBase* test_runner{nullptr};

    FTestResultAsset(FName test_name, FAutomationTestBase& test_runner);

    auto load_or_create(UClass* asset_class, FName output_name) const -> UObject*;

    template <typename T>
    auto load_or_create(FName output_name) const -> T*;

    void save(UObject& asset) const;
};

template <typename T>
auto FTestResultAsset::load_or_create(FName const output_name) const -> T* {
    return CastChecked<T>(load_or_create(T::StaticClass(), output_name));
}
}
