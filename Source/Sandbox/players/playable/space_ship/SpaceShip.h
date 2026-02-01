#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"

#include "SpaceShip.generated.h"

class UCameraComponent;
class USpringArmComponent;
class UStaticMeshComponent;
class UBoxComponent;

UCLASS()
class ASpaceShip : public APawn {
    GENERATED_BODY()
  public:
    struct Sockets {
        inline static FName const left{"Left"};
        inline static FName const right{"Right"};
        inline static FName const middle{"Middle"};
    };

    ASpaceShip();

    void Tick(float dt) override;

    void turn(FVector2D direction);
  protected:
    void BeginPlay() override;

    UPROPERTY(EditAnywhere, Category = "SpaceShip")
    UCameraComponent* camera{nullptr};
    UPROPERTY(EditAnywhere, Category = "SpaceShip")
    USpringArmComponent* spring_arm{nullptr};
    UPROPERTY(EditAnywhere, Category = "SpaceShip")
    UStaticMeshComponent* ship_mesh{nullptr};
    UPROPERTY(EditAnywhere, Category = "SpaceShip")
    UBoxComponent* collision_box{nullptr};

    UPROPERTY(EditAnywhere, Category = "SpaceShip")
    FVector velocity;

    UPROPERTY(EditAnywhere, Category = "SpaceShip")
    float cruise_speed{3000.0f};
    UPROPERTY(EditAnywhere, Category = "SpaceShip")
    float cruise_acceleration{3000.0f};

    UPROPERTY(EditAnywhere, Category = "SpaceShip")
    float boost_speed{5000.0f};
    UPROPERTY(EditAnywhere, Category = "SpaceShip")
    float boost_acceleration{5000.0f};

    UPROPERTY(EditAnywhere, Category = "SpaceShip")
    float brake_speed{1000.0f};
    UPROPERTY(EditAnywhere, Category = "SpaceShip")
    float brake_acceleration{1000.0f};

    UPROPERTY(EditAnywhere, Category = "SpaceShip")
    float rotation_speed{0.5f};
    UPROPERTY(EditAnywhere, Category = "SpaceShip")
    FVector2D rotation_input{FVector2D::ZeroVector};
};
