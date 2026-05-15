#pragma once

#include <Math/Color.h>
#include "CoreMinimal.h"
#include "Misc/Optional.h"

#include "DrawDebugConfig.generated.h"

class UWorld;

USTRUCT()
struct FDrawDebugConfig {
    GENERATED_BODY()

    FDrawDebugConfig() = default;

    auto get_colour() const -> FColor;
    auto get_colour(TOptional<FColor> c) const -> FColor;
    auto get_thickness() const -> float;
    auto get_thickness(TOptional<float> t) const -> float;
    auto get_segments() const -> int32;
    auto get_segments(TOptional<int32> t) const -> int32;

    void draw_line(FVector const& start, FVector const& end) const;

    void draw_point(FVector const& location) const;

    void draw_circle(FTransform const& transform, float const circle_radius) const;
    auto get_circle_xy_rotation() const -> FRotator;

    void draw_circle_arc(FVector const& start,
                         float const arc_radius,
                         float const half_angle_rad,
                         FQuat const& rotation) const;

    void draw_sphere(FVector const& start, float const radius) const;
    void draw_cone(FVector const& start, FVector const& direction, float const length) const;

    // General
    UPROPERTY(EditAnywhere, Category = "Debug")
    TWeakObjectPtr<UWorld> world{nullptr};

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
    UPROPERTY(EditAnywhere, Category = "Debug|Line")
    TOptional<float> line_thickness{};
    UPROPERTY(EditAnywhere, Category = "Debug|Line")
    TOptional<FColor> line_colour{};

    // Point
    UPROPERTY(EditAnywhere, Category = "Debug|Point")
    float point_size{10.f};
    UPROPERTY(EditAnywhere, Category = "Debug|Point")
    TOptional<FColor> point_colour{};

    // Arrow
    UPROPERTY(EditAnywhere, Category = "Debug|Arrow")
    float arrow_size{10.f};
    UPROPERTY(EditAnywhere, Category = "Debug|Arrow")
    TOptional<FColor> arrow_colour{};

    // Box
    UPROPERTY(EditAnywhere, Category = "Debug|Box")
    TOptional<FColor> box_colour{};

    // Coordinate system
    UPROPERTY(EditAnywhere, Category = "Debug|Coord")
    float coord_scale{10.f};

    // Circle
    UPROPERTY(EditAnywhere, Category = "Debug|Circle")
    bool circle_draw_axis{true};
    UPROPERTY(EditAnywhere, Category = "Debug|Circle")
    TOptional<FColor> circle_colour{};
    UPROPERTY(EditAnywhere, Category = "Debug|Circle")
    TOptional<float> circle_thickness{};
    UPROPERTY(EditAnywhere, Category = "Debug|Circle")
    TOptional<int32> circle_segments{};

    // CircleArc
    UPROPERTY(EditAnywhere, Category = "Debug|CircleArc")
    TOptional<FColor> circle_arc_colour{FColor::Purple};
    UPROPERTY(EditAnywhere, Category = "Debug|CircleArc")
    TOptional<float> circle_arc_thickness{};
    UPROPERTY(EditAnywhere, Category = "Debug|CircleArc")
    TOptional<int32> circle_arc_segments{};

    // Donut
    UPROPERTY(EditAnywhere, Category = "Debug|Donut")
    float donut_inner_radius{100.f};
    UPROPERTY(EditAnywhere, Category = "Debug|Donut")
    float donut_outer_radius{200.f};
    UPROPERTY(EditAnywhere, Category = "Debug|Donut")
    int32 donut_segments{16};
    UPROPERTY(EditAnywhere, Category = "Debug|Donut")
    TOptional<FColor> donut_colour{};

    // Sphere
    UPROPERTY(EditAnywhere, Category = "Debug|Sphere")
    TOptional<int32> sphere_segments;
    UPROPERTY(EditAnywhere, Category = "Debug|Sphere")
    TOptional<FColor> sphere_colour{};

    // Cone
    UPROPERTY(EditAnywhere, Category = "Debug|Cone")
    float cone_angle_half_width_deg{45.f};
    UPROPERTY(EditAnywhere, Category = "Debug|Cone")
    float cone_angle_half_height_deg{45.f};
    UPROPERTY(EditAnywhere, Category = "Debug|Cone")
    TOptional<int32> cone_segments;
    UPROPERTY(EditAnywhere, Category = "Debug|Cone")
    TOptional<FColor> cone_colour{};
    UPROPERTY(EditAnywhere, Category = "Debug|Cone")
    TOptional<float> cone_thickness;

    // Cone
    UPROPERTY(EditAnywhere, Category = "Debug|Capsule")
    float capsule_radius{500.f};
    UPROPERTY(EditAnywhere, Category = "Debug|Capsule")
    float capsule_half_height{1000.f};
    UPROPERTY(EditAnywhere, Category = "Debug|Capsule")
    TOptional<FColor> capsule_colour{};
  protected:
    auto check_world_valid() const -> bool;
  private:
    template <typename T>
    auto get_optional(TOptional<T> value, T const& default_value) const -> T;
};
