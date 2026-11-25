#pragma once

#include <ioj/settings/game_setting_category.h>

#include <Misc/Optional.h>

namespace ioj {
enum class EMenuPage : uint8 {
    SelectMission,
    DataArchive,
    Video,
    Gameplay,
    Audio,
    Controls,
    Accessibility,
    System,
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
        case EMenuPage::SelectMission:
        case EMenuPage::DataArchive:
        case EMenuPage::System:
            return {};
    }
    checkNoEntry();
    return {};
}
}
