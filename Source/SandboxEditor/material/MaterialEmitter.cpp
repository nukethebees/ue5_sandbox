#include "SandboxEditor/material/MaterialEmitter.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/Texture.h"
#include "MaterialDomain.h"
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionAppendVector.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant2Vector.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionConstant4Vector.h"
#include "Materials/MaterialExpressionCosine.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionDivide.h"
#include "Materials/MaterialExpressionLinearInterpolate.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionPerInstanceCustomData.h"
#include "Materials/MaterialExpressionSaturate.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionSine.h"
#include "Materials/MaterialExpressionSubtract.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionTextureObjectParameter.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionTime.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Misc/PackageName.h"
#include "ShaderCompiler.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

namespace material_synth {
namespace {

auto object_path(MaterialSettings const& settings) -> FString {
    auto const package{FString{UTF8_TO_TCHAR(settings.package_path.c_str())}};
    auto const name{FString{UTF8_TO_TCHAR(settings.name.c_str())}};
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

struct ExpressionValue {
    UMaterialExpression* expression{};
    FString output_name;
};

auto output_name(Node const& node) -> FString {
    if ((node.kind == NodeKind::parameter || node.kind == NodeKind::sample) &&
        node.type == ValueType::float4) {
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
            LoadObject<UTexture>(nullptr, UTF8_TO_TCHAR(dependency.c_str()), nullptr, LOAD_NoWarn)};
        if (texture == nullptr) {
            result.errors.Add(
                FString::Printf(TEXT("Texture dependency disappeared before emission: %s"),
                                UTF8_TO_TCHAR(dependency.c_str())));
        }
    }
    if (!result.errors.IsEmpty()) {
        return result;
    }

    auto const desired_object_path{object_path(ir.settings)};
    auto* existing{LoadObject<UObject>(nullptr, *desired_object_path, nullptr, LOAD_NoWarn)};
    auto* material{Cast<UMaterial>(existing)};
    if (existing != nullptr && material == nullptr) {
        result.errors.Add(TEXT("Refusing to replace an existing non-material object."));
        return result;
    }
    if (material != nullptr && FString{material->GetOutermost()->GetMetaData().GetValue(
                                   material, ownership_key)} != generator_version) {
        result.errors.Add(TEXT("Refusing to modify a material not owned by MaterialSynth."));
        return result;
    }

    auto* const package{material != nullptr
                            ? material->GetOutermost()
                            : CreatePackage(UTF8_TO_TCHAR(ir.settings.package_path.c_str()))};
    bool const is_new{material == nullptr};
    if (material == nullptr) {
        material = NewObject<UMaterial>(package,
                                        FName{UTF8_TO_TCHAR(ir.settings.name.c_str())},
                                        RF_Public | RF_Standalone | RF_Transactional);
    }
    if (material == nullptr) {
        result.errors.Add(TEXT("Failed to create material."));
        return result;
    }

    material->Modify();
    TArray<UMaterialExpression*> const previous_expressions{material->GetExpressions()};
    for (auto* const expression : previous_expressions) {
        UMaterialEditingLibrary::DeleteMaterialExpression(material, expression);
    }
    material->MaterialDomain = ir.settings.domain == MaterialDomain::ui ? MD_UI : MD_Surface;
    material->BlendMode =
        ir.settings.blend_mode == BlendMode::additive ? BLEND_Additive : BLEND_Translucent;
    material->SetShadingModel(ir.settings.shading_model == ShadingModel::unlit ? MSM_Unlit
                                                                               : MSM_DefaultLit);
    material->TwoSided = ir.settings.two_sided;
    material->bDisableDepthTest = ir.settings.disable_depth_test;
    material->SetUsageByFlag(MATUSAGE_InstancedStaticMeshes,
                             ir.settings.used_with_instanced_static_meshes);

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
        auto const& node{ir.nodes[index]};
        UMaterialExpression* expression{};
        switch (node.kind) {
            case NodeKind::constant:
                if (node.type == ValueType::float1) {
                    auto* const constant{CastChecked<UMaterialExpressionConstant>(
                        create_expression(UMaterialExpressionConstant::StaticClass()))};
                    constant->R = static_cast<float>(node.constant[0]);
                    expression = constant;
                } else if (node.type == ValueType::float2) {
                    auto* const constant{CastChecked<UMaterialExpressionConstant2Vector>(
                        create_expression(UMaterialExpressionConstant2Vector::StaticClass()))};
                    constant->R = static_cast<float>(node.constant[0]);
                    constant->G = static_cast<float>(node.constant[1]);
                    expression = constant;
                } else if (node.type == ValueType::float3) {
                    auto* const constant{CastChecked<UMaterialExpressionConstant3Vector>(
                        create_expression(UMaterialExpressionConstant3Vector::StaticClass()))};
                    constant->Constant = FLinearColor{static_cast<float>(node.constant[0]),
                                                      static_cast<float>(node.constant[1]),
                                                      static_cast<float>(node.constant[2]),
                                                      0.0f};
                    expression = constant;
                } else {
                    auto* const constant{CastChecked<UMaterialExpressionConstant4Vector>(
                        create_expression(UMaterialExpressionConstant4Vector::StaticClass()))};
                    constant->Constant = FLinearColor{static_cast<float>(node.constant[0]),
                                                      static_cast<float>(node.constant[1]),
                                                      static_cast<float>(node.constant[2]),
                                                      static_cast<float>(node.constant[3])};
                    expression = constant;
                }
                break;
            case NodeKind::parameter: {
                auto const& parameter{ir.parameters[node.parameter_index]};
                auto const name{FName{UTF8_TO_TCHAR(parameter.name.c_str())}};
                if (parameter.type == ValueType::texture) {
                    auto* const value{
                        CastChecked<UMaterialExpressionTextureObjectParameter>(create_expression(
                            UMaterialExpressionTextureObjectParameter::StaticClass()))};
                    value->ParameterName = name;
                    value->Texture =
                        LoadObject<UTexture>(nullptr,
                                             UTF8_TO_TCHAR(parameter.texture_path.c_str()),
                                             nullptr,
                                             LOAD_NoWarn);
                    value->SamplerType = SAMPLERTYPE_LinearColor;
                    expression = value;
                } else if (parameter.type == ValueType::float1) {
                    auto* const value{CastChecked<UMaterialExpressionScalarParameter>(
                        create_expression(UMaterialExpressionScalarParameter::StaticClass()))};
                    value->ParameterName = name;
                    value->DefaultValue = static_cast<float>(parameter.default_value[0]);
                    expression = value;
                } else {
                    auto* const value{CastChecked<UMaterialExpressionVectorParameter>(
                        create_expression(UMaterialExpressionVectorParameter::StaticClass()))};
                    value->ParameterName = name;
                    value->DefaultValue =
                        FLinearColor{static_cast<float>(parameter.default_value[0]),
                                     static_cast<float>(parameter.default_value[1]),
                                     static_cast<float>(parameter.default_value[2]),
                                     static_cast<float>(parameter.default_value[3])};
                    expression = value;
                }
                break;
            }
            case NodeKind::texture_coordinate: {
                auto* const coordinate{CastChecked<UMaterialExpressionTextureCoordinate>(
                    create_expression(UMaterialExpressionTextureCoordinate::StaticClass()))};
                coordinate->CoordinateIndex = node.coordinate_index;
                expression = coordinate;
                break;
            }
            case NodeKind::per_instance_custom_data: {
                auto* const custom_data{CastChecked<UMaterialExpressionPerInstanceCustomData>(
                    create_expression(UMaterialExpressionPerInstanceCustomData::StaticClass()))};
                custom_data->DataIndex = static_cast<int32>(node.instance_data_index);
                expression = custom_data;
                break;
            }
            case NodeKind::add:
            case NodeKind::multiply: {
                auto value{expressions[node.inputs[0].index]};
                for (std::size_t input_index{1}; input_index < node.inputs.size(); ++input_index) {
                    auto* const operation{create_expression(
                        node.kind == NodeKind::add ? UMaterialExpressionAdd::StaticClass()
                                                   : UMaterialExpressionMultiply::StaticClass())};
                    connect_input(value, operation, TEXT("A"), index);
                    connect_input(
                        expressions[node.inputs[input_index].index], operation, TEXT("B"), index);
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
            case NodeKind::sample: {
                auto* const sample{CastChecked<UMaterialExpressionTextureSample>(
                    create_expression(UMaterialExpressionTextureSample::StaticClass()))};
                sample->SamplerType = SAMPLERTYPE_LinearColor;
                expression = sample;
                break;
            }
            case NodeKind::custom: {
                auto* const custom{CastChecked<UMaterialExpressionCustom>(
                    create_expression(UMaterialExpressionCustom::StaticClass()))};
                custom->OutputType = custom_output_type(node.type);
                custom->Description = UTF8_TO_TCHAR(node.description.c_str());
                custom->Code = UTF8_TO_TCHAR(node.code.c_str());
                custom->Inputs.Empty();
                for (auto const& input : node.custom_inputs) {
                    auto& custom_input{custom->Inputs.AddDefaulted_GetRef()};
                    custom_input.InputName = FName{UTF8_TO_TCHAR(input.name.c_str())};
                }
                expression = custom;
                break;
            }
            case NodeKind::vector_constructor: {
                auto value{expressions[node.inputs[0].index]};
                for (std::size_t input_index{1}; input_index < node.inputs.size(); ++input_index) {
                    auto* const append{
                        create_expression(UMaterialExpressionAppendVector::StaticClass())};
                    connect_input(value, append, TEXT("A"), index);
                    connect_input(
                        expressions[node.inputs[input_index].index], append, TEXT("B"), index);
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
        expressions.Add({expression, output_name(node)});

        static constexpr TCHAR const* binary_inputs[]{TEXT("A"), TEXT("B")};
        if (node.kind == NodeKind::subtract || node.kind == NodeKind::divide) {
            connect_input(expressions[node.inputs[0].index], expression, binary_inputs[0], index);
            connect_input(expressions[node.inputs[1].index], expression, binary_inputs[1], index);
        } else if (node.kind == NodeKind::lerp) {
            connect_input(expressions[node.inputs[0].index], expression, TEXT("A"), index);
            connect_input(expressions[node.inputs[1].index], expression, TEXT("B"), index);
            connect_input(expressions[node.inputs[2].index], expression, TEXT("Alpha"), index);
        } else if (node.kind == NodeKind::saturate) {
            connect_input(expressions[node.inputs[0].index], expression, TEXT(""), index);
        } else if (node.kind == NodeKind::sample) {
            connect_input(expressions[node.inputs[0].index], expression, TEXT("Tex"), index);
            connect_input(expressions[node.inputs[1].index], expression, TEXT("UVs"), index);
        } else if (node.kind == NodeKind::custom) {
            auto* const custom{CastChecked<UMaterialExpressionCustom>(expression)};
            for (std::size_t input_index{}; input_index < node.custom_inputs.size();
                 ++input_index) {
                auto const input_name{
                    FString{UTF8_TO_TCHAR(node.custom_inputs[input_index].name.c_str())}};
                connect_input(expressions[node.custom_inputs[input_index].node.index],
                              custom,
                              *input_name,
                              index);
            }
        } else if (node.kind == NodeKind::sine || node.kind == NodeKind::cosine) {
            connect_input(expressions[node.inputs[0].index], expression, TEXT(""), index);
        }
    }

    if (!result.errors.IsEmpty()) {
        return result;
    }

    for (auto const& output : ir.outputs) {
        bool connected{};
        if (output.name == "emissive") {
            connected = UMaterialEditingLibrary::ConnectMaterialProperty(
                expressions[output.node.index].expression,
                expressions[output.node.index].output_name,
                MP_EmissiveColor);
        } else if (output.name == "opacity") {
            connected = UMaterialEditingLibrary::ConnectMaterialProperty(
                expressions[output.node.index].expression,
                expressions[output.node.index].output_name,
                MP_Opacity);
        }
        if (!connected) {
            result.errors.Add(FString::Printf(TEXT("Failed to connect material output %s."),
                                              UTF8_TO_TCHAR(output.name.c_str())));
        }
    }
    if (!result.errors.IsEmpty()) {
        return result;
    }

    package->GetMetaData().SetValue(material, ownership_key, generator_version);
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
    return result;
}

}
