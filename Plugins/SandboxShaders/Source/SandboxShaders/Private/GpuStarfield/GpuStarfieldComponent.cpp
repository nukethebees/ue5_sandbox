#include "SandboxShaders/GpuStarfield/GpuStarfieldComponent.h"

#include "Containers/ResourceArray.h"
#include "Engine/Engine.h"
#include "MaterialDomain.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "MeshDrawShaderBindings.h"
#include "MeshMaterialShader.h"
#include "PrimitiveSceneProxy.h"
#include "PrimitiveViewRelevance.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "RenderingThread.h"
#include "RenderResource.h"
#include "RHIResourceUtils.h"
#include "SceneInterface.h"
#include "SceneManagement.h"
#include "StaticMeshResources.h"
#include "UObject/ConstructorHelpers.h"
#include "VertexFactory.h"

DEFINE_LOG_CATEGORY_STATIC(LogGpuStarfield, Log, All);

namespace {
constexpr int32 maximum_star_count{1000000};
constexpr float base_starfield_radius{100000000.0f};
constexpr float base_star_size{50000.0f};

auto generate_band_direction(FRandomStream& random_stream, float const width_degrees) -> FVector {
    auto const longitude{random_stream.FRandRange(-UE_PI, UE_PI)};
    auto latitude{0.0f};
    do {
        auto const normal_sample_radius{
            FMath::Sqrt(-2.0f * FMath::Loge(FMath::Max(random_stream.FRand(), UE_SMALL_NUMBER)))};
        auto const normal_sample{normal_sample_radius *
                                 FMath::Cos(2.0f * UE_PI * random_stream.FRand())};
        latitude = normal_sample * FMath::DegreesToRadians(width_degrees);
    } while (FMath::Abs(latitude) > UE_HALF_PI);

    auto const latitude_cosine{FMath::Cos(latitude)};
    return {latitude_cosine * FMath::Cos(longitude),
            latitude_cosine * FMath::Sin(longitude),
            FMath::Sin(latitude)};
}

auto generate_cluster_direction(FRandomStream& random_stream,
                                FVector const& cluster_direction,
                                float const width_degrees) -> FVector {
    FVector cluster_tangent;
    FVector cluster_bitangent;
    cluster_direction.FindBestAxisVectors(cluster_tangent, cluster_bitangent);

    auto const normal_sample_radius{
        FMath::Sqrt(-2.0f * FMath::Loge(FMath::Max(random_stream.FRand(), UE_SMALL_NUMBER)))};
    auto const angular_distance{FMath::Min(normal_sample_radius *
                                               FMath::DegreesToRadians(width_degrees),
                                           UE_HALF_PI)};
    auto const azimuth{random_stream.FRandRange(-UE_PI, UE_PI)};
    auto const radial_direction{cluster_tangent * FMath::Cos(azimuth) +
                                cluster_bitangent * FMath::Sin(azimuth)};
    return cluster_direction * FMath::Cos(angular_distance) +
           radial_direction * FMath::Sin(angular_distance);
}

auto calculate_dust_lane_attenuation(FVector const& direction,
                                     float const strength,
                                     float const width_degrees,
                                     float const irregularity) -> float {
    auto const longitude{static_cast<float>(FMath::Atan2(direction.Y, direction.X))};
    auto const latitude{static_cast<float>(FMath::Asin(FMath::Clamp(direction.Z, -1.0, 1.0)))};
    auto const base_width{FMath::DegreesToRadians(width_degrees)};

    auto const centre_wave{0.4f * FMath::Sin(2.0f * longitude + 0.7f) +
                           0.2f * FMath::Sin(5.0f * longitude - 1.3f)};
    auto const lane_centre{base_width * irregularity * centre_wave};
    auto const width_scale{
        FMath::Max(1.0f + irregularity * 0.3f * FMath::Sin(3.0f * longitude + 2.1f), 0.35f)};
    auto const lane_width{base_width * width_scale};
    auto const normalised_latitude{(latitude - lane_centre) / lane_width};
    auto const latitude_profile{FMath::Exp(-0.5f * normalised_latitude * normalised_latitude)};

    auto const opacity_wave{0.55f * FMath::Sin(2.0f * longitude - 0.4f) +
                            0.3f * FMath::Sin(5.0f * longitude + 1.7f) +
                            0.15f * FMath::Sin(11.0f * longitude - 0.9f)};
    auto const irregular_opacity{0.55f + 0.45f * (opacity_wave * 0.5f + 0.5f)};
    auto const opacity{FMath::Lerp(1.0f, irregular_opacity, irregularity)};
    return FMath::Clamp(1.0f - strength * latitude_profile * opacity, 0.0f, 1.0f);
}

struct FGpuStarfieldRenderParameters {
    float starfield_radius{0.0f};
    float global_star_size{0.0f};
    float global_brightness{0.0f};
    float star_colour_variation_strength{0.0f};
    float bright_star_size_multiplier{1.0f};
    float bright_star_brightness_multiplier{1.0f};
    float parallax_strength{0.0f};
    float bright_star_shape_strength{0.0f};
    float twinkle_strength{0.0f};
    float twinkle_speed{0.0f};
    float dust_lane_strength{0.0f};
    float dust_lane_width_degrees{0.0f};
    float dust_lane_irregularity{0.0f};
    float galactic_haze_strength{0.0f};
    float galactic_haze_width_degrees{0.0f};
    FVector3f galactic_haze_colour{FVector3f::ZeroVector};
    float galactic_core_strength{0.0f};
    float galactic_core_width_degrees{0.0f};
    FVector3f galactic_core_colour{FVector3f::ZeroVector};
    float nebular_knot_strength{0.0f};
    float nebular_knot_size_degrees{0.0f};
    FVector3f nebular_knot_colour{FVector3f::ZeroVector};
    float render_haze{0.0f};
};

auto make_render_parameters(FGpuStarfieldSettings const& settings)
    -> FGpuStarfieldRenderParameters {
    auto const& star{settings.star};
    auto const& bright_star{settings.bright_star};
    auto const& twinkle{settings.twinkle};
    auto const& dust_lane{settings.dust_lane};
    auto const& galactic_haze{settings.galactic_haze};
    auto const& galactic_core{settings.galactic_core};
    auto const& nebular_knot{settings.nebular_knot};
    return {
        .starfield_radius = base_starfield_radius * star.scale,
        .global_star_size = base_star_size * star.scale * star.size_multiplier,
        .global_brightness = star.global_brightness,
        .star_colour_variation_strength = star.colour_variation_strength,
        .bright_star_size_multiplier = bright_star.size_multiplier,
        .bright_star_brightness_multiplier = bright_star.brightness_multiplier,
        .parallax_strength = star.parallax_strength,
        .bright_star_shape_strength = bright_star.shape_strength,
        .twinkle_strength = twinkle.strength,
        .twinkle_speed = twinkle.speed,
        .dust_lane_strength = dust_lane.strength,
        .dust_lane_width_degrees = dust_lane.width_degrees,
        .dust_lane_irregularity = dust_lane.irregularity,
        .galactic_haze_strength = galactic_haze.strength,
        .galactic_haze_width_degrees = galactic_haze.width_degrees,
        .galactic_haze_colour = {galactic_haze.colour.R,
                                 galactic_haze.colour.G,
                                 galactic_haze.colour.B},
        .galactic_core_strength = galactic_core.strength,
        .galactic_core_width_degrees = galactic_core.width_degrees,
        .galactic_core_colour = {
            galactic_core.colour.R, galactic_core.colour.G, galactic_core.colour.B},
        .nebular_knot_strength = nebular_knot.strength,
        .nebular_knot_size_degrees = nebular_knot.size_degrees,
        .nebular_knot_colour = {
            nebular_knot.colour.R, nebular_knot.colour.G, nebular_knot.colour.B},
    };
}

class FGpuStarfieldQuadVertexBuffer : public FVertexBuffer {
  public:
    void InitRHI(FRHICommandListBase& rhi_command_list) override {
        FVector2f const vertices[]{
            {-1.0f, -1.0f},
            {1.0f, -1.0f},
            {-1.0f, 1.0f},
            {1.0f, 1.0f},
        };
        VertexBufferRHI =
            UE::RHIResourceUtils::CreateVertexBufferFromArray(rhi_command_list,
                                                              TEXT("GpuStarfield.QuadVertices"),
                                                              BUF_Static,
                                                              MakeConstArrayView(vertices));
    }
};

class FGpuStarfieldQuadIndexBuffer : public FIndexBuffer {
  public:
    void InitRHI(FRHICommandListBase& rhi_command_list) override {
        uint16 const indices[]{0, 2, 1, 1, 2, 3};
        IndexBufferRHI =
            UE::RHIResourceUtils::CreateIndexBufferFromArray(rhi_command_list,
                                                             TEXT("GpuStarfield.QuadIndices"),
                                                             BUF_Static,
                                                             MakeConstArrayView(indices));
    }
};

TGlobalResource<FGpuStarfieldQuadVertexBuffer> gpu_starfield_quad_vertex_buffer;
TGlobalResource<FGpuStarfieldQuadIndexBuffer> gpu_starfield_quad_index_buffer;

struct FGpuStarfieldBatchElementUserData final : public FOneFrameResource {
    FShaderResourceViewRHIRef star_data_srv;
    FGpuStarfieldRenderParameters parameters;
};

class FGpuStarfieldVertexFactoryShaderParameters final : public FVertexFactoryShaderParameters {
    DECLARE_TYPE_LAYOUT(FGpuStarfieldVertexFactoryShaderParameters, NonVirtual);
  public:
    void Bind(FShaderParameterMap const& parameter_map) {
        star_data_.Bind(parameter_map, TEXT("GpuStarfieldStarData"));
        starfield_radius_.Bind(parameter_map, TEXT("GpuStarfieldRadius"));
        global_star_size_.Bind(parameter_map, TEXT("GpuStarfieldGlobalStarSize"));
        global_brightness_.Bind(parameter_map, TEXT("GpuStarfieldGlobalBrightness"));
        star_colour_variation_strength_.Bind(parameter_map,
                                             TEXT("GpuStarfieldColourVariationStrength"));
        bright_star_size_multiplier_.Bind(parameter_map,
                                          TEXT("GpuStarfieldBrightStarSizeMultiplier"));
        bright_star_brightness_multiplier_.Bind(parameter_map,
                                                TEXT("GpuStarfieldBrightStarBrightnessMultiplier"));
        parallax_strength_.Bind(parameter_map, TEXT("GpuStarfieldParallaxStrength"));
        bright_star_shape_strength_.Bind(parameter_map,
                                         TEXT("GpuStarfieldBrightStarShapeStrength"));
        twinkle_strength_.Bind(parameter_map, TEXT("GpuStarfieldTwinkleStrength"));
        twinkle_speed_.Bind(parameter_map, TEXT("GpuStarfieldTwinkleSpeed"));
        dust_lane_strength_.Bind(parameter_map, TEXT("GpuStarfieldDustLaneStrength"));
        dust_lane_width_degrees_.Bind(parameter_map, TEXT("GpuStarfieldDustLaneWidthDegrees"));
        dust_lane_irregularity_.Bind(parameter_map, TEXT("GpuStarfieldDustLaneIrregularity"));
        galactic_haze_strength_.Bind(parameter_map, TEXT("GpuStarfieldGalacticHazeStrength"));
        galactic_haze_width_degrees_.Bind(parameter_map,
                                          TEXT("GpuStarfieldGalacticHazeWidthDegrees"));
        galactic_haze_colour_.Bind(parameter_map, TEXT("GpuStarfieldGalacticHazeColour"));
        galactic_core_strength_.Bind(parameter_map, TEXT("GpuStarfieldGalacticCoreStrength"));
        galactic_core_width_degrees_.Bind(parameter_map,
                                          TEXT("GpuStarfieldGalacticCoreWidthDegrees"));
        galactic_core_colour_.Bind(parameter_map, TEXT("GpuStarfieldGalacticCoreColour"));
        nebular_knot_strength_.Bind(parameter_map, TEXT("GpuStarfieldNebularKnotStrength"));
        nebular_knot_size_degrees_.Bind(parameter_map,
                                        TEXT("GpuStarfieldNebularKnotSizeDegrees"));
        nebular_knot_colour_.Bind(parameter_map, TEXT("GpuStarfieldNebularKnotColour"));
        render_haze_.Bind(parameter_map, TEXT("GpuStarfieldRenderHaze"));
    }

