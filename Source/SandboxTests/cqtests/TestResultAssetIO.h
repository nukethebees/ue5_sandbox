#pragma once

#include <Containers/ArrayView.h>
#include <CoreMinimal.h>
#include <Curves/SimpleCurve.h>
#include <Engine/CurveTable.h>

#include <type_traits>

class UObject;
class UClass;
class FAutomationTestBase;

namespace ml {
template <typename T, typename TimeType>
    requires std::is_convertible_v<T, float> && std::is_convertible_v<TimeType, float>
void add_simple_curve_row(UCurveTable& curve_table,
                          FName const row_name,
                          TConstArrayView<T> const values,
                          TConstArrayView<TimeType> const times) {
    check(values.Num() == times.Num());

    auto& curve{curve_table.AddSimpleCurve(row_name)};
    for (int32 i{0}; i < values.Num(); ++i) {
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
