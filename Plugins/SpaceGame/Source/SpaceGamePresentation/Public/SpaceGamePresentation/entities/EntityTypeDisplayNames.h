#pragma once

#include <ioj/sim/entities/entity_type.h>

#include <Containers/UnrealString.h>

namespace ioj {
SPACEGAMEPRESENTATION_API auto get_entity_display_name(sim::EntityType type) -> FString const&;
SPACEGAMEPRESENTATION_API auto get_entity_class_name(sim::EntityType type) -> FString const&;
SPACEGAMEPRESENTATION_API auto get_entity_short_name(sim::EntityType type) -> FString const&;
}