    void GetElementShaderBindings(FSceneInterface const* scene,
                                  FSceneView const* view,
                                  FMeshMaterialShader const* shader,
                                  EVertexInputStreamType input_stream_type,
                                  ERHIFeatureLevel::Type feature_level,
                                  FVertexFactory const* vertex_factory,
                                  FMeshBatchElement const& batch_element,
                                  FMeshDrawSingleShaderBindings& shader_bindings,
                                  FVertexInputStreamArray& vertex_streams) const;
  private:
    LAYOUT_FIELD(FShaderResourceParameter, star_data_);
    LAYOUT_FIELD(FShaderParameter, starfield_radius_);
    LAYOUT_FIELD(FShaderParameter, global_star_size_);
    LAYOUT_FIELD(FShaderParameter, global_brightness_);
    LAYOUT_FIELD(FShaderParameter, star_colour_variation_strength_);
    LAYOUT_FIELD(FShaderParameter, bright_star_size_multiplier_);
    LAYOUT_FIELD(FShaderParameter, bright_star_brightness_multiplier_);
    LAYOUT_FIELD(FShaderParameter, parallax_strength_);
    LAYOUT_FIELD(FShaderParameter, bright_star_shape_strength_);
    LAYOUT_FIELD(FShaderParameter, twinkle_strength_);
    LAYOUT_FIELD(FShaderParameter, twinkle_speed_);
    LAYOUT_FIELD(FShaderParameter, dust_lane_strength_);
    LAYOUT_FIELD(FShaderParameter, dust_lane_width_degrees_);
    LAYOUT_FIELD(FShaderParameter, dust_lane_irregularity_);
    LAYOUT_FIELD(FShaderParameter, galactic_haze_strength_);
    LAYOUT_FIELD(FShaderParameter, galactic_haze_width_degrees_);
    LAYOUT_FIELD(FShaderParameter, galactic_haze_colour_);
    LAYOUT_FIELD(FShaderParameter, galactic_core_strength_);
    LAYOUT_FIELD(FShaderParameter, galactic_core_width_degrees_);
    LAYOUT_FIELD(FShaderParameter, galactic_core_colour_);
    LAYOUT_FIELD(FShaderParameter, nebular_knot_strength_);
    LAYOUT_FIELD(FShaderParameter, nebular_knot_size_degrees_);
    LAYOUT_FIELD(FShaderParameter, nebular_knot_colour_);
    LAYOUT_FIELD(FShaderParameter, render_haze_);
};

IMPLEMENT_TYPE_LAYOUT(FGpuStarfieldVertexFactoryShaderParameters);

class FGpuStarfieldVertexFactory final : public FVertexFactory {
    DECLARE_VERTEX_FACTORY_TYPE(FGpuStarfieldVertexFactory);
  public:
    explicit FGpuStarfieldVertexFactory(ERHIFeatureLevel::Type const feature_level)
        : FVertexFactory{feature_level} {}

