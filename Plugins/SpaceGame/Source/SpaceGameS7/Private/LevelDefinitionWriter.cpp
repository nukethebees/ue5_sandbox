#include <SpaceGameS7/LevelDefinitionWriter.h>

#include <sandbox/level_authoring/LevelDefinitionWriter.h>
#include <SandboxCoreEngine/strings.h>
#include <SpaceGame/levels/NativeLevelDefinitionConversion.h>

namespace ml::s7 {
auto emit_editor_level_source(FLevelDefinition const& definition, FStringView const level_config)
    -> std::expected<FString, FString> {
    auto const converted_config{FTCHARToUTF8{level_config.GetData(), level_config.Len()}};
    auto const config_utf8{std::string_view{converted_config.Get(),
                                            static_cast<std::size_t>(converted_config.Length())}};
    auto source{
        level_authoring::emit_editor_level_source(level_authoring::to_native(definition), config_utf8)};
    if (!source) {
        return std::unexpected{ml::to_fstring(source.error())};
    }
    return ml::to_fstring(*source);
}
} // namespace ml::s7
