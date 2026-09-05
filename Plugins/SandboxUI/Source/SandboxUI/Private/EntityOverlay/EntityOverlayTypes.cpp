#include "SandboxUI/EntityOverlay/EntityOverlayTypes.h"

#include "Math/UnrealMathUtility.h"

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

auto FEntityOverlayCollector::try_add(FVector3f const position, float normalized_health) -> bool {
    check(output_instances_);

    if (FVector3f::DistSquared(origin_, position) > maximum_range_squared_) {
        return false;
    }

    if (!FMath::IsFinite(normalized_health)) {
        normalized_health = 0.0f;
        ++invalid_health_count_;
    }

    output_instances_->Add(
        {.world_position = position, .health = FMath::Clamp(normalized_health, 0.0f, 1.0f)});
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
        static_cast<void>(try_add(source.positions[index], source.health_values[index]));
    }
    return output_instances_->Num() - previous_count;
}
