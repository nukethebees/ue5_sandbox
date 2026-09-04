#pragma once

#include "SandboxISMCInstanceChunkWriter.h"
#include "SandboxISMCParallelism.h"
#include "SandboxISMCUpdateMetrics.h"

#include "Async/ParallelFor.h"
#include "Components/MeshComponent.h"
#include "HAL/PlatformTime.h"
#include "Math/BoxSphereBounds.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "Stats/Stats.h"
#include "Templates/SharedPointer.h"

#include "SandboxISMCComponent.generated.h"

class UStaticMesh;
class UMaterialInterface;
struct FSandboxISMCMetricsState;
struct FSandboxISMCStagingBuffer;
struct FSandboxISMCStagingState;

DECLARE_STATS_GROUP(TEXT("SandboxISMC"), STATGROUP_SandboxISMC, STATCAT_Advanced);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Build instance snapshot"),
                          STAT_SandboxISMCBuild,
                          STATGROUP_SandboxISMC,
                          SANDBOXISMC_API);

UCLASS(ClassGroup = (Rendering), meta = (DisplayName = "Sandbox ISMC"))
class SANDBOXISMC_API USandboxISMCComponent final : public UMeshComponent {
    GENERATED_BODY()
  public:
    USandboxISMCComponent();

    auto set_static_mesh(UStaticMesh& mesh) -> void;
    auto clear_static_mesh() -> void;
    auto get_static_mesh() const -> UStaticMesh*;

    template <typename FillChunk>
    auto set_instances(int32 instance_count,
                       ESandboxISMCParallelism parallelism,
                       FillChunk&& fill_chunk) -> void {
        set_instances_impl<true>(
            instance_count, parallelism, FBox3f{ForceInit}, Forward<FillChunk>(fill_chunk));
    }

    template <typename FillChunk>
    auto set_instances(int32 instance_count,
                       FBox3f local_bounds,
                       ESandboxISMCParallelism parallelism,
                       FillChunk&& fill_chunk) -> void {
        checkf(instance_count <= 0 || local_bounds.IsValid != 0,
               TEXT("SandboxISMC bounds must be valid when instances are submitted"));
        set_instances_impl<false>(
            instance_count, parallelism, local_bounds, Forward<FillChunk>(fill_chunk));
    }

    auto clear_instances() -> void;
    auto get_instance_count() const -> int32;
    auto get_update_metrics() const -> FSandboxISMCUpdateMetrics;

    virtual auto CreateSceneProxy() -> FPrimitiveSceneProxy* override;
    virtual auto GetNumMaterials() const -> int32 override;
    virtual auto GetMaterial(int32 element_index) const -> UMaterialInterface* override;
    virtual auto CalcBounds(FTransform const& local_to_world) const -> FBoxSphereBounds override;
    virtual auto SendRenderDynamicData_Concurrent() -> void override;
  private:
    static constexpr int32 instance_chunk_size{1024};
    static constexpr int32 parallel_instance_threshold{4096};

    template <bool CalculateBounds, typename FillChunk>
    auto set_instances_impl(int32 instance_count,
                            ESandboxISMCParallelism parallelism,
                            FBox3f local_bounds,
                            FillChunk&& fill_chunk) -> void {
        checkf(instance_count >= 0, TEXT("SandboxISMC instance count must not be negative"));
        TRACE_CPUPROFILER_EVENT_SCOPE(USandboxISMCComponent::set_instances);
        SCOPE_CYCLE_COUNTER(STAT_SandboxISMCBuild);
        auto const start_cycles{FPlatformTime::Cycles64()};

        auto instances{begin_instance_update(instance_count)};
        auto const chunk_count{FMath::DivideAndRoundUp(instance_count, instance_chunk_size)};
        if constexpr (CalculateBounds) {
            chunk_bounds_.SetNumUninitialized(chunk_count);
        } else {
            chunk_bounds_.Reset();
        }

        auto const build_chunk{[&](int32 chunk_index) {
            auto const first_index{chunk_index * instance_chunk_size};
            auto const count{FMath::Min(instance_chunk_size, instance_count - first_index)};
            FSandboxISMCInstanceChunkWriter writer{instances.Slice(first_index, count),
                                                   first_index,
                                                   mesh_bounds_origin_,
                                                   mesh_bounds_radius_,
                                                   CalculateBounds && has_mesh_bounds_};
            fill_chunk(writer);
            if constexpr (CalculateBounds) {
                chunk_bounds_[chunk_index] = writer.bounds();
            }
        }};

        auto const run_parallel{parallelism == ESandboxISMCParallelism::Parallel ||
                                (parallelism == ESandboxISMCParallelism::Auto &&
                                 instance_count >= parallel_instance_threshold)};
        if (run_parallel) {
            ParallelFor(chunk_count, build_chunk);
        } else {
            for (auto chunk_index = 0; chunk_index < chunk_count; ++chunk_index) {
                build_chunk(chunk_index);
            }
        }

        if constexpr (CalculateBounds) {
            for (auto const& bounds : chunk_bounds_) {
                local_bounds += bounds;
            }
        } else if (instance_count == 0) {
            local_bounds = FBox3f{ForceInit};
        }

        finish_instance_update(
            instance_count, local_bounds, FPlatformTime::Cycles64() - start_cycles);
    }

    auto begin_instance_update(int32 instance_count) -> TArrayView<FSandboxISMCRenderInstance>;
    auto finish_instance_update(int32 instance_count, FBox3f local_box, uint64 elapsed_cycles)
        -> void;

    UPROPERTY(EditAnywhere, Category = "Mesh")
    TObjectPtr<UStaticMesh> static_mesh_;

    TSharedPtr<FSandboxISMCStagingState, ESPMode::ThreadSafe> staging_state_;
    TSharedPtr<FSandboxISMCMetricsState, ESPMode::ThreadSafe> metrics_;
    FSandboxISMCStagingBuffer* pending_staging_buffer_{nullptr};
    TArray<FBox3f> chunk_bounds_;
    FBoxSphereBounds local_bounds_{ForceInit};
    FVector3f mesh_bounds_origin_{FVector3f::ZeroVector};
    float mesh_bounds_radius_{0.0f};
    int32 instance_count_{0};
    bool has_mesh_bounds_{false};
};
