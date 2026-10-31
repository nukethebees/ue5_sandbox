#include <SpaceGame/levels/native_level_definition_conversion.h>

#include <SandboxCoreEngine/strings.h>

namespace ioj::levels::authoring {
auto to_unreal(LevelId const& id) -> FLevelId {
    return FLevelId{FName{to_fstring(id.value)}};
}
}
