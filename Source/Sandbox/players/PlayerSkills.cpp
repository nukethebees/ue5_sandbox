#include "Sandbox/players/PlayerSkills.h"

namespace ml {
SANDBOX_API auto get_display_name(EPlayerSkillName value) -> FName {
    switch (value) {
        case EPlayerSkillName::Strength: {
            static FName const name{TEXT("Strength")};
            return name;
        }
        case EPlayerSkillName::Endurance: {
            static FName const name{TEXT("Endurance")};
            return name;
        }
        case EPlayerSkillName::Agility: {
            static FName const name{TEXT("Agility")};
            return name;
        }
        case EPlayerSkillName::Cyber: {
            static FName const name{TEXT("Cyber")};
            return name;
        }
        case EPlayerSkillName::Psi: {
            static FName const name{TEXT("Psi")};
            return name;
        }
        case EPlayerSkillName::hacking: {
            static FName const name{TEXT("Hacking")};
            return name;
        }
        case EPlayerSkillName::small_guns: {
            static FName const name{TEXT("Small Guns")};
            return name;
        }
        default: {
            break;
        }
    }

    static auto const unhandled_name{FName{TEXT("UNHANDLED_CASE")}};
    return unhandled_name;
}
SANDBOX_API auto get_display_string(EPlayerSkillName value) -> FString const& {
    switch (value) {
        case EPlayerSkillName::Strength: {
            static FString const name{get_display_name(value).ToString()};
            return name;
        }
        case EPlayerSkillName::Endurance: {
            static FString const name{get_display_name(value).ToString()};
            return name;
        }
        case EPlayerSkillName::Agility: {
            static FString const name{get_display_name(value).ToString()};
            return name;
        }
        case EPlayerSkillName::Cyber: {
            static FString const name{get_display_name(value).ToString()};
            return name;
        }
        case EPlayerSkillName::Psi: {
            static FString const name{get_display_name(value).ToString()};
            return name;
        }
        case EPlayerSkillName::hacking: {
            static FString const name{get_display_name(value).ToString()};
            return name;
        }
        case EPlayerSkillName::small_guns: {
            static FString const name{get_display_name(value).ToString()};
            return name;
        }
        default: {
            break;
        }
    }

    static FString const unhandled_name{TEXT("UNHANDLED_CASE")};
    return unhandled_name;
}
SANDBOX_API auto get_display_string_view(EPlayerSkillName value) -> TStringView<TCHAR> {
    return get_display_string(value);
}
SANDBOX_API auto get_display_text(EPlayerSkillName value) -> FText {
    return FText::FromStringView(get_display_string_view(value));
}
} // namespace ml
