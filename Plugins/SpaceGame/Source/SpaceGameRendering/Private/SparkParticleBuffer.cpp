#include "SparkParticleBuffer.h"

#include "SparkUploadBuffer.h"

#include "GlobalShader.h"
#include "ProfilingDebugging/CountersTrace.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "ShaderParameterStruct.h"
#include "Templates/RefCounting.h"

TRACE_DECLARE_INT_COUNTER(SandboxSparkUploadBytes, TEXT("Sandbox/Sparks/UploadBytes"));
TRACE_DECLARE_FLOAT_COUNTER(SandboxSparkRenderThreadUploadMs,
                            TEXT("Sandbox/Sparks/RenderThreadUploadMs"));
TRACE_DECLARE_INT_COUNTER(SandboxSparkBurstUploadBytes, TEXT("Sandbox/Sparks/BurstUploadBytes"));

namespace SpaceGame::Sparks::ParticleBufferPrivate {
inline constexpr uint32 maximum_dispatch_groups_x{65535};

class FSparkExpansionCS final : public FGlobalShader {
  public:
    DECLARE_GLOBAL_SHADER(FSparkExpansionCS);
    SHADER_USE_PARAMETER_STRUCT(FSparkExpansionCS, FGlobalShader);

    BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
    SHADER_PARAMETER(uint32, SparkBurstCount)
    SHADER_PARAMETER(uint32, SparkBurstGroupsX)
    SHADER_PARAMETER(uint32, SparkParticleCapacity)
    SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FGpuSparkBurst>, SparkBursts)
    SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, SparkSelectionIndices)
    SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FSparkParticleRecord>, SparkParticles)
    END_SHADER_PARAMETER_STRUCT()

    static auto ShouldCompilePermutation(FGlobalShaderPermutationParameters const& parameters)
        -> bool {
        return IsFeatureLevelSupported(parameters.Platform, ERHIFeatureLevel::SM5);
    }
};

IMPLEMENT_GLOBAL_SHADER(FSparkExpansionCS,
                        "/Plugin/SpaceGame/Private/Sparks/SparkExpansion.usf",
                        "expand_spark_bursts_cs",
                        SF_Compute);
} // namespace SpaceGame::Sparks::ParticleBufferPrivate

FSparkParticleBuffer::~FSparkParticleBuffer() = default;

void FSparkParticleBuffer::set_initial_data(
    TConstArrayView<FSparkParticleRecord> const initial_data) {
    check(IsInGameThread());
    initial_data_.Reset(initial_data.Num());
    initial_data_.Append(initial_data.GetData(), initial_data.Num());
}

void FSparkParticleBuffer::InitRHI(FRHICommandListBase& rhi_command_list) {
    if (initial_data_.IsEmpty()) {
        return;
    }

    FResourceArrayUploadArrayView upload_view{TConstArrayView<FSparkParticleRecord>{initial_data_}};
    auto const buffer_size{initial_data_.Num() * sizeof(FSparkParticleRecord)};
    auto const create_description{
        FRHIBufferCreateDesc::CreateStructured(
            TEXT("Sparks.ParticleData"), buffer_size, sizeof(FSparkParticleRecord))
            .AddUsage(EBufferUsageFlags::ShaderResource | EBufferUsageFlags::UnorderedAccess |
                      EBufferUsageFlags::Static)
            .SetInitialState(ERHIAccess::SRVMask)
            .SetInitActionResourceArray(&upload_view)};
    TRefCountPtr<FRHIBuffer> buffer{rhi_command_list.CreateBuffer(create_description)};
    auto const rdg_description{
        FRDGBufferDesc::CreateStructuredDesc(sizeof(FSparkParticleRecord), initial_data_.Num())};
    pooled_buffer_ = new FRDGPooledBuffer{rhi_command_list,
                                          MoveTemp(buffer),
                                          rdg_description,
                                          static_cast<uint32>(initial_data_.Num()),
                                          TEXT("Sparks.ParticleData")};
    initial_data_.Reset();
}

void FSparkParticleBuffer::ReleaseRHI() {
    pooled_buffer_.SafeRelease();
}

