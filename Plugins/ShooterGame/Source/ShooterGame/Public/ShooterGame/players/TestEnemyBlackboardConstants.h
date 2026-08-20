#pragma once

#include "CoreMinimal.h"

struct TestEnemyBlackboardConstants {
    struct FName {
        static auto acceptable_radius() -> ::FName {
            static ::FName const v{TEXT("acceptable_radius")};
            return v;
        }
        static auto attack_radius() -> ::FName {
            static ::FName const v{TEXT("attack_radius")};
            return v;
        }
        static auto default_ai_state() -> ::FName {
            static ::FName const v{TEXT("default_ai_state")};
            return v;
        }
        static auto ai_state() -> ::FName {
            static ::FName const v{TEXT("ai_state")};
            return v;
        }
        static auto mob_attack_mode() -> ::FName {
            static ::FName const v{TEXT("mob_attack_mode")};
            return v;
        }
        static auto target_actor() -> ::FName {
            static ::FName const v{TEXT("target_actor")};
            return v;
        }
        static auto last_known_location() -> ::FName {
            static ::FName const v{TEXT("last_known_location")};
            return v;
        }
        static auto defend_actor() -> ::FName {
            static ::FName const v{TEXT("defend_actor")};
            return v;
        }
        static auto defend_position() -> ::FName {
            static ::FName const v{TEXT("defend_position")};
            return v;
        }
    };
    struct FString {
        static auto acceptable_radius() -> ::FString const& {
            static ::FString const v{TEXT("acceptable_radius")};
            return v;
        }
        static auto attack_radius() -> ::FString const& {
            static ::FString const v{TEXT("attack_radius")};
            return v;
        }
        static auto default_ai_state() -> ::FString const& {
            static ::FString const v{TEXT("default_ai_state")};
            return v;
        }
        static auto ai_state() -> ::FString const& {
            static ::FString const v{TEXT("ai_state")};
            return v;
        }
        static auto mob_attack_mode() -> ::FString const& {
            static ::FString const v{TEXT("mob_attack_mode")};
            return v;
        }
        static auto target_actor() -> ::FString const& {
            static ::FString const v{TEXT("target_actor")};
            return v;
        }
        static auto last_known_location() -> ::FString const& {
            static ::FString const v{TEXT("last_known_location")};
            return v;
        }
        static auto defend_actor() -> ::FString const& {
            static ::FString const v{TEXT("defend_actor")};
            return v;
        }
        static auto defend_position() -> ::FString const& {
            static ::FString const v{TEXT("defend_position")};
            return v;
        }
    };
    struct FText {
        static auto acceptable_radius() -> ::FText const& {
            static ::FText const v{::FText::FromName(TEXT("acceptable_radius"))};
            return v;
        }
        static auto attack_radius() -> ::FText const& {
            static ::FText const v{::FText::FromName(TEXT("attack_radius"))};
            return v;
        }
        static auto default_ai_state() -> ::FText const& {
            static ::FText const v{::FText::FromName(TEXT("default_ai_state"))};
            return v;
        }
        static auto ai_state() -> ::FText const& {
            static ::FText const v{::FText::FromName(TEXT("ai_state"))};
            return v;
        }
        static auto mob_attack_mode() -> ::FText const& {
            static ::FText const v{::FText::FromName(TEXT("mob_attack_mode"))};
            return v;
        }
        static auto target_actor() -> ::FText const& {
            static ::FText const v{::FText::FromName(TEXT("target_actor"))};
            return v;
        }
        static auto last_known_location() -> ::FText const& {
            static ::FText const v{::FText::FromName(TEXT("last_known_location"))};
            return v;
        }
        static auto defend_actor() -> ::FText const& {
            static ::FText const v{::FText::FromName(TEXT("defend_actor"))};
            return v;
        }
        static auto defend_position() -> ::FText const& {
            static ::FText const v{::FText::FromName(TEXT("defend_position"))};
            return v;
        }
    };
    struct TStringView {
        static auto acceptable_radius() -> ::TStringView<TCHAR> {
            static ::TStringView const v{TEXT("acceptable_radius")};
            return v;
        }
        static auto attack_radius() -> ::TStringView<TCHAR> {
            static ::TStringView const v{TEXT("attack_radius")};
            return v;
        }
        static auto default_ai_state() -> ::TStringView<TCHAR> {
            static ::TStringView const v{TEXT("default_ai_state")};
            return v;
        }
        static auto ai_state() -> ::TStringView<TCHAR> {
            static ::TStringView const v{TEXT("ai_state")};
            return v;
        }
        static auto mob_attack_mode() -> ::TStringView<TCHAR> {
            static ::TStringView const v{TEXT("mob_attack_mode")};
            return v;
        }
        static auto target_actor() -> ::TStringView<TCHAR> {
            static ::TStringView const v{TEXT("target_actor")};
            return v;
        }
        static auto last_known_location() -> ::TStringView<TCHAR> {
            static ::TStringView const v{TEXT("last_known_location")};
            return v;
        }
        static auto defend_actor() -> ::TStringView<TCHAR> {
            static ::TStringView const v{TEXT("defend_actor")};
            return v;
        }
        static auto defend_position() -> ::TStringView<TCHAR> {
            static ::TStringView const v{TEXT("defend_position")};
            return v;
        }
    };
};