    void InitRHI(FRHICommandListBase& rhi_command_list) override {
        FVertexDeclarationElementList elements;
        elements.Add(AccessStreamComponent(
            FVertexStreamComponent{
                &gpu_starfield_quad_vertex_buffer, 0, sizeof(FVector2f), VET_Float2},
            0));
        InitDeclaration(elements);
    }

    static auto
        ShouldCompilePermutation(FVertexFactoryShaderPermutationParameters const& parameters)
            -> bool {
        auto const& material{parameters.MaterialParameters};
        auto const is_starfield_material{material.MaterialDomain == MD_Surface &&
                                         material.BlendMode == BLEND_Additive &&
                                         material.ShadingModels.HasShadingModel(MSM_Unlit) &&
                                         material.bIsUsedWithParticleSprites};
        return IsFeatureLevelSupported(parameters.Platform, ERHIFeatureLevel::SM5) &&
               (is_starfield_material || material.bIsSpecialEngineMaterial);
    }

    static void
        ModifyCompilationEnvironment(FVertexFactoryShaderPermutationParameters const& parameters,
                                     FShaderCompilerEnvironment& environment) {
        FVertexFactory::ModifyCompilationEnvironment(parameters, environment);
    }
};

void FGpuStarfieldVertexFactoryShaderParameters::GetElementShaderBindings(
    FSceneInterface const* const scene,
    FSceneView const* const view,
    FMeshMaterialShader const* const shader,
    EVertexInputStreamType const input_stream_type,
    ERHIFeatureLevel::Type const feature_level,
    FVertexFactory const* const vertex_factory,
    FMeshBatchElement const& batch_element,
    FMeshDrawSingleShaderBindings& shader_bindings,
    FVertexInputStreamArray& vertex_streams) const {
    auto const* const user_data{
        static_cast<FGpuStarfieldBatchElementUserData const*>(batch_element.UserData)};
    check(user_data != nullptr);

    shader_bindings.Add(star_data_, user_data->star_data_srv);
    shader_bindings.Add(starfield_radius_, user_data->parameters.starfield_radius);
    shader_bindings.Add(global_star_size_, user_data->parameters.global_star_size);
    shader_bindings.Add(global_brightness_, user_data->parameters.global_brightness);
    shader_bindings.Add(star_colour_variation_strength_,
                        user_data->parameters.star_colour_variation_strength);
    shader_bindings.Add(bright_star_size_multiplier_,
                        user_data->parameters.bright_star_size_multiplier);
    shader_bindings.Add(bright_star_brightness_multiplier_,
                        user_data->parameters.bright_star_brightness_multiplier);
    shader_bindings.Add(parallax_strength_, user_data->parameters.parallax_strength);
    shader_bindings.Add(bright_star_shape_strength_,
                        user_data->parameters.bright_star_shape_strength);
    shader_bindings.Add(twinkle_strength_, user_data->parameters.twinkle_strength);
    shader_bindings.Add(twinkle_speed_, user_data->parameters.twinkle_speed);
    shader_bindings.Add(dust_lane_strength_, user_data->parameters.dust_lane_strength);
    shader_bindings.Add(dust_lane_width_degrees_, user_data->parameters.dust_lane_width_degrees);
    shader_bindings.Add(dust_lane_irregularity_, user_data->parameters.dust_lane_irregularity);
    shader_bindings.Add(galactic_haze_strength_, user_data->parameters.galactic_haze_strength);
    shader_bindings.Add(galactic_haze_width_degrees_,
                        user_data->parameters.galactic_haze_width_degrees);
    shader_bindings.Add(galactic_haze_colour_, user_data->parameters.galactic_haze_colour);
    shader_bindings.Add(galactic_core_strength_, user_data->parameters.galactic_core_strength);
    shader_bindings.Add(galactic_core_width_degrees_,
                        user_data->parameters.galactic_core_width_degrees);
    shader_bindings.Add(galactic_core_colour_, user_data->parameters.galactic_core_colour);
    shader_bindings.Add(nebular_knot_strength_, user_data->parameters.nebular_knot_strength);
    shader_bindings.Add(nebular_knot_size_degrees_,
                        user_data->parameters.nebular_knot_size_degrees);
    shader_bindings.Add(nebular_knot_colour_, user_data->parameters.nebular_knot_colour);
    shader_bindings.Add(render_haze_, user_data->parameters.render_haze);
}

IMPLEMENT_VERTEX_FACTORY_PARAMETER_TYPE(FGpuStarfieldVertexFactory,
                                        SF_Vertex,
                                        FGpuStarfieldVertexFactoryShaderParameters);

IMPLEMENT_VERTEX_FACTORY_TYPE(
    FGpuStarfieldVertexFactory,
    "/Plugin/SandboxShaders/Private/GpuStarfield/GpuStarfieldVertexFactory.ush",
    EVertexFactoryFlags::UsedWithMaterials);

class FGpuStarfieldBuffer final : public FRenderResource {
  public:
    explicit FGpuStarfieldBuffer(TConstArrayView<FGpuStarfieldGpuData> const star_data)
        : initial_data_{star_data} {}

