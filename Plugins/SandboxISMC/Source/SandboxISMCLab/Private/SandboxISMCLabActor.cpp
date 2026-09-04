#include "SandboxISMCLabActor.h"

#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Math/RandomStream.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "SandboxISMCComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SandboxISMCLabActor)

DEFINE_LOG_CATEGORY_STATIC(LogSandboxISMCLab, Log, All);

ASandboxISMCLabActor::ASandboxISMCLabActor() {
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;

    instances_ = CreateDefaultSubobject<USandboxISMCComponent>(TEXT("Instances"));
    SetRootComponent(instances_);
}

void ASandboxISMCLabActor::OnConstruction(FTransform const& transform) {
    Super::OnConstruction(transform);

    if (regenerate_on_construction_) {
        regenerate_instances();
    }
}

void ASandboxISMCLabActor::BeginPlay() {
    Super::BeginPlay();

    if (regenerate_on_begin_play_) {
        regenerate_instances();
    }
}

void ASandboxISMCLabActor::Tick(float delta_seconds) {
    TRACE_CPUPROFILER_EVENT_SCOPE(ASandboxISMCLabActor::Tick);
    Super::Tick(delta_seconds);

    auto const instance_count{instance_data_.num()};
    if (animate_ && instance_count > 0 && animated_instance_count_ > 0) {
        auto const update_count{FMath::Min(animated_instance_count_, instance_count)};
        auto const delta_rotation{FQuat4f{
            FVector3f::UpVector, FMath::DegreesToRadians(rotation_speed_degrees_ * delta_seconds)}};

        for (auto instance_index = 0; instance_index < update_count; ++instance_index) {
            instance_data_.rotations[instance_index] =
                (delta_rotation * instance_data_.rotations[instance_index]).GetNormalized();
        }

        submit_instances();
    }

    auto const* world = GetWorld();
    if (!log_metrics_ || world == nullptr || world->GetTimeSeconds() < next_log_time_seconds_) {
        return;
    }

    next_log_time_seconds_ = world->GetTimeSeconds() + log_interval_seconds_;
    auto const metrics = instances_->get_update_metrics();
    UE_LOG(LogSandboxISMCLab,
           Display,
           TEXT("instances=%d build=%.3f ms submit=%.3f ms upload=%.3f ms bytes=%llu"),
           metrics.instance_count,
           metrics.build_ms,
           metrics.submit_ms,
           metrics.upload_ms,
           metrics.upload_bytes);
}

void ASandboxISMCLabActor::regenerate_instances() {
    TRACE_CPUPROFILER_EVENT_SCOPE(ASandboxISMCLabActor::regenerate_instances);
    if (static_mesh_ == nullptr) {
        UE_LOG(LogSandboxISMCLab,
               Warning,
               TEXT("Select a static mesh before generating SandboxISMC instances"));
        clear_instances();
        return;
    }

    instances_->set_static_mesh(*static_mesh_);
    instance_data_.reset();

    if (distribution_ == ESandboxISMCLabDistribution::Grid) {
        auto const dimensions = FIntVector{
            FMath::Max(grid_dimensions_.X, 1),
            FMath::Max(grid_dimensions_.Y, 1),
            FMath::Max(grid_dimensions_.Z, 1),
        };
        auto const instance_count = dimensions.X * dimensions.Y * dimensions.Z;
        instance_data_.add_uninitialised(instance_count);

        auto const grid_extent = FVector3f{
            static_cast<float>(dimensions.X - 1) * static_cast<float>(grid_spacing_.X),
            static_cast<float>(dimensions.Y - 1) * static_cast<float>(grid_spacing_.Y),
            static_cast<float>(dimensions.Z - 1) * static_cast<float>(grid_spacing_.Z),
        };
        auto const origin = grid_extent * -0.5f;

        for (auto z = 0; z < dimensions.Z; ++z) {
            for (auto y = 0; y < dimensions.Y; ++y) {
                for (auto x = 0; x < dimensions.X; ++x) {
                    auto const position =
                        origin + FVector3f{
                                     static_cast<float>(x) * static_cast<float>(grid_spacing_.X),
                                     static_cast<float>(y) * static_cast<float>(grid_spacing_.Y),
                                     static_cast<float>(z) * static_cast<float>(grid_spacing_.Z),
                                 };
                    auto const instance_index{z * dimensions.X * dimensions.Y + y * dimensions.X +
                                              x};
                    instance_data_.positions[instance_index] = position;
                    instance_data_.rotations[instance_index] = FQuat4f::Identity;
                    instance_data_.scales[instance_index] = FVector3f::OneVector;
                }
            }
        }
    } else {
        auto const instance_count = FMath::Max(cloud_instance_count_, 1);
        instance_data_.add_uninitialised(instance_count);
        FRandomStream random{random_seed_};
        auto const extent = FVector3f{cloud_extent_};

        for (auto instance_index = 0; instance_index < instance_count; ++instance_index) {
            auto const position = FVector3f{
                static_cast<float>(random.FRandRange(-extent.X, extent.X)),
                static_cast<float>(random.FRandRange(-extent.Y, extent.Y)),
                static_cast<float>(random.FRandRange(-extent.Z, extent.Z)),
            };
            auto const rotation = FQuat4f{
                FVector3f::UpVector,
                static_cast<float>(random.FRandRange(0.0f, 2.0f * UE_PI)),
            };
            instance_data_.positions[instance_index] = position;
            instance_data_.rotations[instance_index] = rotation;
            instance_data_.scales[instance_index] = FVector3f::OneVector;
        }
    }

    submit_instances();
    auto const metrics = instances_->get_update_metrics();
    UE_LOG(LogSandboxISMCLab,
           Display,
           TEXT("Generated %d SandboxISMC instances; packed upload is %.2f MiB"),
           metrics.instance_count,
           static_cast<double>(metrics.upload_bytes) / (1024.0 * 1024.0));
}

void ASandboxISMCLabActor::clear_instances() {
    instance_data_.reset();
    instances_->clear_instances();
}

void ASandboxISMCLabActor::submit_instances() {
    TRACE_CPUPROFILER_EVENT_SCOPE(ASandboxISMCLabActor::submit_instances);
    auto const source{instance_data_.get_const_view()};
    instances_->set_instances(
        source.num(), ESandboxISMCParallelism::Auto, [&](FSandboxISMCInstanceChunkWriter& chunk) {
            auto const chunk_count{chunk.num()};
            auto const first_index{chunk.first_index()};
            for (auto local_index = 0; local_index < chunk_count; ++local_index) {
                auto const instance_index{first_index + local_index};
                chunk.set_transform(local_index,
                                    source.positions[instance_index],
                                    source.rotations[instance_index],
                                    source.scales[instance_index]);
            }
        });
}
