#include "SandboxISMCLabActor.h"

#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Math/RandomStream.h"
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
    Super::Tick(delta_seconds);

    auto const instance_count = instances_->get_instance_count();
    if (animate_ && instance_count > 0 && animated_instance_count_ > 0) {
        auto const update_count = FMath::Min(animated_instance_count_, instance_count);
        auto instance_data = instances_->edit_instances(0, update_count);
        auto const delta_rotation = FQuat4f{
            FVector3f::UpVector, FMath::DegreesToRadians(rotation_speed_degrees_ * delta_seconds)};

        for (auto instance_index = 0; instance_index < update_count; ++instance_index) {
            instance_data.rotations[instance_index] =
                (delta_rotation * instance_data.rotations[instance_index]).GetNormalized();
        }

        instances_->commit_instance_updates();
    }

    auto const* world = GetWorld();
    if (!log_metrics_ || world == nullptr || world->GetTimeSeconds() < next_log_time_seconds_) {
        return;
    }

    next_log_time_seconds_ = world->GetTimeSeconds() + log_interval_seconds_;
    auto const metrics = instances_->get_update_metrics();
    UE_LOG(LogSandboxISMCLab,
           Display,
           TEXT("instances=%d dirty=%d ranges=%d prepare=%.3f ms submit=%.3f ms upload=%.3f ms "
                "bytes=%llu"),
           metrics.instance_count,
           metrics.dirty_instance_count,
           metrics.dirty_range_count,
           metrics.prepare_ms,
           metrics.submit_ms,
           metrics.upload_ms,
           metrics.upload_bytes);
}

void ASandboxISMCLabActor::regenerate_instances() {
    if (static_mesh_ == nullptr) {
        UE_LOG(LogSandboxISMCLab,
               Warning,
               TEXT("Select a static mesh before generating SandboxISMC instances"));
        clear_instances();
        return;
    }

    instances_->set_static_mesh(*static_mesh_);
    instances_->clear_instances();

    if (distribution_ == ESandboxISMCLabDistribution::Grid) {
        auto const dimensions = FIntVector{
            FMath::Max(grid_dimensions_.X, 1),
            FMath::Max(grid_dimensions_.Y, 1),
            FMath::Max(grid_dimensions_.Z, 1),
        };
        auto const instance_count = dimensions.X * dimensions.Y * dimensions.Z;
        instances_->reserve_instances(instance_count);
        TArray<FVector3f> positions;
        positions.Reserve(instance_count);

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
                    positions.Add(position);
                }
            }
        }
        instances_->add_instances(positions);
    } else {
        auto const instance_count = FMath::Max(cloud_instance_count_, 1);
        instances_->reserve_instances(instance_count);
        TArray<FVector3f> positions;
        TArray<FQuat4f> rotations;
        positions.Reserve(instance_count);
        rotations.Reserve(instance_count);
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
            positions.Add(position);
            rotations.Add(rotation);
        }
        instances_->add_instances(positions, rotations);
    }

    instances_->commit_instance_updates();
    auto const metrics = instances_->get_update_metrics();
    UE_LOG(LogSandboxISMCLab,
           Display,
           TEXT("Generated %d SandboxISMC instances; packed upload is %.2f MiB"),
           metrics.instance_count,
           static_cast<double>(metrics.upload_bytes) / (1024.0 * 1024.0));
}

void ASandboxISMCLabActor::clear_instances() {
    instances_->clear_instances();
    instances_->commit_instance_updates();
}