    void InitRHI(FRHICommandListBase& rhi_command_list) override {
        if (initial_data_.IsEmpty()) {
            return;
        }

        FResourceArrayUploadArrayView upload_view{initial_data_};
        auto const buffer_size{initial_data_.Num() * sizeof(FGpuStarfieldGpuData)};
        auto const description{
            FRHIBufferCreateDesc::CreateStructured(
                TEXT("GpuStarfield.StarData"), buffer_size, sizeof(FGpuStarfieldGpuData))
                .AddUsage(EBufferUsageFlags::ShaderResource | EBufferUsageFlags::Static)
                .SetInitialState(ERHIAccess::SRVMask)
                .SetInitActionResourceArray(&upload_view)};
        buffer_ = rhi_command_list.CreateBuffer(description);
        srv_ = rhi_command_list.CreateShaderResourceView(
            buffer_, FRHIViewDesc::CreateBufferSRV().SetTypeFromBuffer(buffer_));
        initial_data_.Reset();
    }

    void ReleaseRHI() override {
        srv_.SafeRelease();
        buffer_.SafeRelease();
    }

    auto srv() const -> FShaderResourceViewRHIRef { return srv_; }
  private:
    TArray<FGpuStarfieldGpuData> initial_data_;
    FBufferRHIRef buffer_;
    FShaderResourceViewRHIRef srv_;
};

class FGpuStarfieldSceneProxy final : public FPrimitiveSceneProxy {
  public:
    FGpuStarfieldSceneProxy(UGpuStarfieldComponent const* const component,
                            TConstArrayView<FGpuStarfieldGpuData> const star_data,
                            UMaterialInterface const* const material,
                            FGpuStarfieldRenderParameters const parameters)
        : FPrimitiveSceneProxy{component}
        , star_buffer_{star_data}
        , vertex_factory_{GetScene().GetFeatureLevel()}
        , material_render_proxy_{material->GetRenderProxy()}
        , material_relevance_{material->GetRelevance_Concurrent(GetScene().GetShaderPlatform())}
        , parameters_{parameters}
        , star_count_{star_data.Num()} {
        bWillEverBeLit = false;
        bIsAlwaysVisible = true;
        BeginInitResource(&star_buffer_);
        BeginInitResource(&vertex_factory_);
    }

