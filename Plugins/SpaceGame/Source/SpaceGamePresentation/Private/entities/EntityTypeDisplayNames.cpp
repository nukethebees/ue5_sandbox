#include "SpaceGamePresentation/entities/EntityTypeDisplayNames.h"

namespace ml::entity_type_display_names::Private {
auto get_entity_name(::ioj::sim::EntityType const type,
                     TCHAR const* const player_ship,
                     TCHAR const* const turret,
                     TCHAR const* const capital_ship,
                     TCHAR const* const capital_ship_fighter,
                     TCHAR const* const tube_spinner) -> FString const& {
    switch (type) {
        case ::ioj::sim::EntityType::PlayerShip: {
            static FString const name{player_ship};
            return name;
        }
        case ::ioj::sim::EntityType::Turret: {
            static FString const name{turret};
            return name;
        }
        case ::ioj::sim::EntityType::CapitalShip: {
            static FString const name{capital_ship};
            return name;
        }
        case ::ioj::sim::EntityType::Fighter: {
            static FString const name{capital_ship_fighter};
            return name;
        }
        case ::ioj::sim::EntityType::TubeSpinner: {
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
auto get_entity_display_name(::ioj::sim::EntityType const type) -> FString const& {
    return entity_type_display_names::Private::get_entity_name(type,
                                                               TEXT("Player Ship"),
                                                               TEXT("Turret"),
                                                               TEXT("Capital Ship"),
                                                               TEXT("Capital Ship Fighter"),
                                                               TEXT("Tube Spinner"));
}

auto get_entity_class_name(::ioj::sim::EntityType const type) -> FString const& {
    return entity_type_display_names::Private::get_entity_name(type,
                                                               TEXT("PlayerShip"),
                                                               TEXT("Turret"),
                                                               TEXT("CapitalShip"),
                                                               TEXT("Fighter"),
                                                               TEXT("TubeSpinner"));
}

auto get_entity_short_name(::ioj::sim::EntityType const type) -> FString const& {
    return entity_type_display_names::Private::get_entity_name(
        type, TEXT("Player"), TEXT("Turret"), TEXT("Capital"), TEXT("Fighter"), TEXT("Spinner"));
}
}
