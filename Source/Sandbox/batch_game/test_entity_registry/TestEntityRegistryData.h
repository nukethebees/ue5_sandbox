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

namespace ml::entity_registry {

template <bool is_const>
struct EntityDataView : public ml::FSoAViewMixin {
    using View = EntityDataView<false>;
    using ConstView = EntityDataView<true>;

    template <typename T>
    using TView = std::conditional_t<is_const, TConstArrayView<T>, TArrayView<T>>;
    template <typename T>
    using TSoaView = std::conditional_t<is_const, TVectors3View<T const>, TVectors3View<T>>;

    using ThisClass = EntityDataView<is_const>;

    TSoaView<float> locations;
    TSoaView<float> velocities;
    TView<float> radii;
    TView<int32> healths;
    TView<ETestTeam> teams;
    TView<ETestEntityType> entity_types;
    TView<uint8> alive;

    template <typename TFunc>
    auto apply_arrays(this auto&& self, TFunc&& func) -> decltype(auto) {
        return std::forward<TFunc>(func)(self.locations,
                                         self.velocities,
                                         self.radii,
                                         self.healths,
                                         self.teams,
                                         self.entity_types,
                                         self.alive);
    }
};

struct EntityData : public ml::FSoAArrayMixin {
    using View = EntityDataView<false>;
    using ConstView = EntityDataView<true>;

    void add_disabled(int32 const count);
    void add(ConstView const view);

    void set_all_alive();
    void set_all_velocities(float const v);
    void set_all_entity_types(ETestEntityType const v);

    template <typename TFunc>
    auto apply_arrays(this auto&& self, TFunc&& func) -> decltype(auto) {
        return std::forward<TFunc>(func)(self.locations,
                                         self.velocities,
                                         self.radii,
                                         self.healths,
                                         self.teams,
                                         self.entity_types,
                                         self.alive);
    }

    FVectors3f locations;
    FVectors3f velocities;
    TArray<float> radii;
    TArray<int32> healths;
    TArray<ETestTeam> teams;
    TArray<ETestEntityType> entity_types;
    TArray<uint8> alive;
};
}
