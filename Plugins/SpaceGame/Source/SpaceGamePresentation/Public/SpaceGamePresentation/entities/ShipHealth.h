#pragma once

#include "CoreMinimal.h"

#include "ShipHealth.generated.h"

USTRUCT(BlueprintType)
struct SPACEGAMEPRESENTATION_API FShipHealth {
    GENERATED_BODY()

    inline static constexpr int32 default_max_health{100};
    inline static constexpr int32 upgraded_max_health{150};

    FShipHealth() = default;
    FShipHealth(int32 const health, int32 const max_health)
        : health{health}
        , max_health{max_health} {}
    explicit FShipHealth(int32 const max_health)
        : health{max_health}
        , max_health{max_health} {}

    bool operator==(FShipHealth const& other) const noexcept = default;

    void upgrade_max_health() { max_health = upgraded_max_health; }
    bool is_alive() const noexcept { return health > 0; }
    void clamp_to_max() noexcept;

    UPROPERTY(EditAnywhere, Category = "Health")
    int32 health{100};
    UPROPERTY(EditAnywhere, Category = "Health")
    int32 max_health{default_max_health};
};
