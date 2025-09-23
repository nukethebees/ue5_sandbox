#pragma once

#include "Sandbox/data/video_options/VisualQualityLevel.h"

FText const& GetQualityLevelDisplayName(EVisualQualityLevel level) {
    static FText const auto_text{FText::FromString(TEXT("Auto"))};
    static FText const low_text{FText::FromString(TEXT("Low"))};
    static FText const medium_text{FText::FromString(TEXT("Medium"))};
    static FText const high_text{FText::FromString(TEXT("High"))};
    static FText const epic_text{FText::FromString(TEXT("Epic"))};
    static FText const cinematic_text{FText::FromString(TEXT("Cinematic"))};
    static FText const unknown_text{FText::FromString(TEXT("Unknown"))};

    switch (level) {
        case EVisualQualityLevel::Auto:
            return auto_text;
        case EVisualQualityLevel::Low:
            return low_text;
        case EVisualQualityLevel::Medium:
            return medium_text;
        case EVisualQualityLevel::High:
            return high_text;
        case EVisualQualityLevel::Epic:
            return epic_text;
        case EVisualQualityLevel::Cinematic:
            return cinematic_text;
        default:
            return unknown_text;
    }
}