    ~FGpuStarfieldSceneProxy() override {
        vertex_factory_.ReleaseResource();
        star_buffer_.ReleaseResource();
    }

    void GetDynamicMeshElements(TArray<FSceneView const*> const& views,
                                FSceneViewFamily const& view_family,
                                uint32 const visibility_map,
                                FMeshElementCollector& collector) const override {
        QUICK_SCOPE_CYCLE_COUNTER(STAT_GpuStarfieldSceneProxy_GetDynamicMeshElements);
        CSV_SCOPED_TIMING_STAT_EXCLUSIVE(GpuStarfieldSubmit);

        auto const star_srv{star_buffer_.srv()};
        if (!star_srv.IsValid() || star_count_ <= 0) {
            return;
        }

        auto const view_count{views.Num()};
        for (int32 view_index{0}; view_index < view_count; ++view_index) {
            if ((visibility_map & (1u << view_index)) == 0) {
                continue;
            }

            auto const add_quad_draw = [&](int32 const instance_count, bool const render_haze) {
                auto& user_data{
                    collector.AllocateOneFrameResource<FGpuStarfieldBatchElementUserData>()};
                user_data.star_data_srv = star_srv;
                user_data.parameters = parameters_;
                user_data.parameters.render_haze = render_haze ? 1.0f : 0.0f;

                auto& mesh{collector.AllocateMesh()};
                mesh.VertexFactory = &vertex_factory_;
                mesh.MaterialRenderProxy = material_render_proxy_;
                mesh.Type = PT_TriangleList;
                mesh.DepthPriorityGroup = SDPG_World;
                mesh.CastShadow = false;
                mesh.bUseAsOccluder = false;
                mesh.bCanApplyViewModeOverrides = false;
                mesh.bDisableBackfaceCulling = true;

                auto& element{mesh.Elements[0]};
                element.IndexBuffer = &gpu_starfield_quad_index_buffer;
                element.FirstIndex = 0;
                element.NumPrimitives = 2;
                element.NumInstances = instance_count;
                element.MinVertexIndex = 0;
                element.MaxVertexIndex = 3;
                element.PrimitiveUniformBuffer = GetUniformBuffer();
                element.UserData = &user_data;

                collector.AddMesh(view_index, mesh);
            };

            if (parameters_.galactic_haze_strength > 0.0f) {
                add_quad_draw(1, true);
            }
            add_quad_draw(star_count_, false);
        }
    }

