#pragma once

#include <ioj/sim/entity_type.h>

#include <Containers/UnrealString.h>

namespace ml {
SPACEGAMEPRESENTATION_API auto get_entity_display_name(::ioj::sim::EntityType type)
    -> FString const&;
SPACEGAMEPRESENTATION_API auto get_entity_class_name(::ioj::sim::EntityType type) -> FString const&;
SPACEGAMEPRESENTATION_API auto get_entity_short_name(::ioj::sim::EntityType type) -> FString const&;
}
