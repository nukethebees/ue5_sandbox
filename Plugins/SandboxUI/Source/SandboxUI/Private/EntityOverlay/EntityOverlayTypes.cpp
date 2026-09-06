#include "SandboxUI/EntityOverlay/EntityOverlayTypes.h"

#include "Math/UnrealMathUtility.h"

namespace ml::ui::entity_overlay {
inline constexpr uint32 objective_role_mask{0x3};
inline constexpr uint32 has_fill_color_mask{1 << 2};
inline constexpr uint32 fill_color_red_shift{8};
inline constexpr uint32 fill_color_green_shift{16};
inline constexpr uint32 fill_color_blue_shift{24};

auto pack_unorm8(float const value) -> uint32 {
    return static_cast<uint32>(FMath::RoundToInt(FMath::Clamp(value, 0.0f, 1.0f) * 255.0f));
}

auto pack_display_data(EEntityOverlayObjectiveRole const objective_role) -> uint32 {
    auto const role{static_cast<uint32>(objective_role)};
    check((role & ~objective_role_mask) == 0);
    return role;
}

auto pack_display_data(EEntityOverlayObjectiveRole const objective_role,
                       FLinearColor const fill_color) -> uint32 {
    return pack_display_data(objective_role) | has_fill_color_mask |
           (pack_unorm8(fill_color.R) << fill_color_red_shift) |
           (pack_unorm8(fill_color.G) << fill_color_green_shift) |
           (pack_unorm8(fill_color.B) << fill_color_blue_shift);
}
}

void FEntityOverlayCollector::begin(FVector3f const origin,
                                    float const maximum_range,
                                    TArray<FEntityOverlayInstance>& output_instances) {
    check(maximum_range >= 0.0f);

    origin_ = origin;
    maximum_range_squared_ = maximum_range * maximum_range;
    output_instances_ = &output_instances;
    output_instances_->Reset();
    invalid_health_count_ = 0;
}

auto FEntityOverlayCollector::try_add(FVector3f const position,
                                      float normalized_health,
                                      float world_radius,
                                      EEntityOverlayObjectiveRole const objective_role,
                                      bool const bypass_range) -> bool {
    return try_add_impl(position,
                        normalized_health,
                        world_radius,
                        ml::ui::entity_overlay::pack_display_data(objective_role),
                        bypass_range);
}

auto FEntityOverlayCollector::try_add_colored(FVector3f const position,
                                              float normalized_health,
                                              float world_radius,
                                              FLinearColor const fill_color,
                                              EEntityOverlayObjectiveRole const objective_role,
                                              bool const bypass_range) -> bool {
    return try_add_impl(position,
                        normalized_health,
                        world_radius,
                        ml::ui::entity_overlay::pack_display_data(objective_role, fill_color),
                        bypass_range);
}

auto FEntityOverlayCollector::try_add_impl(FVector3f const position,
                                           float normalized_health,
                                           float world_radius,
                                           uint32 const display_data,
                                           bool const bypass_range) -> bool {
    check(output_instances_);

    if (!bypass_range && FVector3f::DistSquared(origin_, position) > maximum_range_squared_) {
        return false;
    }

    if (!FMath::IsFinite(normalized_health)) {
        normalized_health = 0.0f;
        ++invalid_health_count_;
    }
    if (!FMath::IsFinite(world_radius)) {
        world_radius = 0.0f;
    }

    output_instances_->Add({.world_position = position,
                            .health = FMath::Clamp(normalized_health, 0.0f, 1.0f),
                            .world_radius = FMath::Max(world_radius, 0.0f),
                            .display_data = display_data});
    return true;
}

auto FEntityOverlayCollector::append(FEntityOverlaySourceView const source) -> int32 {
    check(output_instances_);
    if (!source.is_valid()) {
        return 0;
    }

    auto const previous_count{output_instances_->Num()};
    auto const count{source.positions.Num()};
    output_instances_->Reserve(previous_count + count);
    for (int32 index{0}; index < count; ++index) {
        static_cast<void>(try_add(
            source.positions[index], source.health_values[index], source.world_radii[index]));
    }
    return output_instances_->Num() - previous_count;
}
