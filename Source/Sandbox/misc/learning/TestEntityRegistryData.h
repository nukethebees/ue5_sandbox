#pragma once

#include "TestTeam.h"

#include <SandboxCore/soa_vectors.h>
#include <SandboxCore/soa_view_utils.h>

#include <Containers/Array.h>
#include <Containers/ArrayView.h>
#include <HAL/Platform.h>

#include <array>
#include <type_traits>

#include "TestEntityRegistryData.generated.h"

USTRUCT()
struct FTestEntityRegistryEntityData {
    GENERATED_BODY()

    template <bool is_const>
    struct TTestEntityDataView {
        template <typename T>
        using TView = std::conditional_t<is_const, TConstArrayView<T>, TArrayView<T>>;
        template <typename T>
        using TSoaView = std::conditional_t<is_const, TVectors3View<T const>, TVectors3View<T>>;

        using ThisClass = TTestEntityDataView<is_const>;

        TView<FVector> locations;
        TSoaView<float> velocities;
        TView<int32> healths;
        TView<ETestTeam> teams;
        TView<uint8> alive;

        auto get_num() const -> int32 { return locations.Num(); }
        auto get_slice(int32 const offset, int32 const count) const {
            return ThisClass{
                locations.Slice(offset, count),
                velocities.slice(offset, count),
                healths.Slice(offset, count),
                teams.Slice(offset, count),
                alive.Slice(offset, count),
            };
        }
        auto right(int32 const count) const {
            return ThisClass{
                locations.Right(count),
                velocities.right(count),
                healths.Right(count),
                teams.Right(count),
                alive.Right(count),
            };
        }
    };

    using View = TTestEntityDataView<false>;
    using ConstView = TTestEntityDataView<true>;

    FTestEntityRegistryEntityData() = default;

    auto get_num() const -> int32;
    auto get_view() -> View;
    auto get_const_view() const -> ConstView;

    void add_uninitialised(int32 const count);
    void add_disabled(int32 const count);
    void add(ConstView const view);

    void reset();

    UPROPERTY()
    TArray<FVector> locations;
    UPROPERTY()
    FVectors3f velocities;
    UPROPERTY()
    TArray<int32> healths;
    UPROPERTY()
    TArray<ETestTeam> teams;
    UPROPERTY()
    TArray<uint8> alive;
};
