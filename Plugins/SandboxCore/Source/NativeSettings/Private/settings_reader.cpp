#include <NativeSettings/settings_reader.h>

#include <ioj/levels/authoring/definition_reader.h>

#include <IPlatformCrypto.h>
#include <Misc/Paths.h>
#include <Modules/ModuleManager.h>

#include <format>

IMPLEMENT_MODULE(FDefaultModuleImpl, NativeSettings)

namespace ioj::settings {
auto read_file(FStringView const path) -> std::expected<Settings, FString> {
    auto const project_directory{FPaths::ConvertRelativePathToFull(FPaths::ProjectDir())};
    auto const root{FPaths::Combine(project_directory, TEXT("LevelScripts"))};
    auto const full_path{FPaths::ConvertRelativePathToFull(project_directory, FString{path})};
    auto settings{
        levels::authoring::DefinitionReader{std::filesystem::path{*root}}.read_settings_file(
            std::filesystem::path{*full_path})};
    if (!settings) {
        auto const message{levels::format_diagnostics(settings.error())};
        return std::unexpected{FString{UTF8_TO_TCHAR(message.c_str())}};
    }
    return std::move(*settings);
}

auto source_hash(FStringView const previous_hash, Settings const& settings) -> FString {
    auto const previous{FTCHARToUTF8{previous_hash.GetData(), previous_hash.Len()}};
    std::string source{previous.Get(), static_cast<std::size_t>(previous.Length())};
    for (auto const& setting : settings) {
        std::format_to(std::back_inserter(source), "\n{}:{}=", setting.path.size(), setting.path);
        std::visit(
            [&source, &setting](auto const& value) {
                using Value = std::decay_t<decltype(value)>;
                if constexpr (std::same_as<Value, std::vector<double>>) {
                    source.append("vector:");
                    for (auto const number : value) {
                        std::format_to(std::back_inserter(source), "{};", number);
                    }
                } else if constexpr (std::same_as<Value, std::string>) {
                    std::format_to(std::back_inserter(source), "string:{}:{}", value.size(), value);
                } else {
                    std::format_to(
                        std::back_inserter(source), "{}:{}", setting.value.index(), value);
                }
            },
            setting.value);
    }

    auto context{IPlatformCrypto::Get().CreateContext()};
    TArray<uint8> hash;
    TArrayView<uint8 const> const bytes{reinterpret_cast<uint8 const*>(source.data()),
                                        static_cast<int32>(source.size())};
    if (!context || !context->CalcSHA256(bytes, hash)) {
        UE_LOG(LogTemp, Error, TEXT("Cannot hash evaluated game settings"));
        return {};
    }
    return BytesToHex(hash.GetData(), hash.Num()).ToLower();
}
}
