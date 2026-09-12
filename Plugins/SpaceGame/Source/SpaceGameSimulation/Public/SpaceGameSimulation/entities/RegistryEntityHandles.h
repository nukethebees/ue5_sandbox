#pragma once

#include <sandbox/simulation/registry_entity_handles.h>

#include <Containers/Array.h>

namespace ml {
using NativeRegistryEntityHandles = ::RegistryEntityHandles;
}

struct FRegistryEntityHandles : ml::NativeRegistryEntityHandles {
    auto operator[](int32 const index) const -> FRegistryEntityHandle {
        return {registry_indices[index], generations[index]};
    }

    void append_to(TArray<FRegistryEntityHandle>& destination) const {
        auto const count{num()};
        destination.Reserve(destination.Num() + count);
        for (int32 index{}; index < count; ++index) {
            destination.Emplace(registry_indices[index], generations[index]);
        }
    }

    auto to_array() const -> TArray<FRegistryEntityHandle> {
        TArray<FRegistryEntityHandle> result;
        append_to(result);
        return result;
    }
};
