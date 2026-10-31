#pragma once

#include <ioj/presentation/sparks.h>

#include "RenderGraphResources.h"
#include "RenderResource.h"

struct FSparkUploadBuffer;

class FSparkParticleBuffer final : public FRenderResource {
  public:
    ~FSparkParticleBuffer() override;

    void set_initial_data(std::span<ioj::presentation::SparkParticleRecord const> initial_data);

    void InitRHI(FRHICommandListBase& rhi_command_list) override;
    void ReleaseRHI() override;

    void upload(FRHICommandListBase& rhi_command_list, FSparkUploadBuffer const& upload_buffer);
    void expand(FRHICommandListImmediate& rhi_command_list,
                std::vector<ioj::presentation::GpuSparkBurst>&& bursts,
                std::vector<uint32>&& selection_indices);
    void clear(FRHICommandListImmediate& rhi_command_list);

    auto srv() const -> FShaderResourceViewRHIRef;
  private:
    TArray<ioj::presentation::SparkParticleRecord> initial_data_;
    TRefCountPtr<FRDGPooledBuffer> pooled_buffer_;
};