void FSparkParticleBuffer::upload(FRHICommandListBase& rhi_command_list,
                                  FSparkUploadBuffer const& upload_buffer) {
    if (!pooled_buffer_.IsValid()) {
        return;
    }

    auto const start_cycles{FPlatformTime::Cycles64()};
    int64 upload_bytes{0};
    check(upload_buffer.destinations.Num() == upload_buffer.counts.Num());
    int32 source_index{0};
    auto const range_count{upload_buffer.counts.Num()};
    for (int32 range_index{0}; range_index < range_count; ++range_index) {
        auto const particle_count{upload_buffer.counts[range_index]};
        auto const byte_count{particle_count * sizeof(FSparkParticleRecord)};
        if (byte_count <= 0) {
            continue;
        }
        check(source_index + particle_count <= upload_buffer.particles.Num());
        auto const byte_offset{upload_buffer.destinations[range_index] *
                               sizeof(FSparkParticleRecord)};
        auto* const destination{rhi_command_list.LockBuffer(
            pooled_buffer_->GetRHI(), byte_offset, byte_count, RLM_WriteOnly)};
        FMemory::Memcpy(destination, upload_buffer.particles.GetData() + source_index, byte_count);
        rhi_command_list.UnlockBuffer(pooled_buffer_->GetRHI());
        source_index += particle_count;
        upload_bytes += byte_count;
    }
    check(source_index == upload_buffer.particles.Num());
    TRACE_COUNTER_SET_ALWAYS(SandboxSparkUploadBytes, upload_bytes);
    TRACE_COUNTER_SET_ALWAYS(
        SandboxSparkRenderThreadUploadMs,
        FPlatformTime::ToMilliseconds64(FPlatformTime::Cycles64() - start_cycles));
}

void FSparkParticleBuffer::expand(FRHICommandListImmediate& rhi_command_list,
                                  TArray<FGpuSparkBurst>&& bursts,
                                  TArray<uint32>&& selection_indices) {
    if (!pooled_buffer_.IsValid() || bursts.IsEmpty()) {
        return;
    }

    auto const burst_count{static_cast<uint32>(bursts.Num())};
    auto const particle_capacity{pooled_buffer_->Desc.NumElements};
    auto const upload_bytes{bursts.Num() * sizeof(FGpuSparkBurst) +
                            selection_indices.Num() * sizeof(uint32)};
    if (selection_indices.IsEmpty()) {
        selection_indices.Add(0);
    }

    FRDGBuilder graph_builder{rhi_command_list};
    auto const particle_buffer{graph_builder.RegisterExternalBuffer(pooled_buffer_)};
    auto const burst_buffer{
        CreateStructuredBuffer(graph_builder, TEXT("Sparks.Bursts"), MoveTemp(bursts))};
    auto const selection_buffer{CreateStructuredBuffer(
        graph_builder, TEXT("Sparks.SelectionIndices"), MoveTemp(selection_indices))};
    auto* const parameters{graph_builder.AllocParameters<
        SpaceGame::Sparks::ParticleBufferPrivate::FSparkExpansionCS::FParameters>()};
    auto const groups_x{FMath::Min(
        burst_count, SpaceGame::Sparks::ParticleBufferPrivate::maximum_dispatch_groups_x)};
    auto const groups_y{FMath::DivideAndRoundUp(burst_count, groups_x)};
    parameters->SparkBurstCount = burst_count;
    parameters->SparkBurstGroupsX = groups_x;
    parameters->SparkParticleCapacity = particle_capacity;
    parameters->SparkBursts = graph_builder.CreateSRV(burst_buffer);
    parameters->SparkSelectionIndices = graph_builder.CreateSRV(selection_buffer);
    parameters->SparkParticles = graph_builder.CreateUAV(particle_buffer);

    auto const shader{TShaderMapRef<SpaceGame::Sparks::ParticleBufferPrivate::FSparkExpansionCS>{
        GetGlobalShaderMap(GMaxRHIFeatureLevel)}};
    FComputeShaderUtils::AddPass(
        graph_builder,
        RDG_EVENT_NAME("Sparks.Expand %u bursts", burst_count),
        shader,
        parameters,
        FIntVector{static_cast<int32>(groups_x), static_cast<int32>(groups_y), 1});
    graph_builder.SetBufferAccessFinal(particle_buffer, ERHIAccess::SRVMask);
    graph_builder.Execute();
    TRACE_COUNTER_SET_ALWAYS(SandboxSparkBurstUploadBytes, upload_bytes);
}

void FSparkParticleBuffer::clear(FRHICommandListImmediate& rhi_command_list) {
    if (!pooled_buffer_.IsValid()) {
        return;
    }

    FRDGBuilder graph_builder{rhi_command_list};
    auto const particle_buffer{graph_builder.RegisterExternalBuffer(pooled_buffer_)};
    AddClearUAVPass(graph_builder, graph_builder.CreateUAV(particle_buffer), 0);
    graph_builder.SetBufferAccessFinal(particle_buffer, ERHIAccess::SRVMask);
    graph_builder.Execute();
}

auto FSparkParticleBuffer::srv() const -> FShaderResourceViewRHIRef {
    return pooled_buffer_.IsValid() ? pooled_buffer_->GetSRV() : nullptr;
}
