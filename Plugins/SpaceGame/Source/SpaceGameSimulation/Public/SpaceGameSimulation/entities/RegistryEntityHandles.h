#pragma once

#include <ioj/sim/registry_entity_handles.h>

#include <Containers/Array.h>

namespace ml {
using NativeRegistryEntityHandles = ::ioj::sim::RegistryEntityHandles;

inline void append_registry_entity_handles(NativeRegistryEntityHandles const& source,
                                           TArray<::ioj::sim::RegistryEntityHandle>& destination) {
    auto const count{source.num()};
    destination.Reserve(destination.Num() + count);
    for (int32 index{}; index < count; ++index) {
        destination.Emplace(source.registry_indices[index], source.generations[index]);
    }
}

inline auto to_registry_entity_handle_array(NativeRegistryEntityHandles const& source)
    -> TArray<::ioj::sim::RegistryEntityHandle> {
    TArray<::ioj::sim::RegistryEntityHandle> result;
    append_registry_entity_handles(source, result);
    return result;
}
}

struct FRegistryEntityHandles : ml::NativeRegistryEntityHandles {
    auto operator[](int32 const index) const -> ::ioj::sim::RegistryEntityHandle {
        return {registry_indices[index], generations[index]};
    }

    void append_to(TArray<::ioj::sim::RegistryEntityHandle>& destination) const {
        ml::append_registry_entity_handles(*this, destination);
    }

    auto to_array() const -> TArray<::ioj::sim::RegistryEntityHandle> {
        return ml::to_registry_entity_handle_array(*this);
    }
};
