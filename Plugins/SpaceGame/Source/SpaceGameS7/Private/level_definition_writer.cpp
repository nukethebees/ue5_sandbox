#include <SpaceGameS7/level_definition_writer.h>

#include <ioj/levels/authoring/level_definition_writer.h>
#include <SpaceGame/levels/native_level_definition_conversion.h>

#include <SandboxCoreEngine/strings.h>

namespace ioj::levels::authoring {
auto emit_editor_level_source(ml::FLevelDefinition const& definition)
    -> std::expected<FString, FString> {
    auto source{::ioj::levels::authoring::emit_editor_level_source(
        ::ioj::levels::authoring::to_native(definition))};
    if (!source) {
        return std::unexpected{ml::to_fstring(format_diagnostics(source.error()))};
    }
    return ml::to_fstring(*source);
}
} // namespace ioj::levels::authoring
