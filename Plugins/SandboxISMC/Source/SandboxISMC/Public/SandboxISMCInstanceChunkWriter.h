#pragma once

#include "SandboxISMCInstanceRange.h"
#include "SandboxISMCRenderInstance.h"

#include "Containers/ArrayView.h"
#include "Math/Box.h"
#include "Math/Quat.h"
#include "Math/Transform.h"
#include "Math/Vector.h"

class SANDBOXISMC_API FSandboxISMCInstanceChunkWriter final {
  public:
    FSandboxISMCInstanceChunkWriter(TArrayView<FSandboxISMCRenderInstance> instances,
                                    int32 first_index,
                                    FVector3f mesh_bounds_origin,
                                    float mesh_bounds_radius,
                                    bool has_mesh_bounds)
        : instances_{instances}
        , first_index_{first_index}
        , mesh_bounds_origin_{mesh_bounds_origin}
        , mesh_bounds_radius_{mesh_bounds_radius}
        , has_mesh_bounds_{has_mesh_bounds} {}

    auto first_index() const -> int32 { return first_index_; }
    auto num() const -> int32 { return instances_.Num(); }
    auto range() const -> FSandboxISMCInstanceRange { return {first_index_, instances_.Num()}; }

    auto set_transform(int32 local_index, FVector3f position, FQuat4f rotation, FVector3f scale)
        -> void {
        check(instances_.IsValidIndex(local_index));

        auto const matrix{FTransform3f{rotation, position, scale}.ToMatrixWithScale()};
        auto& instance{instances_[local_index]};
        instance.origin = FVector4f{position, 0.0f};
        instance.transform_row_0 = FVector4f{matrix.M[0][0], matrix.M[0][1], matrix.M[0][2], 0.0f};
        instance.transform_row_1 = FVector4f{matrix.M[1][0], matrix.M[1][1], matrix.M[1][2], 0.0f};
        instance.transform_row_2 = FVector4f{matrix.M[2][0], matrix.M[2][1], matrix.M[2][2], 0.0f};

        if (has_mesh_bounds_) {
            auto const center{position + rotation.RotateVector(mesh_bounds_origin_ * scale)};
            auto const radius{mesh_bounds_radius_ * scale.GetAbsMax()};
            auto const extent{FVector3f{radius}};
            bounds_ += FBox3f{center - extent, center + extent};
        }
    }

    auto bounds() const -> FBox3f const& { return bounds_; }
  private:
    TArrayView<FSandboxISMCRenderInstance> instances_;
    FBox3f bounds_{ForceInit};
    int32 first_index_{0};
    FVector3f mesh_bounds_origin_{FVector3f::ZeroVector};
    float mesh_bounds_radius_{0.0f};
    bool has_mesh_bounds_{false};
};