    auto GetViewRelevance(FSceneView const* const view) const -> FPrimitiveViewRelevance override {
        FPrimitiveViewRelevance relevance;
        relevance.bDrawRelevance = IsShown(view);
        relevance.bDynamicRelevance = true;
        relevance.bShadowRelevance = false;
        relevance.bRenderInMainPass = ShouldRenderInMainPass();
        material_relevance_.SetPrimitiveViewRelevance(relevance);
        relevance.bVelocityRelevance = false;
        return relevance;
    }

    auto CanBeOccluded() const -> bool override { return !material_relevance_.bDisableDepthTest; }

    auto GetMemoryFootprint() const -> uint32 override {
        return sizeof(*this) + GetAllocatedSize();
    }

    auto GetAllocatedSize() const -> uint32 { return FPrimitiveSceneProxy::GetAllocatedSize(); }

    auto GetTypeHash() const -> SIZE_T override {
        static size_t unique_pointer;
        return reinterpret_cast<size_t>(&unique_pointer);
    }

    void set_parameters_render_thread(FGpuStarfieldRenderParameters const parameters) {
        check(IsInRenderingThread());
        parameters_ = parameters;
    }
  private:
    FGpuStarfieldBuffer star_buffer_;
    FGpuStarfieldVertexFactory vertex_factory_;
    FMaterialRenderProxy const* material_render_proxy_;
    FMaterialRelevance material_relevance_;
    FGpuStarfieldRenderParameters parameters_;
    int32 star_count_{0};
};
}

UGpuStarfieldComponent::UGpuStarfieldComponent() {
    PrimaryComponentTick.bCanEverTick = false;
    SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SetGenerateOverlapEvents(false);
    SetCastShadow(false);
    bCastDynamicShadow = false;
    bCastStaticShadow = false;
    CanCharacterStepUpOn = ECB_No;

    static ConstructorHelpers::FObjectFinder<UMaterialInterface> const material{
        TEXT("/SandboxShaders/GpuStarfield/M_GpuStarfield.M_GpuStarfield")};
    if (material.Succeeded()) {
        material_ = material.Object;
    }
}

