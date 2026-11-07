#pragma once

class AActor;
class UWorld;

namespace ioj {
SPACEGAME_API void reset_level_world(UWorld& world, AActor const& retained_actor);
}
