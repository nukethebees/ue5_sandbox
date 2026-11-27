#pragma once

#include <ioj/input/menu_navigation.h>
#include <ioj/settings/game_setting_category.h>

#include <Misc/Optional.h>

namespace ioj {
enum class EMenuPage : uint8 {
    SelectLevel = static_cast<uint8>(MainMenuItem::SelectLevel),
    DataArchive = static_cast<uint8>(MainMenuItem::DataArchive),
    Video = static_cast<uint8>(MainMenuItem::Video),
    Gameplay = static_cast<uint8>(MainMenuItem::Gameplay),
    Audio = static_cast<uint8>(MainMenuItem::Audio),
    Controls = static_cast<uint8>(MainMenuItem::Controls),
    Accessibility = static_cast<uint8>(MainMenuItem::Accessibility),
    System = static_cast<uint8>(MainMenuItem::System),
};

inline auto settings_category(EMenuPage page) -> TOptional<GameSettingCategory> {
    switch (page) {
        case EMenuPage::Video:
            return GameSettingCategory::Video;
        case EMenuPage::Gameplay:
            return GameSettingCategory::Gameplay;
        case EMenuPage::Audio:
            return GameSettingCategory::Audio;
        case EMenuPage::Controls:
            return GameSettingCategory::Controls;
        case EMenuPage::Accessibility:
            return GameSettingCategory::Accessibility;
        case EMenuPage::SelectLevel:
        case EMenuPage::DataArchive:
        case EMenuPage::System:
            return {};
    }
    checkNoEntry();
    return {};
}
}
