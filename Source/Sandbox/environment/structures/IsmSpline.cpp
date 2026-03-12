#include "Sandbox/environment/structures/IsmSpline.h"

#include "Sandbox/logging/SandboxLogCategories.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SplineComponent.h"
#include "Engine/StaticMesh.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

AIsmSpline::AIsmSpline()
    : ismc{CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("ismc"))}
    , spline{CreateDefaultSubobject<USplineComponent>(TEXT("spline"))} {

    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

    ismc->SetupAttachment(RootComponent);
    spline->SetupAttachment(RootComponent);
}

void AIsmSpline::OnConstruction(FTransform const& transform) {
    Super::OnConstruction(transform);
    update_isms(false);
}

void AIsmSpline::update_isms_button() {
    return update_isms(true);
}
void AIsmSpline::update_isms(bool warn_on_no_mesh) {
    RETURN_IF_NULLPTR(ismc);
    RETURN_IF_NULLPTR(spline);
    if (!mesh) {
        if (warn_on_no_mesh) {
            WARN_IS_FALSE(LogSandboxActor, mesh);
        }
        return;
    }
    RETURN_IF_NULLPTR(mesh);
    RETURN_IF_FALSE(update_values(*spline));

    ismc->ClearInstances();
    ismc->SetStaticMesh(mesh);
    ismc->PreAllocateInstancesMemory(n_elems_);

    for (int32 i{0}; i < n_elems_; i++) {
        auto const distance{distance_per_element_ * static_cast<float>(i)};
        auto const transform{
            spline->GetTransformAtDistanceAlongSpline(distance, ESplineCoordinateSpace::Local)};
        ismc->AddInstance(transform);
    }
}
bool AIsmSpline::update_values(this auto& self, USplineComponent& s) {
    auto const length{s.GetSplineLength()};

    switch (self.spawn_mode) {
        case ESpawnMode::spawn_per_distance: {
            RETURN_VALUE_IF_TRUE((self.distance_per_element_user <= 0.f), false);

            self.distance_per_element_ = self.distance_per_element_user;
            self.n_elems_ = FMath::FloorToInt(length / self.distance_per_element_);
            return true;
        }
        case ESpawnMode::spread_over_distance: {
            RETURN_VALUE_IF_TRUE((self.n_elems_user < 2), false);

            self.n_elems_ = self.n_elems_user;
            self.distance_per_element_ = length / static_cast<float>(self.n_elems_ - 1);
            return true;
        }
        default: {
            break;
        }
    }

    UE_LOG(LogSandboxActor, Error, TEXT("Unhandled ESpawnMode value."));
    return false;
}
