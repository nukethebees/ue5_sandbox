#include "SandboxEditor/material/MaterialEmitter.h"

#include <lispb/indices.h>
#include <material_gen/BlendMode.h>
#include <material_gen/Indices.h>
#include <material_gen/MaterialDomain.h>
#include <material_gen/MaterialIR.h>
#include <material_gen/NodeKind.h>
#include <material_gen/Nodes.h>
#include <material_gen/PositionSpace.h>
#include <material_gen/SceneTexture.h>
#include <material_gen/ShadingModel.h>
#include <material_gen/TextureSamplerType.h>
#include <material_gen/ValueType.h>

#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/Texture.h"
#include "MaterialDomain.h"
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionAppendVector.h"
#include "Materials/MaterialExpressionCameraPositionWS.h"
#include "Materials/MaterialExpressionCameraVectorWS.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant2Vector.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionConstant4Vector.h"
#include "Materials/MaterialExpressionCosine.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionDivide.h"
#include "Materials/MaterialExpressionIf.h"
#include "Materials/MaterialExpressionLinearInterpolate.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionObjectPositionWS.h"
#include "Materials/MaterialExpressionPerInstanceCustomData.h"
#include "Materials/MaterialExpressionPixelNormalWS.h"
#include "Materials/MaterialExpressionSaturate.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionSceneTexture.h"
#include "Materials/MaterialExpressionSine.h"
#include "Materials/MaterialExpressionSubtract.h"
#include "Materials/MaterialExpressionTextureBase.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionTextureObject.h"
#include "Materials/MaterialExpressionTextureObjectParameter.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionTime.h"
#include "Materials/MaterialExpressionTransformPosition.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionVertexColor.h"
#include "Materials/MaterialExpressionVertexNormalWS.h"
#include "Materials/MaterialExpressionWorldPosition.h"
#include "Misc/PackageName.h"
#include "ShaderCompiler.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

#include <cstddef>
#include <cstdint>
#include <string>

