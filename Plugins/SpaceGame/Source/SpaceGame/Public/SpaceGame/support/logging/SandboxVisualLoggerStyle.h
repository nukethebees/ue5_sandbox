#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"

#include "SandboxVisualLoggerStyle.generated.h"

USTRUCT(BlueprintType)
struct FSandboxVisualLoggerEntityStyle {
    GENERATED_BODY()

    UPROPERTY(EditDefaultsOnly)
    FColor friendly_entity_colour{FColor::Green};

    UPROPERTY(EditDefaultsOnly)
    FColor enemy_entity_colour{FColor::Red};

    UPROPERTY(EditDefaultsOnly)
    FColor neutral_entity_colour{FColor::Silver};

    UPROPERTY(EditDefaultsOnly)
    FColor capital_ship_colour{FColor::Cyan};

    UPROPERTY(EditDefaultsOnly, meta = (ClampMin = "0.0", UIMin = "0.0", Units = "cm"))
    float fighter_entity_radius{100.f};

    UPROPERTY(EditDefaultsOnly, meta = (ClampMin = "0.0", UIMin = "0.0", Units = "cm"))
    FVector3f capital_ship_box_extent{500.f, 500.f, 500.f};
};

USTRUCT(BlueprintType)
struct FSandboxVisualLoggerNavigationStyle {
    GENERATED_BODY()

    UPROPERTY(EditDefaultsOnly)
    FColor movement_destination_colour{FColor::Yellow};

    UPROPERTY(EditDefaultsOnly)
    FColor parent_to_child_line_colour{FColor::Blue};
};

USTRUCT(BlueprintType)
struct FSandboxVisualLoggerCombatStyle {
    GENERATED_BODY()

    UPROPERTY(EditDefaultsOnly)
    FColor selected_target_colour{FColor::Orange};

    UPROPERTY(EditDefaultsOnly)
    FColor successful_los_colour{FColor::Green};

    UPROPERTY(EditDefaultsOnly)
    FColor failed_los_colour{FColor::Red};
};

USTRUCT(BlueprintType)
struct FSandboxVisualLoggerLineStyle {
    GENERATED_BODY()

    UPROPERTY(EditDefaultsOnly, meta = (ClampMin = "0.0", UIMin = "0.0"))
    float normal_line_thickness{1.f};

    UPROPERTY(EditDefaultsOnly, meta = (ClampMin = "0.0", UIMin = "0.0"))
    float highlighted_line_thickness{3.f};
};

UCLASS(BlueprintType)
class SPACEGAME_API USandboxVisualLoggerStyle : public UDataAsset {
    GENERATED_BODY()
  public:
    UPROPERTY(EditDefaultsOnly, Category = "Visual Logger")
    FSandboxVisualLoggerEntityStyle entities;

    UPROPERTY(EditDefaultsOnly, Category = "Visual Logger")
    FSandboxVisualLoggerNavigationStyle navigation;

    UPROPERTY(EditDefaultsOnly, Category = "Visual Logger")
    FSandboxVisualLoggerCombatStyle combat;

    UPROPERTY(EditDefaultsOnly, Category = "Visual Logger")
    FSandboxVisualLoggerLineStyle lines;
};
