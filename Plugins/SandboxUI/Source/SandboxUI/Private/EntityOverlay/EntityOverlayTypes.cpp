#include "SandboxUI/EntityOverlay/EntityOverlayTypes.h"

void FEntityOverlayCollector::begin(FVector3f const origin,
                                    float const maximum_range,
                                    TArray<ioj::ui::entity_overlay::Instance>& output_instances) {
    check(maximum_range >= 0.0f);

    collector_.begin({origin.X, origin.Y, origin.Z}, maximum_range);
    output_instances_ = &output_instances;
    output_instances_->Reset();
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
        auto const position{source.positions[index]};
        if (auto const instance{collector_.try_add({position.X, position.Y, position.Z},
                                                   source.health_values[index],
                                                   source.world_radii[index],
                                                   0)}) {
            output_instances_->Add(*instance);
        }
    }
    return output_instances_->Num() - previous_count;
}