namespace material_synth {
namespace {

auto text(MaterialIR const& ir, lispb::StringId const id) -> FString {
    auto const value{ir.text(id)};
    FUTF8ToTCHAR const converted{value.data(), static_cast<int32>(value.size())};
    return FString{converted.Length(), converted.Get()};
}

auto object_path(MaterialIR const& ir) -> FString {
    auto const package{text(ir, ir.settings.package_path)};
    auto const name{text(ir, ir.settings.name)};
    return package + TEXT(".") + name;
}

auto make_expression(UMaterial& material, UClass* const type, std::size_t const index)
    -> UMaterialExpression* {
    auto const x{-900 + static_cast<int32>(index % 4) * 260};
    auto const y{-300 + static_cast<int32>(index / 4) * 180};
    return UMaterialEditingLibrary::CreateMaterialExpression(&material, type, x, y);
}

auto custom_output_type(ValueType const type) -> ECustomMaterialOutputType {
    switch (type) {
        case ValueType::float1:
            return CMOT_Float1;
        case ValueType::float2:
            return CMOT_Float2;
        case ValueType::float3:
            return CMOT_Float3;
        case ValueType::float4:
            return CMOT_Float4;
        default:
            return CMOT_MAX;
    }
}

auto position_space(PositionSpace const space) -> EMaterialPositionTransformSource {
    return space == PositionSpace::world ? TRANSFORMPOSSOURCE_World : TRANSFORMPOSSOURCE_Local;
}

auto sampler_type(UTexture const* const texture) -> EMaterialSamplerType {
    return texture != nullptr && texture->SRGB ? SAMPLERTYPE_Color : SAMPLERTYPE_LinearColor;
}

auto sampler_type(TextureSamplerType const type) -> EMaterialSamplerType {
    return type == TextureSamplerType::linear_grayscale ? SAMPLERTYPE_LinearGrayscale
                                                        : SAMPLERTYPE_LinearColor;
}

struct ExpressionValue {
    UMaterialExpression* expression{};
    FString output_name;
};

auto output_name(Nodes const& nodes, NodeHandle node) -> FString {
    if ((nodes.kind(node) == NodeKind::parameter || nodes.kind(node) == NodeKind::sample) &&
        nodes.type(node) == ValueType::float4) {
        return TEXT("RGBA");
    }
    return {};
}

auto connect(ExpressionValue const& from,
             UMaterialExpression* const to,
             TCHAR const* const input_name) -> bool {
    return UMaterialEditingLibrary::ConnectMaterialExpressions(
        from.expression, from.output_name, to, input_name);
}

auto is_current(UMaterial& material, FString const& source_filename, FString const& source_hash)
    -> bool {
    auto& metadata{material.GetOutermost()->GetMetaData()};
    return FString{metadata.GetValue(&material, ownership_key)} == generator_owner &&
           FString{metadata.GetValue(&material, source_key)} == source_filename &&
           FString{metadata.GetValue(&material, source_hash_key)} == source_hash &&
           FString{metadata.GetValue(&material, version_key)} == generator_version;
}

auto is_owned(UMaterial& material) -> bool {
    auto& metadata{material.GetOutermost()->GetMetaData()};
    auto const owner{FString{metadata.GetValue(&material, ownership_key)}};
    auto const version{FString{metadata.GetValue(&material, version_key)}};
    return owner == generator_owner || (owner == TEXT("1") && version == TEXT("1"));
}

}

auto emit(MaterialIR const& ir, FString const& source_filename, FString const& source_hash)
    -> EmitResult {
    EmitResult result;
    for (auto const& diagnostic : validate(ir)) {
        result.errors.Add(FString::Printf(TEXT("%s:%llu:%llu: %s"),
                                          UTF8_TO_TCHAR(diagnostic.path.c_str()),
                                          diagnostic.line,
                                          diagnostic.column,
                                          UTF8_TO_TCHAR(diagnostic.message.c_str())));
    }
    if (!result.errors.IsEmpty()) {
        return result;
    }

    for (auto const& dependency : ir.texture_dependencies) {
        auto* const texture{
            LoadObject<UTexture>(nullptr, *text(ir, dependency), nullptr, LOAD_NoWarn)};
        if (texture == nullptr) {
            result.errors.Add(FString::Printf(
                TEXT("Texture dependency disappeared before emission: %s"), *text(ir, dependency)));
        }
    }
    if (!result.errors.IsEmpty()) {
        return result;
    }

    auto const desired_object_path{object_path(ir)};
    auto* existing{LoadObject<UObject>(nullptr, *desired_object_path, nullptr, LOAD_NoWarn)};
    auto* material{Cast<UMaterial>(existing)};
    if (existing != nullptr && material == nullptr) {
        result.errors.Add(TEXT("Refusing to replace an existing non-material object."));
        return result;
    }
    if (material != nullptr && !ir.settings.adopt_existing && !is_owned(*material)) {
        result.errors.Add(TEXT("Refusing to modify a material not owned by MaterialSynth."));
        return result;
    }
    if (material != nullptr && is_current(*material, source_filename, source_hash)) {
        result.material = material;
        return result;
    }

    auto* const package{material != nullptr ? material->GetOutermost()
                                            : CreatePackage(*text(ir, ir.settings.package_path))};
    bool const is_new{material == nullptr};
    if (material == nullptr) {
        material = NewObject<UMaterial>(package,
                                        FName{*text(ir, ir.settings.name)},
                                        RF_Public | RF_Standalone | RF_Transactional);
    }
    if (material == nullptr) {
        result.errors.Add(TEXT("Failed to create material."));
        return result;
    }

    GShaderCompilingManager->FinishAllCompilation();

    material->Modify();
    TArray<UMaterialExpression*> const previous_expressions{material->GetExpressions()};
    for (auto* const expression : previous_expressions) {
        if (expression->IsRooted()) {
            expression->RemoveFromRoot();
        }
        UMaterialEditingLibrary::DeleteMaterialExpression(material, expression);
    }
    material->MaterialDomain = ir.settings.domain == MaterialDomain::ui      ? MD_UI
                             : ir.settings.domain == MaterialDomain::surface ? MD_Surface
                                                                             : MD_PostProcess;
    material->BlendMode = ir.settings.blend_mode == BlendMode::additive    ? BLEND_Additive
                        : ir.settings.blend_mode == BlendMode::translucent ? BLEND_Translucent
                        : ir.settings.blend_mode == BlendMode::opaque      ? BLEND_Opaque
                        : ir.settings.blend_mode == BlendMode::masked      ? BLEND_Masked
                                                                           : BLEND_AlphaComposite;
    material->OpacityMaskClipValue = static_cast<float>(ir.settings.opacity_mask_clip_value);
    material->SetShadingModel(ir.settings.shading_model == ShadingModel::unlit ? MSM_Unlit
                                                                               : MSM_DefaultLit);
    material->TwoSided = ir.settings.two_sided;
    material->bDisableDepthTest = ir.settings.disable_depth_test;
    material->SetUsageByFlag(MATUSAGE_InstancedStaticMeshes,
                             ir.settings.used_with_instanced_static_meshes);
    material->SetUsageByFlag(MATUSAGE_ParticleSprites, ir.settings.used_with_particle_sprites);

    TArray<ExpressionValue> expressions;
    expressions.Reserve(static_cast<int32>(ir.nodes.size()));
    std::size_t expression_index{};
    auto create_expression{
        [&](UClass* const type) { return make_expression(*material, type, expression_index++); }};
    auto connect_input{[&](ExpressionValue const& from,
                           UMaterialExpression* const to,
                           TCHAR const* const input_name,
                           std::size_t const node_index) {
        if (!connect(from, to, input_name)) {
            result.errors.Add(
                FString::Printf(TEXT("Failed to connect input %s for material IR node %llu."),
                                input_name,
                                node_index));
        }
    }};

    auto const node_count{ir.nodes.size()};
    for (std::size_t index{}; index < node_count; ++index) {
        auto const node{NodeHandle{static_cast<std::uint32_t>(index)}};
        UMaterialExpression* expression{};
        switch (ir.nodes.kind(node)) {
            case NodeKind::constant:
                if (ir.nodes.type(node) == ValueType::float1) {
                    auto* const constant{CastChecked<UMaterialExpressionConstant>(
                        create_expression(UMaterialExpressionConstant::StaticClass()))};
                    constant->R = static_cast<float>(ir.nodes.constant(node, 0));
                    expression = constant;
                } else if (ir.nodes.type(node) == ValueType::float2) {
                    auto* const constant{CastChecked<UMaterialExpressionConstant2Vector>(
                        create_expression(UMaterialExpressionConstant2Vector::StaticClass()))};
                    constant->R = static_cast<float>(ir.nodes.constant(node, 0));
                    constant->G = static_cast<float>(ir.nodes.constant(node, 1));
                    expression = constant;
                } else if (ir.nodes.type(node) == ValueType::float3) {
                    auto* const constant{CastChecked<UMaterialExpressionConstant3Vector>(
                        create_expression(UMaterialExpressionConstant3Vector::StaticClass()))};
                    constant->Constant =
                        FLinearColor{static_cast<float>(ir.nodes.constant(node, 0)),
                                     static_cast<float>(ir.nodes.constant(node, 1)),
                                     static_cast<float>(ir.nodes.constant(node, 2)),
                                     0.0f};
                    expression = constant;
                } else {
                    auto* const constant{CastChecked<UMaterialExpressionConstant4Vector>(
                        create_expression(UMaterialExpressionConstant4Vector::StaticClass()))};
                    constant->Constant =
                        FLinearColor{static_cast<float>(ir.nodes.constant(node, 0)),
                                     static_cast<float>(ir.nodes.constant(node, 1)),
                                     static_cast<float>(ir.nodes.constant(node, 2)),
                                     static_cast<float>(ir.nodes.constant(node, 3))};
                    expression = constant;
                }
                break;
            case NodeKind::parameter: {
                auto const parameter{ir.nodes.parameter(node)};
                auto const name{FName{*text(ir, ir.parameters.name[parameter.value])}};
                if (ir.parameters.type[parameter.value] == ValueType::texture) {
                    auto* const value{
                        CastChecked<UMaterialExpressionTextureObjectParameter>(create_expression(
                            UMaterialExpressionTextureObjectParameter::StaticClass()))};
                    value->ParameterName = name;
                    value->Texture =
                        LoadObject<UTexture>(nullptr,
                                             *text(ir, ir.parameters.texture_path[parameter.value]),
                                             nullptr,
                                             LOAD_NoWarn);
                    value->SamplerType =
                        sampler_type(ir.parameters.texture_sampler_type[parameter.value]);
                    expression = value;
                } else if (ir.parameters.type[parameter.value] == ValueType::float1) {
                    auto* const value{CastChecked<UMaterialExpressionScalarParameter>(
                        create_expression(UMaterialExpressionScalarParameter::StaticClass()))};
                    value->ParameterName = name;
                    value->DefaultValue =
                        static_cast<float>(ir.parameters.default_values[0][parameter.value]);
                    expression = value;
                } else {
                    auto* const value{CastChecked<UMaterialExpressionVectorParameter>(
                        create_expression(UMaterialExpressionVectorParameter::StaticClass()))};
                    value->ParameterName = name;
                    value->DefaultValue = FLinearColor{
                        static_cast<float>(ir.parameters.default_values[0][parameter.value]),
                        static_cast<float>(ir.parameters.default_values[1][parameter.value]),
                        static_cast<float>(ir.parameters.default_values[2][parameter.value]),
                        static_cast<float>(ir.parameters.default_values[3][parameter.value])};
                    expression = value;
                }
                break;
            }
            case NodeKind::texture_coordinate: {
                auto* const coordinate{CastChecked<UMaterialExpressionTextureCoordinate>(
                    create_expression(UMaterialExpressionTextureCoordinate::StaticClass()))};
                coordinate->CoordinateIndex = ir.nodes.coordinate_index(node);
                expression = coordinate;
                break;
            }
            case NodeKind::per_instance_custom_data: {
                auto* const custom_data{CastChecked<UMaterialExpressionPerInstanceCustomData>(
                    create_expression(UMaterialExpressionPerInstanceCustomData::StaticClass()))};
                custom_data->DataIndex = static_cast<int32>(ir.nodes.instance_data_index(node));
                expression = custom_data;
                break;
            }
            case NodeKind::add:
            case NodeKind::multiply: {
                auto value{expressions[ir.nodes.inputs(node)[0].value]};
                for (std::size_t input_index{1}; input_index < ir.nodes.inputs(node).size();
                     ++input_index) {
                    auto* const operation{
                        create_expression(ir.nodes.kind(node) == NodeKind::add
                                              ? UMaterialExpressionAdd::StaticClass()
                                              : UMaterialExpressionMultiply::StaticClass())};
                    connect_input(value, operation, TEXT("A"), index);
                    connect_input(expressions[ir.nodes.inputs(node)[input_index].value],
                                  operation,
                                  TEXT("B"),
                                  index);
                    value = {operation, {}};
                }
                expression = value.expression;
                break;
            }
            case NodeKind::subtract:
                expression = create_expression(UMaterialExpressionSubtract::StaticClass());
                break;
            case NodeKind::divide:
                expression = create_expression(UMaterialExpressionDivide::StaticClass());
                break;
            case NodeKind::lerp:
                expression = create_expression(UMaterialExpressionLinearInterpolate::StaticClass());
                break;
            case NodeKind::saturate:
                expression = create_expression(UMaterialExpressionSaturate::StaticClass());
                break;
            case NodeKind::step: {
                auto* const step{CastChecked<UMaterialExpressionIf>(
                    create_expression(UMaterialExpressionIf::StaticClass()))};
                auto* const one{CastChecked<UMaterialExpressionConstant>(
                    create_expression(UMaterialExpressionConstant::StaticClass()))};
                auto* const zero{CastChecked<UMaterialExpressionConstant>(
                    create_expression(UMaterialExpressionConstant::StaticClass()))};
                one->R = 1.0f;
                zero->R = 0.0f;
                step->A.Connect(0, expressions[ir.nodes.inputs(node)[1].value].expression);
                step->B.Connect(0, expressions[ir.nodes.inputs(node)[0].value].expression);
                step->AGreaterThanB.Connect(0, one);
                step->AEqualsB.Connect(0, one);
                step->ALessThanB.Connect(0, zero);
                expression = step;
                break;
            }
            case NodeKind::sample: {
                auto* const sample{CastChecked<UMaterialExpressionTextureSample>(
                    create_expression(UMaterialExpressionTextureSample::StaticClass()))};
                sample->SamplerType = sampler_type(ir.nodes.texture_sampler_type(node));
                expression = sample;
                break;
            }
            case NodeKind::custom: {
                auto* const custom{CastChecked<UMaterialExpressionCustom>(
                    create_expression(UMaterialExpressionCustom::StaticClass()))};
                custom->OutputType = custom_output_type(ir.nodes.type(node));
                custom->Description = *text(ir, ir.nodes.description(node));
                custom->Code = *text(ir, ir.nodes.code(node));
                custom->Inputs.Empty();
                for (auto const& input : ir.nodes.custom_inputs(node)) {
                    auto& custom_input{custom->Inputs.AddDefaulted_GetRef()};
                    custom_input.InputName =
                        FName{*text(ir, ir.nodes.input_declarations.name[input.value])};
                }
                expression = custom;
                break;
            }
            case NodeKind::texture_object: {
                auto* const texture{CastChecked<UMaterialExpressionTextureObject>(
                    create_expression(UMaterialExpressionTextureObject::StaticClass()))};
                texture->Texture = LoadObject<UTexture>(
                    nullptr, *text(ir, ir.nodes.texture_path(node)), nullptr, LOAD_NoWarn);
                texture->SamplerType = sampler_type(texture->Texture);
                expression = texture;
                break;
            }
            case NodeKind::component_mask: {
                auto* const mask{CastChecked<UMaterialExpressionComponentMask>(
                    create_expression(UMaterialExpressionComponentMask::StaticClass()))};
                mask->R = ir.text(ir.nodes.component_mask(node)).find('r') != std::string::npos;
                mask->G = ir.text(ir.nodes.component_mask(node)).find('g') != std::string::npos;
                mask->B = ir.text(ir.nodes.component_mask(node)).find('b') != std::string::npos;
                mask->A = ir.text(ir.nodes.component_mask(node)).find('a') != std::string::npos;
                expression = mask;
                break;
            }
            case NodeKind::world_position:
                expression = create_expression(UMaterialExpressionWorldPosition::StaticClass());
                break;
            case NodeKind::object_position:
                expression = create_expression(UMaterialExpressionObjectPositionWS::StaticClass());
                break;
            case NodeKind::pixel_normal:
                expression = create_expression(UMaterialExpressionPixelNormalWS::StaticClass());
                break;
            case NodeKind::vertex_normal:
                expression = create_expression(UMaterialExpressionVertexNormalWS::StaticClass());
                break;
            case NodeKind::camera_vector:
                expression = create_expression(UMaterialExpressionCameraVectorWS::StaticClass());
                break;
            case NodeKind::camera_position:
                expression = create_expression(UMaterialExpressionCameraPositionWS::StaticClass());
                break;
            case NodeKind::vertex_color:
                expression = create_expression(UMaterialExpressionVertexColor::StaticClass());
                break;
            case NodeKind::transform_position: {
                auto* const transform{CastChecked<UMaterialExpressionTransformPosition>(
                    create_expression(UMaterialExpressionTransformPosition::StaticClass()))};
                transform->TransformSourceType = position_space(ir.nodes.source_space(node));
                transform->TransformType = position_space(ir.nodes.destination_space(node));
                expression = transform;
                break;
            }
            case NodeKind::scene_texture: {
                auto* const scene_texture{CastChecked<UMaterialExpressionSceneTexture>(
                    create_expression(UMaterialExpressionSceneTexture::StaticClass()))};
                scene_texture->SceneTextureId =
                    ir.nodes.scene_texture(node) == SceneTexture::scene_depth
                        ? PPI_SceneDepth
                        : PPI_PostProcessInput0;
                expression = scene_texture;
                break;
            }
            case NodeKind::shader_call: {
                auto* const custom{CastChecked<UMaterialExpressionCustom>(
                    create_expression(UMaterialExpressionCustom::StaticClass()))};
                custom->OutputType = custom_output_type(ir.nodes.type(node));
                custom->Description = *text(ir, ir.nodes.shader_function(node));
                custom->IncludeFilePaths.Add(*text(ir, ir.nodes.shader_path(node)));
                custom->Inputs.Empty();

                FString arguments;
                for (auto const& input : ir.nodes.custom_inputs(node)) {
                    auto& custom_input{custom->Inputs.AddDefaulted_GetRef()};
                    custom_input.InputName =
                        FName{*text(ir, ir.nodes.input_declarations.name[input.value])};
                    if (!arguments.IsEmpty()) {
                        arguments += TEXT(", ");
                    }
                    arguments += custom_input.InputName.ToString();
                }
                custom->Code = FString::Printf(
                    TEXT("return %s(%s);"), *text(ir, ir.nodes.shader_function(node)), *arguments);
                expression = custom;
                break;
            }
            case NodeKind::vector_constructor: {
                auto value{expressions[ir.nodes.inputs(node)[0].value]};
                for (std::size_t input_index{1}; input_index < ir.nodes.inputs(node).size();
                     ++input_index) {
                    auto* const append{
                        create_expression(UMaterialExpressionAppendVector::StaticClass())};
                    connect_input(value, append, TEXT("A"), index);
                    connect_input(expressions[ir.nodes.inputs(node)[input_index].value],
                                  append,
                                  TEXT("B"),
                                  index);
                    value = {append, {}};
                }
                expression = value.expression;
                break;
            }
            case NodeKind::time: {
                auto* const time{CastChecked<UMaterialExpressionTime>(
                    create_expression(UMaterialExpressionTime::StaticClass()))};
                time->bIgnorePause = false;
                time->bOverride_Period = false;
                expression = time;
                break;
            }
            case NodeKind::sine: {
                auto* const sine{CastChecked<UMaterialExpressionSine>(
                    create_expression(UMaterialExpressionSine::StaticClass()))};
                sine->Period = 2.0f * UE_PI;
                expression = sine;
                break;
            }
            case NodeKind::cosine: {
                auto* const cosine{CastChecked<UMaterialExpressionCosine>(
                    create_expression(UMaterialExpressionCosine::StaticClass()))};
                cosine->Period = 2.0f * UE_PI;
                expression = cosine;
                break;
            }
        }
        if (expression == nullptr) {
            result.errors.Add(FString::Printf(
                TEXT("Failed to create material expression for IR node %llu."), index));
            return result;
        }
        expressions.Add({expression, output_name(ir.nodes, node)});

        static constexpr TCHAR const* binary_inputs[]{TEXT("A"), TEXT("B")};
        if (ir.nodes.kind(node) == NodeKind::subtract || ir.nodes.kind(node) == NodeKind::divide) {
            connect_input(
                expressions[ir.nodes.inputs(node)[0].value], expression, binary_inputs[0], index);
            connect_input(
                expressions[ir.nodes.inputs(node)[1].value], expression, binary_inputs[1], index);
        } else if (ir.nodes.kind(node) == NodeKind::lerp) {
            connect_input(
                expressions[ir.nodes.inputs(node)[0].value], expression, TEXT("A"), index);
            connect_input(
                expressions[ir.nodes.inputs(node)[1].value], expression, TEXT("B"), index);
            connect_input(
                expressions[ir.nodes.inputs(node)[2].value], expression, TEXT("Alpha"), index);
        } else if (ir.nodes.kind(node) == NodeKind::saturate) {
            connect_input(expressions[ir.nodes.inputs(node)[0].value], expression, TEXT(""), index);
        } else if (ir.nodes.kind(node) == NodeKind::sample) {
            connect_input(
                expressions[ir.nodes.inputs(node)[0].value], expression, TEXT("Tex"), index);
            connect_input(
                expressions[ir.nodes.inputs(node)[1].value], expression, TEXT("UVs"), index);
        } else if (ir.nodes.kind(node) == NodeKind::custom) {
            auto* const custom{CastChecked<UMaterialExpressionCustom>(expression)};
            for (std::size_t input_index{}; input_index < ir.nodes.custom_inputs(node).size();
                 ++input_index) {
                auto const input_name{
                    FString{*text(ir,
                                  ir.nodes.input_declarations
                                      .name[ir.nodes.custom_inputs(node)[input_index].value])}};
                connect_input(expressions[ir.nodes.inputs(node)[input_index].value],
                              custom,
                              *input_name,
                              index);
            }
        } else if (ir.nodes.kind(node) == NodeKind::shader_call) {
            auto* const custom{CastChecked<UMaterialExpressionCustom>(expression)};
            for (std::size_t input_index{}; input_index < ir.nodes.custom_inputs(node).size();
                 ++input_index) {
                auto const& input{ir.nodes.custom_inputs(node)[input_index]};
                auto const input_name{
                    FString{*text(ir, ir.nodes.input_declarations.name[input.value])}};
                connect_input(expressions[ir.nodes.inputs(node)[input_index].value],
                              custom,
                              *input_name,
                              index);
            }
        } else if (ir.nodes.kind(node) == NodeKind::component_mask ||
                   ir.nodes.kind(node) == NodeKind::transform_position) {
            connect_input(expressions[ir.nodes.inputs(node)[0].value], expression, TEXT(""), index);
        } else if (ir.nodes.kind(node) == NodeKind::sine ||
                   ir.nodes.kind(node) == NodeKind::cosine) {
            connect_input(expressions[ir.nodes.inputs(node)[0].value], expression, TEXT(""), index);
        }
    }

    if (!result.errors.IsEmpty()) {
        return result;
    }

    for (auto const output : ir.outputs.handles()) {
        bool connected{};
        if (ir.text(ir.outputs.name[output.value]) == "emissive") {
            connected = UMaterialEditingLibrary::ConnectMaterialProperty(
                expressions[ir.outputs.node[output.value].value].expression,
                expressions[ir.outputs.node[output.value].value].output_name,
                MP_EmissiveColor);
        } else if (ir.text(ir.outputs.name[output.value]) == "opacity") {
            connected = UMaterialEditingLibrary::ConnectMaterialProperty(
                expressions[ir.outputs.node[output.value].value].expression,
                expressions[ir.outputs.node[output.value].value].output_name,
                MP_Opacity);
        } else if (ir.text(ir.outputs.name[output.value]) == "base-color") {
            connected = UMaterialEditingLibrary::ConnectMaterialProperty(
                expressions[ir.outputs.node[output.value].value].expression,
                expressions[ir.outputs.node[output.value].value].output_name,
                MP_BaseColor);
        } else if (ir.text(ir.outputs.name[output.value]) == "opacity-mask") {
            connected = UMaterialEditingLibrary::ConnectMaterialProperty(
                expressions[ir.outputs.node[output.value].value].expression,
                expressions[ir.outputs.node[output.value].value].output_name,
                MP_OpacityMask);
        } else if (ir.text(ir.outputs.name[output.value]) == "world-position-offset") {
            connected = UMaterialEditingLibrary::ConnectMaterialProperty(
                expressions[ir.outputs.node[output.value].value].expression,
                expressions[ir.outputs.node[output.value].value].output_name,
                MP_WorldPositionOffset);
        }
        if (!connected) {
            result.errors.Add(FString::Printf(TEXT("Failed to connect material output %s."),
                                              *text(ir, ir.outputs.name[output.value])));
        }
    }
    if (!result.errors.IsEmpty()) {
        return result;
    }

    package->GetMetaData().SetValue(material, ownership_key, generator_owner);
    package->GetMetaData().SetValue(material, source_key, *source_filename);
    package->GetMetaData().SetValue(material, source_hash_key, *source_hash);
    package->GetMetaData().SetValue(material, version_key, generator_version);
    result.errors = UMaterialEditingLibrary::RecompileMaterial(material);
    GShaderCompilingManager->FinishAllCompilation();
    if (!result.errors.IsEmpty()) {
        return result;
    }

    package->MarkPackageDirty();
    if (is_new) {
        FAssetRegistryModule::AssetCreated(material);
    }
    FSavePackageArgs save_args{};
    save_args.TopLevelFlags = RF_Public | RF_Standalone;
    auto const filename{FPackageName::LongPackageNameToFilename(
        package->GetName(), FPackageName::GetAssetPackageExtension())};
    if (!UPackage::SavePackage(package, material, *filename, save_args)) {
        result.errors.Add(TEXT("Failed to save generated material package."));
        return result;
    }
    result.material = material;
    result.changed = true;
    return result;
}

}
