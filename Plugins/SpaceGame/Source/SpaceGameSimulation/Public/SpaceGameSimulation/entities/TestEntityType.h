#pragma once

#include <ioj/sim/entity_type.h>
#include <SandboxCore/enum_array.h>

using ETestEntityType = ::ioj::sim::EntityType;

template <>
struct TEnumTraits<::ioj::sim::EntityType> {
    static constexpr int32 count{static_cast<int32>(::ioj::sim::EntityType::COUNT)};
};

namespace ml {
SPACEGAMESIMULATION_API auto get_entity_display_name(::ioj::sim::EntityType type) -> FString const&;
SPACEGAMESIMULATION_API auto get_entity_class_name(::ioj::sim::EntityType type) -> FString const&;
SPACEGAMESIMULATION_API auto get_entity_short_name(::ioj::sim::EntityType type) -> FString const&;
} // namespace ml
