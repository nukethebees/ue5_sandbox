#pragma once

#include "SandboxISMCInstanceRange.h"
#include "SandboxISMCRenderInstance.h"

#include "Containers/ArrayView.h"
#include "Math/Box.h"
#include "Math/Quat.h"
#include "Math/Transform.h"
#include "Math/Vector.h"

enum class ESandboxISMCBoundsMode : uint8 {
    Calculate,
    Supplied,
};

class SANDBOXISMC_API FSandboxISMCInstanceChunkWriter final {
  public:
    FSandboxISMCInstanceChunkWriter(TArrayView<FSandboxISMCRenderInstance> instances,
                                    TArrayView<float> custom_data,
                                    int32 num_custom_data_floats,
                                    int32 first_index,
                                    FBox3f position_bounds,
                                    FVector3f position_root,
                                    FVector3f mesh_bounds_origin,
                                    FVector3f mesh_bounds_extent)
        : instances_{instances}
        , custom_data_{custom_data}
        , num_custom_data_floats_{num_custom_data_floats}
        , first_index_{first_index}
        , position_bounds_{position_bounds}
        , position_root_{position_root}
        , mesh_bounds_origin_{mesh_bounds_origin}
        , mesh_bounds_extent_{mesh_bounds_extent} {
        check(num_custom_data_floats >= 0);
        check(custom_data.Num() == instances.Num() * num_custom_data_floats);
    }

    auto first_index() const -> int32 { return first_index_; }
    auto num() const -> int32 { return instances_.Num(); }
    auto range() const -> FSandboxISMCInstanceRange { return {first_index_, instances_.Num()}; }
    auto num_custom_data_floats() const -> int32 { return num_custom_data_floats_; }

    static auto supports_scale(FVector3f scale) -> bool {
        return scale.X >= 0.0f && scale.Y >= 0.0f && scale.Z >= 0.0f && scale.X <= 31.875f &&
               scale.Y <= 31.875f && scale.Z <= 31.875f;
    }

    auto custom_data(int32 local_index) -> TArrayView<float> {
        check(instances_.IsValidIndex(local_index));
        return custom_data_.Slice(local_index * num_custom_data_floats_, num_custom_data_floats_);
    }

    // Views cover this entire chunk and must not overlap the packed output storage.
    // Rotations must be finite and normalized.
    template <ESandboxISMCBoundsMode BoundsMode>
    auto set_transforms(TConstArrayView<FVector3f> positions,
                        TConstArrayView<FQuat4f> rotations,
                        TConstArrayView<FVector3f> scales) -> void;

    // Source geometry bounds. The component adds codec error once per snapshot.
    auto bounds() const -> FBox3f const& { return bounds_; }

    static auto expand_render_bounds(FBox3f source, FVector3f mesh_origin, FVector3f mesh_extent)
        -> FBox3f {
        if (!source.IsValid) {
            return source;
        }
        auto const radius{(mesh_origin.GetAbs() + mesh_extent).Size()};
        auto const codec_error{ml::sandbox_ismc::geometry_error(radius)};
        auto const magnitude{source.Min.GetAbs().ComponentMax(source.Max.GetAbs())};
        auto const margin{FVector3f{codec_error} +
                          (magnitude + FVector3f{codec_error + 1.0f}) * (16.0f * FLT_EPSILON)};
        return FBox3f{source.Min - margin, source.Max + margin};
    }
  private:
    TArrayView<FSandboxISMCRenderInstance> instances_;
    TArrayView<float> custom_data_;
    FBox3f bounds_{ForceInit};
    int32 num_custom_data_floats_{0};
    int32 first_index_{0};
    FBox3f position_bounds_{ForceInit};
    FVector3f position_root_{FVector3f::ZeroVector};
    FVector3f mesh_bounds_origin_{FVector3f::ZeroVector};
    FVector3f mesh_bounds_extent_{FVector3f::ZeroVector};
};

extern template SANDBOXISMC_API auto
    FSandboxISMCInstanceChunkWriter::set_transforms<ESandboxISMCBoundsMode::Calculate>(
        TConstArrayView<FVector3f>, TConstArrayView<FQuat4f>, TConstArrayView<FVector3f>) -> void;
extern template SANDBOXISMC_API auto
    FSandboxISMCInstanceChunkWriter::set_transforms<ESandboxISMCBoundsMode::Supplied>(
        TConstArrayView<FVector3f>, TConstArrayView<FQuat4f>, TConstArrayView<FVector3f>) -> void;
