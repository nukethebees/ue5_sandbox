#pragma once

#include "GpuSparkBurst.h"
#include "SpaceGameRendering/SparkParticleRecord.h"

#include "RenderGraphResources.h"
#include "RenderResource.h"

struct FSparkUploadBuffer;

class FSparkParticleBuffer final : public FRenderResource {
  public:
    ~FSparkParticleBuffer() override;

    void set_initial_data(TConstArrayView<FSparkParticleRecord> initial_data);

    void InitRHI(FRHICommandListBase& rhi_command_list) override;
    void ReleaseRHI() override;

    void upload(FRHICommandListBase& rhi_command_list, FSparkUploadBuffer const& upload_buffer);
    void expand(FRHICommandListImmediate& rhi_command_list,
                TArray<FGpuSparkBurst>&& bursts,
                TArray<uint32>&& selection_indices);
    void clear(FRHICommandListImmediate& rhi_command_list);

    auto srv() const -> FShaderResourceViewRHIRef;
  private:
    TArray<FSparkParticleRecord> initial_data_;
    TRefCountPtr<FRDGPooledBuffer> pooled_buffer_;
};
