#pragma once

#include <Sandbox/batch_game/TestEntityType.h>
#include <Sandbox/batch_game/TestTeam.h>

#include <SandboxCore/array_utils.h>
#include <SandboxCore/soa_array_mixin.h>
#include <SandboxCore/soa_vectors.h>

#include <Containers/Array.h>
#include <Containers/ArrayView.h>
#include <HAL/Platform.h>

#include <type_traits>

template <bool is_const>
struct TTestEntityDataView : public ml::FSoAViewMixin {
    template <typename T>
    using TView = std::conditional_t<is_const, TConstArrayView<T>, TArrayView<T>>;
    template <typename T>
    using TSoaView = std::conditional_t<is_const, TVectors3View<T const>, TVectors3View<T>>;

    using ThisClass = TTestEntityDataView<is_const>;

    TSoaView<float> locations;
    TSoaView<float> velocities;
    TView<int32> healths;
    TView<ETestTeam> teams;
    TView<ETestEntityType> entity_types;
    TView<uint8> alive;

    auto get_slice(int32 const offset, int32 const count) const {
        return ThisClass{
            {},
            locations.slice(offset, count),
            velocities.slice(offset, count),
            healths.Slice(offset, count),
            teams.Slice(offset, count),
            entity_types.Slice(offset, count),
            alive.Slice(offset, count),
        };
    }
    auto right(int32 const count) const {
        return ThisClass{
            {},
            locations.right(count),
            velocities.right(count),
            healths.Right(count),
            teams.Right(count),
            entity_types.Right(count),
            alive.Right(count),
        };
    }

    template <typename TFunc>
    auto apply_arrays(this auto&& self, TFunc&& func) -> decltype(auto) {
        return std::forward<TFunc>(func)(self.locations,
                                         self.velocities,
                                         self.healths,
                                         self.teams,
                                         self.entity_types,
                                         self.alive);
    }
};

struct FTestEntityRegistryEntityData : public ml::FSoAArrayMixin {
    using View = TTestEntityDataView<false>;
    using ConstView = TTestEntityDataView<true>;

    auto get_view() -> View;
    auto get_const_view() const -> ConstView;

    void add_disabled(int32 const count);
    void add(ConstView const view);

    void set_all_alive();
    void set_all_velocities(float const v);
    void set_all_entity_types(ETestEntityType const v);

    auto take_right(int32 const count);

    template <typename TFunc>
    auto apply_arrays(this auto&& self, TFunc&& func) -> decltype(auto) {
        return std::forward<TFunc>(func)(self.locations,
                                         self.velocities,
                                         self.healths,
                                         self.teams,
                                         self.entity_types,
                                         self.alive);
    }

    FVectors3f locations;
    FVectors3f velocities;
    TArray<int32> healths;
    TArray<ETestTeam> teams;
    TArray<ETestEntityType> entity_types;
    TArray<uint8> alive;
};
