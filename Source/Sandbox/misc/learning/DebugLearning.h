#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Math/Color.h"

#include "DebugLearning.generated.h"

class UStaticMeshComponent;

UCLASS()
class ADebugLearning : public AActor {
    GENERATED_BODY()
  public:
    ADebugLearning();

    void Tick(float dt) override;
  protected:
    void BeginPlay() override;

    UPROPERTY(EditAnywhere, Category = "Debug")
    TObjectPtr<UStaticMeshComponent> mesh{nullptr};

    // General
    UPROPERTY(EditAnywhere, Category = "Debug")
    float spacing{1000.f};
    UPROPERTY(EditAnywhere, Category = "Debug")
    FColor colour{FColor::Green};
    UPROPERTY(EditAnywhere, Category = "Debug")
    float thickness{8.f};

    // Lines
    UPROPERTY(EditAnywhere, Category = "Debug")
    float line_thickness{8.f};
    UPROPERTY(EditAnywhere, Category = "Debug")
    float line_length{1000.f};

    // General shape
    UPROPERTY(EditAnywhere, Category = "Debug")
    float radius{500.f};
    UPROPERTY(EditAnywhere, Category = "Debug")
    int32 segments{16};
    UPROPERTY(EditAnywhere, Category = "Debug")
    float length{1000.f};

    // Point
    UPROPERTY(EditAnywhere, Category = "Debug|Point")
    float point_size{10.f};

    // Arrow
    UPROPERTY(EditAnywhere, Category = "Debug|Arrow")
    float arrow_size{10.f};
    UPROPERTY(EditAnywhere, Category = "Debug|Arrow")
    FColor arrow_colour{FColor::Red};

    // Box
    UPROPERTY(EditAnywhere, Category = "Debug|Box")
    FColor box_colour{FColor::Green};

    // Coordinate system
    UPROPERTY(EditAnywhere, Category = "Debug|Coord")
    float coord_scale{10.f};

    // Circle
    UPROPERTY(EditAnywhere, Category = "Debug|Circle")
    bool draw_axis{true};

    // CircleArc
    UPROPERTY(EditAnywhere, Category = "Debug|CircleArc")
    FColor circle_arc_colour{FColor::Purple};
    UPROPERTY(EditAnywhere, Category = "Debug|CircleArc")
    float circle_arc_angle{45.0f};

    // Donut
    UPROPERTY(EditAnywhere, Category = "Debug|Donut")
    float donut_inner_radius{100.f};
    UPROPERTY(EditAnywhere, Category = "Debug|Donut")
    float donut_outer_radius{200.f};
    UPROPERTY(EditAnywhere, Category = "Debug|Donut")
    int32 donut_segments{16};
    UPROPERTY(EditAnywhere, Category = "Debug|Donut")
    FColor donut_colour{FColor::Orange};

    // Sphere
    UPROPERTY(EditAnywhere, Category = "Debug|Sphere")
    int32 sphere_segments{16};
    UPROPERTY(EditAnywhere, Category = "Debug|Sphere")
    FColor sphere_colour{FColor::Blue};

    // Cone
    UPROPERTY(EditAnywhere, Category = "Debug|Cone")
    float cone_angle_width{45.f};
    UPROPERTY(EditAnywhere, Category = "Debug|Cone")
    float cone_angle_height{45.f};
    UPROPERTY(EditAnywhere, Category = "Debug|Cone")
    FColor cone_colour{FColor::Blue};

    // Cone
    UPROPERTY(EditAnywhere, Category = "Debug|Capsule")
    float capsule_radius{500.f};
    UPROPERTY(EditAnywhere, Category = "Debug|Capsule")
    float capsule_half_height{1000.f};
    UPROPERTY(EditAnywhere, Category = "Debug|Capsule")
    FColor capsule_colour{FColor::Blue};
};
