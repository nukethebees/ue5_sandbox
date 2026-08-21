#pragma once

class AActor;

namespace ml {
void check_describable_actors_are_visible_to_hitscan();
bool describable_actor_can_be_hitcan(AActor& actor);
}
