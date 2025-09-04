#include "Sandbox/utilities/levels.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/TopLevelAssetPath.h"

namespace ml {
auto get_all_level_names(FName level_directory) -> TArray<FName> {
    TArray<FName> level_names;

    auto& asset_registry_module{
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry")};
    auto& asset_registry{asset_registry_module.Get()};

    FARFilter filter;
    filter.ClassPaths.Add(FTopLevelAssetPath(FName("/Script/Engine.World")));
    filter.bRecursivePaths = true;
    filter.PackagePaths.Add(level_directory);

    TArray<FAssetData> asset_data;
    asset_registry.GetAssets(filter, asset_data);

    for (auto const& data : asset_data) {
        level_names.Add(data.AssetName); // Or use data.PackageName for full path
    }

    return level_names;
}

auto format_level_display_name(FName level_name) -> FString {
    if (level_name.IsNone()) {
        return FString("");
    }

    static constexpr auto SPACE{TCHAR(' ')};
    static constexpr auto UNDERSCORE{TCHAR('_')};
    static constexpr auto NULL_CHAR{TCHAR('\0')};

    auto x{level_name.ToString()};
    FString out{};

    bool ready_for_word{true};
    int32 i{0};
    int32 const name_len{x.Len()};
    int32 const peek_end{name_len - 1};

    auto peek{[&]() {
        if (i < peek_end) {
            return x[i + 1];
        } else {
            return TCHAR('\0');
        }
    }};
    auto next_is_lower{[&]() -> bool { return FChar::IsLower(peek()); }};
    auto at_end{[&]() { return i >= name_len; }};
    auto get_prev{[&]() {
        auto const out_len{out.Len()};
        return out_len ? out[out_len - 1] : TCHAR('\0');
    }};

    while (!at_end()) {
        auto c{x[i]};
        ++i;

        // Convert underscores to spaces
        if (c == UNDERSCORE) {
            auto const prev{get_prev()};

            // Don't add leading spaces
            if (prev == NULL_CHAR) {
                continue;
            }

            if (prev != SPACE) {
                out.AppendChar(SPACE);
            }

            ready_for_word = true;
            continue;
        }

        // If the word is uppercase then leave it
        // Assuming uppercase marks the start of a word then consume the remaining lowercase words
        if (FChar::IsUpper(c)) {
            // If the previous char wasn't a space then add one to mark the start of a word
            auto const prev{get_prev()};
            if ((prev != NULL_CHAR) && (prev != SPACE) && (!FChar::IsUpper(prev))) {
                out.AppendChar(SPACE);
            }

            // Add the uppercase char and increment the index
            out.AppendChar(c);
            ready_for_word = false;

            while (!at_end()) {
                c = x[i];
                if (FChar::IsLower(c)) {
                    out.AppendChar(c);
                    ++i;
                } else {
                    break;
                }
            }
            continue;
        }

        // If we had an underscore before then try and start a word
        if (ready_for_word && FChar::IsLower(c)) {
            out.AppendChar(FChar::ToUpper(c));
            ready_for_word = false;
            continue;
        }

        if (FChar::IsDigit(c)) {
            auto const prev{get_prev()};
            if ((prev != NULL_CHAR) && !FChar::IsDigit(prev) && (prev != SPACE)) {
                out.AppendChar(SPACE);
            }
        }

        out.AppendChar(c);
    }

    return out;
}
}
