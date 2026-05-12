#pragma once

#include "CoreMinimal.h"
#include "Misc/Optional.h"

#include "DrawDebugConfig.generated.h"

class UWorld;

USTRUCT()
struct FDrawDebugConfig {
    GENERATED_BODY()

    FDrawDebugConfig() = default;

    void draw_line(UWorld const* world, FVector const& start, FVector const& end) const;
    void draw_circle_arc(UWorld const* world,
                         FVector const& start,
                         float const arc_radius,
                         float const half_angle_rad) const;

    auto get_line_colour() const -> FColor;
    auto get_line_thickness() const -> float;

    auto get_circle_arc_colour() const -> FColor;
    auto get_circle_arc_thickness() const -> float;

    // General
    UPROPERTY(EditAnywhere, Category = "Debug")
    float spacing{1000.f};
    UPROPERTY(EditAnywhere, Category = "Debug")
    FColor colour{FColor::Green};
    UPROPERTY(EditAnywhere, Category = "Debug")
    float thickness{8.f};
    UPROPERTY(EditAnywhere, Category = "Debug")
    bool persistent{false};
    UPROPERTY(EditAnywhere, Category = "Debug")
    unsigned depth_priority{0u};
    UPROPERTY(EditAnywhere, Category = "Debug")
    float lifetime{0.f};

    // General shape
    UPROPERTY(EditAnywhere, Category = "Debug")
    float radius{500.f};
    UPROPERTY(EditAnywhere, Category = "Debug")
    int32 segments{16};
    UPROPERTY(EditAnywhere, Category = "Debug")
    float length{1000.f};

    // Lines
    UPROPERTY(EditAnywhere, Category = "Debug")
    TOptional<float> line_thickness{};
    UPROPERTY(EditAnywhere, Category = "Debug")
    TOptional<float> line_length{};

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
