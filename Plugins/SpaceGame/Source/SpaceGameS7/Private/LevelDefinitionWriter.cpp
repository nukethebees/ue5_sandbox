#include <SpaceGameS7/LevelDefinitionWriter.h>

#include <sandbox/level_authoring/LevelDefinitionWriter.h>
#include <SandboxCoreEngine/strings.h>
#include <SpaceGame/levels/NativeLevelDefinitionConversion.h>

namespace ml::s7 {
auto emit_editor_level_source(FLevelDefinition const& definition)
    -> std::expected<FString, FString> {
    auto source{level_authoring::emit_editor_level_source(level_authoring::to_native(definition))};
    if (!source) {
        return std::unexpected{ml::to_fstring(source.error())};
    }
    return ml::to_fstring(*source);
}
} // namespace ml::s7
