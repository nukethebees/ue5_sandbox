#include "SpaceGame/input/CanonicalShipControls.h"

#include "InputMappingContext.h"

namespace ioj {
namespace ship_control_context_details {
inline constexpr FCanonicalShipControlContext contexts[]{
    {EShipControlScope::General, TEXT("IMC_Ship_General"), TEXT("/SpaceGame/Input/SpaceShip/")},
    {EShipControlScope::Starfox, TEXT("IMC_Ship_Starfox"), TEXT("/SpaceGame/Input/SpaceShip/")},
    {EShipControlScope::Fighter, TEXT("IMC_Ship_Fighter"), TEXT("/SpaceGame/Input/SpaceShip/")},
    {EShipControlScope::Skater, TEXT("IMC_Ship_Skater"), TEXT("/SpaceGame/Input/SpaceShip/")},
    {EShipControlScope::Gunship, TEXT("IMC_Ship_Gunship"), TEXT("/SpaceGame/Input/SpaceShip/")},
};
} // namespace ship_control_context_details

auto canonical_ship_control_contexts() -> TConstArrayView<FCanonicalShipControlContext> {
    return ship_control_context_details::contexts;
}
auto canonical_ship_control_context(EShipControlScope const scope)
    -> FCanonicalShipControlContext const& {
    auto const index{static_cast<int32>(scope)};
    check(index >= 0 && index < UE_ARRAY_COUNT(ship_control_context_details::contexts));
    return ship_control_context_details::contexts[index];
}
auto flight_control_scope(sim::player::FlightModelSlot const slot) -> EShipControlScope {
    switch (slot) {
        case sim::player::FlightModelSlot::First:
            return EShipControlScope::Starfox;
        case sim::player::FlightModelSlot::Second:
            return EShipControlScope::Fighter;
        case sim::player::FlightModelSlot::Third:
            return EShipControlScope::Skater;
        case sim::player::FlightModelSlot::Fourth:
            return EShipControlScope::Gunship;
    }
    checkNoEntry();
    return EShipControlScope::Starfox;
}
auto flight_model_slot(EShipControlScope const scope) -> sim::player::FlightModelSlot {
    switch (scope) {
        case EShipControlScope::Starfox:
            return sim::player::FlightModelSlot::First;
        case EShipControlScope::Fighter:
            return sim::player::FlightModelSlot::Second;
        case EShipControlScope::Skater:
            return sim::player::FlightModelSlot::Third;
        case EShipControlScope::Gunship:
            return sim::player::FlightModelSlot::Fourth;
        case EShipControlScope::General:
            break;
    }
    checkNoEntry();
    return sim::player::FlightModelSlot::First;
}
auto load_ship_control_context(EShipControlScope const scope) -> UInputMappingContext* {
    auto const& definition{canonical_ship_control_context(scope)};
    auto const package{
        FString::Printf(TEXT("%s%s"), definition.package_path, definition.asset_name)};
    auto const object_path{FString::Printf(TEXT("%s.%s"), *package, definition.asset_name)};
    return LoadObject<UInputMappingContext>(nullptr, *object_path);
}
} // namespace ioj
