#pragma once

#include <CoreMinimal.h>

class UObject;
class UClass;
class FAutomationTestBase;

namespace ml {
struct FTestResultAsset {
    FName package_name;
    FName asset_name;
    FAutomationTestBase* test_runner{nullptr};

    FTestResultAsset(FName test_name, FAutomationTestBase& test_runner);

    auto load_or_create(UClass* asset_class) const -> UObject*;

    template <typename T>
    auto load_or_create() const -> T*;

    void save(UObject& asset) const;
};

template <typename T>
auto FTestResultAsset::load_or_create() const -> T* {
    return CastChecked<T>(load_or_create(T::StaticClass()));
}
}
