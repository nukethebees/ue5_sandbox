#include "TestEntityType.h"

namespace {
auto get_entity_name(ETestEntityType const type,
                     TCHAR const* const player_ship,
                     TCHAR const* const turret,
                     TCHAR const* const capital_ship,
                     TCHAR const* const capital_ship_fighter,
                     TCHAR const* const tube_spinner) -> FString const& {
    switch (type) {
        case ETestEntityType::PlayerShip: {
            static FString const name{player_ship};
            return name;
        }
        case ETestEntityType::Turret: {
            static FString const name{turret};
            return name;
        }
        case ETestEntityType::CapitalShip: {
            static FString const name{capital_ship};
            return name;
        }
        case ETestEntityType::CapitalShipFighter: {
            static FString const name{capital_ship_fighter};
            return name;
        }
        case ETestEntityType::TubeSpinner: {
            static FString const name{tube_spinner};
            return name;
        }
        default: {
            static FString const name{TEXT("Unhandled")};
            return name;
        }
    }
}
}

namespace ml {
auto get_entity_display_name(ETestEntityType const type) -> FString const& {
    return get_entity_name(type,
                           TEXT("Player Ship"),
                           TEXT("Turret"),
                           TEXT("Capital Ship"),
                           TEXT("Capital Ship Fighter"),
                           TEXT("Tube Spinner"));
}

auto get_entity_class_name(ETestEntityType const type) -> FString const& {
    return get_entity_name(type,
                           TEXT("PlayerShip"),
                           TEXT("Turret"),
                           TEXT("CapitalShip"),
                           TEXT("CapitalShipFighter"),
                           TEXT("TubeSpinner"));
}

auto get_entity_short_name(ETestEntityType const type) -> FString const& {
    return get_entity_name(type,
                           TEXT("Player"),
                           TEXT("Turret"),
                           TEXT("Capital"),
                           TEXT("Fighter"),
                           TEXT("Spinner"));
}
}