void UGpuStarfieldComponent::apply_settings(FGpuStarfieldSettings const& settings) {
    auto normalised{settings};
    auto& generation{normalised.generation};
    auto& star{normalised.star};
    auto& galactic_band{normalised.galactic_band};
    auto& stellar_cluster{normalised.stellar_cluster};
    auto& dust_lane{normalised.dust_lane};
    auto& bright_star{normalised.bright_star};
    auto& twinkle{normalised.twinkle};
    auto& galactic_haze{normalised.galactic_haze};
    auto& galactic_core{normalised.galactic_core};
    auto& nebular_knot{normalised.nebular_knot};

    generation.star_count = FMath::Clamp(generation.star_count, 1, maximum_star_count);
    star.scale = FMath::Max(star.scale, 0.001f);
    star.size_multiplier = FMath::Max(star.size_multiplier, 0.0f);
    star.global_brightness = FMath::Max(star.global_brightness, 0.0f);
    star.colour_variation_strength = FMath::Clamp(star.colour_variation_strength, 0.0f, 1.0f);
    star.parallax_strength = FMath::Clamp(star.parallax_strength, 0.0f, 1.0f);
    galactic_band.strength = FMath::Clamp(galactic_band.strength, 0.0f, 1.0f);
    galactic_band.width_degrees = FMath::Clamp(galactic_band.width_degrees, 1.0f, 45.0f);
    stellar_cluster.strength = FMath::Clamp(stellar_cluster.strength, 0.0f, 1.0f);
    stellar_cluster.width_degrees = FMath::Clamp(stellar_cluster.width_degrees, 1.0f, 20.0f);
    dust_lane.strength = FMath::Clamp(dust_lane.strength, 0.0f, 1.0f);
    dust_lane.width_degrees = FMath::Clamp(dust_lane.width_degrees, 0.5f, 20.0f);
    dust_lane.irregularity = FMath::Clamp(dust_lane.irregularity, 0.0f, 1.0f);
    bright_star.fraction = FMath::Clamp(bright_star.fraction, 0.0f, 0.1f);
    bright_star.size_multiplier = FMath::Max(bright_star.size_multiplier, 1.0f);
    bright_star.brightness_multiplier = FMath::Max(bright_star.brightness_multiplier, 1.0f);
    bright_star.shape_strength = FMath::Clamp(bright_star.shape_strength, 0.0f, 1.0f);
    twinkle.strength = FMath::Clamp(twinkle.strength, 0.0f, 0.5f);
    twinkle.speed = FMath::Clamp(twinkle.speed, 0.0f, 2.0f);
    galactic_haze.strength = FMath::Clamp(galactic_haze.strength, 0.0f, 10.0f);
    galactic_haze.width_degrees = FMath::Clamp(galactic_haze.width_degrees, 1.0f, 60.0f);
    galactic_haze.colour.R = FMath::Max(galactic_haze.colour.R, 0.0f);
    galactic_haze.colour.G = FMath::Max(galactic_haze.colour.G, 0.0f);
    galactic_haze.colour.B = FMath::Max(galactic_haze.colour.B, 0.0f);
    galactic_core.strength = FMath::Clamp(galactic_core.strength, 0.0f, 10.0f);
    galactic_core.width_degrees = FMath::Clamp(galactic_core.width_degrees, 1.0f, 90.0f);
    galactic_core.colour.R = FMath::Max(galactic_core.colour.R, 0.0f);
    galactic_core.colour.G = FMath::Max(galactic_core.colour.G, 0.0f);
    galactic_core.colour.B = FMath::Max(galactic_core.colour.B, 0.0f);
    nebular_knot.strength = FMath::Clamp(nebular_knot.strength, 0.0f, 10.0f);
    nebular_knot.size_degrees = FMath::Clamp(nebular_knot.size_degrees, 2.0f, 30.0f);
    nebular_knot.colour.R = FMath::Max(nebular_knot.colour.R, 0.0f);
    nebular_knot.colour.G = FMath::Max(nebular_knot.colour.G, 0.0f);
    nebular_knot.colour.B = FMath::Max(nebular_knot.colour.B, 0.0f);

    auto const structural_change{
        !has_generated_stars_ || settings_.generation.star_count != generation.star_count ||
        settings_.generation.random_seed != generation.random_seed ||
        settings_.galactic_band.strength != galactic_band.strength ||
        settings_.galactic_band.width_degrees != galactic_band.width_degrees ||
        settings_.stellar_cluster.strength != stellar_cluster.strength ||
        settings_.stellar_cluster.width_degrees != stellar_cluster.width_degrees ||
        settings_.dust_lane.strength != dust_lane.strength ||
        settings_.dust_lane.width_degrees != dust_lane.width_degrees ||
        settings_.dust_lane.irregularity != dust_lane.irregularity ||
        settings_.bright_star.fraction != bright_star.fraction};
    auto const bounds_change{settings_.star.scale != star.scale ||
                             settings_.star.size_multiplier != star.size_multiplier ||
                             settings_.bright_star.size_multiplier != bright_star.size_multiplier};
    auto const shader_parameter_change{
        bounds_change || settings_.star.global_brightness != star.global_brightness ||
        settings_.star.colour_variation_strength != star.colour_variation_strength ||
        settings_.star.parallax_strength != star.parallax_strength ||
        settings_.bright_star.brightness_multiplier != bright_star.brightness_multiplier ||
        settings_.bright_star.shape_strength != bright_star.shape_strength ||
        settings_.twinkle.strength != twinkle.strength ||
        settings_.twinkle.speed != twinkle.speed ||
        settings_.galactic_haze.strength != galactic_haze.strength ||
        settings_.galactic_haze.width_degrees != galactic_haze.width_degrees ||
        settings_.galactic_haze.colour != galactic_haze.colour ||
        settings_.galactic_core.strength != galactic_core.strength ||
        settings_.galactic_core.width_degrees != galactic_core.width_degrees ||
        settings_.galactic_core.colour != galactic_core.colour ||
        settings_.nebular_knot.strength != nebular_knot.strength ||
        settings_.nebular_knot.size_degrees != nebular_knot.size_degrees ||
        settings_.nebular_knot.colour != nebular_knot.colour};

    settings_ = normalised;
    if (structural_change) {
        generate_stars();
        UpdateBounds();
        MarkRenderStateDirty();
    } else if (shader_parameter_change) {
        if (bounds_change) {
            UpdateBounds();
            MarkRenderTransformDirty();
        }
        MarkRenderDynamicDataDirty();
    }
}

