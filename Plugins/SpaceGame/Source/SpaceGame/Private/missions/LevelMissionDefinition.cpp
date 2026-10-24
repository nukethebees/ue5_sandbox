#include <SpaceGame/missions/LevelMissionDefinition.h>
void FLevelMissionDefinition::replace_startup_actor(AActor const* old_actor, AActor& new_actor) {
    for (auto& binding : entity_bindings) {
        if (binding.Value == old_actor) {
            binding.Value = &new_actor;
        }
    }
}
