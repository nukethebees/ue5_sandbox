#pragma once

#include "TestTeam.h"

#include <Containers/Array.h>
#include <Containers/ArrayView.h>
#include <HAL/Platform.h>

#include <array>

#include "TestEntityRegistryData.generated.h"

USTRUCT()
struct FTestEntityRegistryEntityData {
    GENERATED_BODY()

    template <template <typename> typename TView>
    struct TTestEntityDataView {
        using ThisClass = TTestEntityDataView<TView>;

        TView<FVector> locations;
        TView<FVector> velocities;
        TView<int32> healths;
        TView<ETestTeam> teams;
        TView<uint8> alive;

        auto get_num() const -> int32 { return locations.Num(); }
        auto get_slice(int32 const offset, int32 const count) const {
            return ThisClass{
                locations.Slice(offset, count),
                velocities.Slice(offset, count),
                healths.Slice(offset, count),
                teams.Slice(offset, count),
                alive.Slice(offset, count),
            };
        }
    };

    using View = TTestEntityDataView<TArrayView>;
    using ConstView = TTestEntityDataView<TConstArrayView>;

    FTestEntityRegistryEntityData() = default;

    auto get_num() const -> int32;
    auto get_view() -> View;
    auto get_const_view() const -> ConstView;

    void add_uninitialised(int32 const count);
    void add_disabled(int32 const count);
    void add(ConstView const view);

    UPROPERTY()
    TArray<FVector> locations;
    UPROPERTY()
    TArray<FVector> velocities;
    UPROPERTY()
    TArray<int32> healths;
    UPROPERTY()
    TArray<ETestTeam> teams;
    UPROPERTY()
    TArray<uint8> alive;
};
