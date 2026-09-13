#include <SpaceGameS7/LevelDefinitionWriter.h>

#include <sandbox/level_authoring/LevelDefinitionWriter.h>
#include <SpaceGame/levels/NativeLevelDefinitionConversion.h>

#include <Containers/StringConv.h>

namespace ml::s7 {
namespace {
auto writer_to_fstring(std::string_view const value) -> FString {
    auto const converted{FUTF8ToTCHAR{value.data(), static_cast<int32>(value.size())}};
    return FString{converted.Length(), converted.Get()};
}
} // namespace

auto emit_initial_level_source(FLevelDefinition const& definition)
    -> std::expected<FString, FString> {
    auto source{level_authoring::emit_initial_level_source(level_authoring::to_native(definition))};
    if (!source) {
        return std::unexpected{writer_to_fstring(source.error())};
    }
    return writer_to_fstring(*source);
}
} // namespace ml::s7