FPrimitiveSceneProxy* UGpuStarfieldComponent::CreateSceneProxy() {
    if (!IsValid(material_) || star_data_.IsEmpty()) {
        if (!IsValid(material_)) {
            UE_LOG(LogGpuStarfield,
                   Warning,
                   TEXT("GPU starfield material is unavailable."));
        }
        return nullptr;
    }

    return new FGpuStarfieldSceneProxy{
        this, star_data_, material_, make_render_parameters(settings_)};
}

FBoxSphereBounds UGpuStarfieldComponent::CalcBounds(FTransform const& local_to_world) const {
    auto const& star{settings_.star};
    auto const radius{base_starfield_radius * star.scale +
                      base_star_size * star.scale * star.size_multiplier *
                          settings_.bright_star.size_multiplier};
    return FBoxSphereBounds{FVector::ZeroVector, FVector{radius}, radius}.TransformBy(
        local_to_world);
}

void UGpuStarfieldComponent::SendRenderDynamicData_Concurrent() {
    Super::SendRenderDynamicData_Concurrent();

    if (SceneProxy == nullptr) {
        return;
    }

    auto* const scene_proxy{static_cast<FGpuStarfieldSceneProxy*>(SceneProxy)};
    auto const parameters{make_render_parameters(settings_)};
    ENQUEUE_RENDER_COMMAND(UpdateGpuStarfieldParameters)
    ([scene_proxy, parameters](FRHICommandListImmediate& rhi_command_list) {
        scene_proxy->set_parameters_render_thread(parameters);
    });
}

void UGpuStarfieldComponent::GetUsedMaterials(TArray<UMaterialInterface*>& out_materials,
                                              bool const get_debug_materials) const {
    if (IsValid(material_)) {
        out_materials.AddUnique(material_);
    }
}

void UGpuStarfieldComponent::generate_stars() {
    auto const& generation{settings_.generation};
    auto const& galactic_band{settings_.galactic_band};
    auto const& stellar_cluster{settings_.stellar_cluster};
    auto const& dust_lane{settings_.dust_lane};
    FRandomStream random_stream{generation.random_seed};
    star_data_.SetNumUninitialized(generation.star_count);

    constexpr int32 cluster_count{6};
    FVector cluster_directions[cluster_count];
    if (stellar_cluster.strength > 0.0f) {
        FRandomStream cluster_random_stream{generation.random_seed ^ 0x53a9b4d1};
        for (auto& cluster_direction : cluster_directions) {
            cluster_direction = cluster_random_stream.VRand();
            if (galactic_band.strength > 0.0f &&
                cluster_random_stream.FRand() < galactic_band.strength) {
                cluster_direction =
                    generate_band_direction(cluster_random_stream, galactic_band.width_degrees);
            }
        }
    }

    auto const star_count{star_data_.Num()};
    for (int32 star_index{0}; star_index < star_count; ++star_index) {
        auto direction{random_stream.VRand()};
        if (galactic_band.strength > 0.0f &&
            random_stream.FRand() < galactic_band.strength) {
            direction = generate_band_direction(random_stream, galactic_band.width_degrees);
        }
        if (stellar_cluster.strength > 0.0f &&
            random_stream.FRand() < stellar_cluster.strength) {
            auto const cluster_index{random_stream.RandRange(0, cluster_count - 1)};
            direction = generate_cluster_direction(random_stream,
                                                   cluster_directions[cluster_index],
                                                   stellar_cluster.width_degrees);
        }

        auto const population_roll{random_stream.FRand()};
        auto const depth_roll{random_stream.FRand()};
        auto const is_near_star{population_roll < 0.01f};
        auto depth_factor{1.0f};
        if (is_near_star) {
            depth_factor = FMath::Lerp(0.001f, 0.01f, depth_roll);
        } else if (population_roll < 0.15f) {
            depth_factor = FMath::Lerp(0.03f, 0.2f, depth_roll);
        } else {
            depth_factor = FMath::Lerp(0.5f, 1.0f, depth_roll);
        }

        auto magnitude{FMath::Pow(random_stream.FRand(), 4.0f)};
        if (is_near_star) {
            magnitude = FMath::Max(magnitude, 0.5f);
        }

        auto& star{star_data_[star_index]};
        star.direction = FVector3f{direction};
        star.size = FMath::Lerp(0.65f, 1.8f, magnitude);
        star.brightness = FMath::Lerp(0.08f, 1.0f, magnitude);
        if (dust_lane.strength > 0.0f) {
            star.brightness *= calculate_dust_lane_attenuation(direction,
                                                               dust_lane.strength,
                                                               dust_lane.width_degrees,
                                                               dust_lane.irregularity);
        }
        star.depth_factor = depth_factor;
        star.colour_temperature = (random_stream.FRand() + random_stream.FRand()) * 0.5f;
        star.bright_star_factor =
            population_roll < settings_.bright_star.fraction ? 1.0f : 0.0f;
    }

    has_generated_stars_ = true;
}
