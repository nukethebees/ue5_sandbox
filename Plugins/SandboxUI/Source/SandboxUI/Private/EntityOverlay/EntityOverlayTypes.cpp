#include "SandboxUI/EntityOverlay/EntityOverlayTypes.h"

namespace {
auto native_role(EEntityOverlayObjectiveRole const role) -> ml::ui::entity_overlay::ObjectiveRole {
    return static_cast<ml::ui::entity_overlay::ObjectiveRole>(role);
}
}

void FEntityOverlayCollector::begin(FVector3f const origin,
                                    float const maximum_range,
                                    TArray<FEntityOverlayInstance>& output_instances) {
    check(maximum_range >= 0.0f);

    collector_.begin({origin.X, origin.Y, origin.Z}, maximum_range);
    output_instances_ = &output_instances;
    output_instances_->Reset();
}

auto FEntityOverlayCollector::try_add(FVector3f const position,
                                      float normalized_health,
                                      float world_radius,
                                      EEntityOverlayObjectiveRole const objective_role,
                                      bool const bypass_range) -> bool {
    auto display_data{ml::ui::entity_overlay::pack_display_data(native_role(objective_role))};
    return try_add_impl(position, normalized_health, world_radius, display_data, bypass_range);
}

auto FEntityOverlayCollector::try_add_colored(FVector3f const position,
                                              float normalized_health,
                                              float world_radius,
                                              FLinearColor const fill_color,
                                              EEntityOverlayObjectiveRole const objective_role,
                                              bool const bypass_range) -> bool {
    return try_add_impl(
        position,
        normalized_health,
        world_radius,
        ml::ui::entity_overlay::pack_display_data(
            native_role(objective_role), {fill_color.R, fill_color.G, fill_color.B, fill_color.A}),
        bypass_range);
}

auto FEntityOverlayCollector::try_add_impl(FVector3f const position,
                                           float normalized_health,
                                           float world_radius,
                                           uint32 const display_data,
                                           bool const bypass_range) -> bool {
    check(output_instances_);

    auto const addition{collector_.try_add({position.X, position.Y, position.Z},
                                           normalized_health,
                                           world_radius,
                                           display_data,
                                           bypass_range)};
    if (!addition) {
        return false;
    }
    auto const& instance{addition->instance};
    auto const added_index{output_instances_->Add({.world_position = {instance.world_position.x,
                                                                      instance.world_position.y,
                                                                      instance.world_position.z},
                                                   .health = instance.health,
                                                   .world_radius = instance.world_radius,
                                                   .display_data = instance.display_data})};
    if (addition->swap_index >= 0) {
        output_instances_->Swap(addition->swap_index, added_index);
    }
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
