#include "SandboxEditor/material/MaterialEmitter.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/Texture.h"
#include "MaterialDomain.h"
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant2Vector.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionConstant4Vector.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionDivide.h"
#include "Materials/MaterialExpressionLinearInterpolate.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionPerInstanceCustomData.h"
#include "Materials/MaterialExpressionSaturate.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionSubtract.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionTextureObjectParameter.h"
#include "Materials/MaterialExpressionTextureSample.h"
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

auto connect(UMaterialExpression* const from,
             UMaterialExpression* const to,
             TCHAR const* const input_name) -> bool {
    return UMaterialEditingLibrary::ConnectMaterialExpressions(from, TEXT(""), to, input_name);
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

    TArray<UMaterialExpression*> expressions;
    expressions.Reserve(static_cast<int32>(ir.nodes.size()));
    auto const node_count{ir.nodes.size()};
    for (std::size_t index{}; index < node_count; ++index) {
        auto const& node{ir.nodes[index]};
        UMaterialExpression* expression{};
        switch (node.kind) {
            case NodeKind::constant:
                if (node.type == ValueType::float1) {
                    auto* const constant{CastChecked<UMaterialExpressionConstant>(make_expression(
                        *material, UMaterialExpressionConstant::StaticClass(), index))};
                    constant->R = static_cast<float>(node.constant[0]);
                    expression = constant;
                } else if (node.type == ValueType::float2) {
                    auto* const constant{
                        CastChecked<UMaterialExpressionConstant2Vector>(make_expression(
                            *material, UMaterialExpressionConstant2Vector::StaticClass(), index))};
                    constant->R = static_cast<float>(node.constant[0]);
                    constant->G = static_cast<float>(node.constant[1]);
                    expression = constant;
                } else if (node.type == ValueType::float3) {
                    auto* const constant{
                        CastChecked<UMaterialExpressionConstant3Vector>(make_expression(
                            *material, UMaterialExpressionConstant3Vector::StaticClass(), index))};
                    constant->Constant = FLinearColor{static_cast<float>(node.constant[0]),
                                                      static_cast<float>(node.constant[1]),
                                                      static_cast<float>(node.constant[2]),
                                                      0.0f};
                    expression = constant;
                } else {
                    auto* const constant{
                        CastChecked<UMaterialExpressionConstant4Vector>(make_expression(
                            *material, UMaterialExpressionConstant4Vector::StaticClass(), index))};
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
                    auto* const value{CastChecked<UMaterialExpressionTextureObjectParameter>(
                        make_expression(*material,
                                        UMaterialExpressionTextureObjectParameter::StaticClass(),
                                        index))};
                    value->ParameterName = name;
                    value->Texture =
                        LoadObject<UTexture>(nullptr,
                                             UTF8_TO_TCHAR(parameter.texture_path.c_str()),
                                             nullptr,
                                             LOAD_NoWarn);
                    value->SamplerType = SAMPLERTYPE_LinearColor;
                    expression = value;
                } else if (parameter.type == ValueType::float1) {
                    auto* const value{
                        CastChecked<UMaterialExpressionScalarParameter>(make_expression(
                            *material, UMaterialExpressionScalarParameter::StaticClass(), index))};
                    value->ParameterName = name;
                    value->DefaultValue = static_cast<float>(parameter.default_value[0]);
                    expression = value;
                } else {
                    auto* const value{
                        CastChecked<UMaterialExpressionVectorParameter>(make_expression(
                            *material, UMaterialExpressionVectorParameter::StaticClass(), index))};
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
                auto* const coordinate{
                    CastChecked<UMaterialExpressionTextureCoordinate>(make_expression(
                        *material, UMaterialExpressionTextureCoordinate::StaticClass(), index))};
                coordinate->CoordinateIndex = node.coordinate_index;
                expression = coordinate;
                break;
            }
            case NodeKind::per_instance_custom_data: {
                auto* const custom_data{CastChecked<UMaterialExpressionPerInstanceCustomData>(
                    make_expression(*material,
                                    UMaterialExpressionPerInstanceCustomData::StaticClass(),
                                    index))};
                custom_data->DataIndex = static_cast<int32>(node.instance_data_index);
                expression = custom_data;
                break;
            }
            case NodeKind::add:
                expression =
                    make_expression(*material, UMaterialExpressionAdd::StaticClass(), index);
                break;
            case NodeKind::subtract:
                expression =
                    make_expression(*material, UMaterialExpressionSubtract::StaticClass(), index);
                break;
            case NodeKind::multiply:
                expression =
                    make_expression(*material, UMaterialExpressionMultiply::StaticClass(), index);
                break;
            case NodeKind::divide:
                expression =
                    make_expression(*material, UMaterialExpressionDivide::StaticClass(), index);
                break;
            case NodeKind::lerp:
                expression = make_expression(
                    *material, UMaterialExpressionLinearInterpolate::StaticClass(), index);
                break;
            case NodeKind::saturate:
                expression =
                    make_expression(*material, UMaterialExpressionSaturate::StaticClass(), index);
                break;
            case NodeKind::sample: {
                auto* const sample{CastChecked<UMaterialExpressionTextureSample>(make_expression(
                    *material, UMaterialExpressionTextureSample::StaticClass(), index))};
                sample->SamplerType = SAMPLERTYPE_LinearColor;
                expression = sample;
                break;
            }
            case NodeKind::custom: {
                auto* const custom{CastChecked<UMaterialExpressionCustom>(
                    make_expression(*material, UMaterialExpressionCustom::StaticClass(), index))};
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
        }
        expressions.Add(expression);

        static constexpr TCHAR const* binary_inputs[]{TEXT("A"), TEXT("B")};
        if (node.kind >= NodeKind::add && node.kind <= NodeKind::divide) {
            connect(expressions[node.inputs[0].index], expression, binary_inputs[0]);
            connect(expressions[node.inputs[1].index], expression, binary_inputs[1]);
        } else if (node.kind == NodeKind::lerp) {
            connect(expressions[node.inputs[0].index], expression, TEXT("A"));
            connect(expressions[node.inputs[1].index], expression, TEXT("B"));
            connect(expressions[node.inputs[2].index], expression, TEXT("Alpha"));
        } else if (node.kind == NodeKind::saturate) {
            connect(expressions[node.inputs[0].index], expression, TEXT("Input"));
        } else if (node.kind == NodeKind::sample) {
            connect(expressions[node.inputs[0].index], expression, TEXT("TextureObject"));
            connect(expressions[node.inputs[1].index], expression, TEXT("Coordinates"));
        } else if (node.kind == NodeKind::custom) {
            auto* const custom{CastChecked<UMaterialExpressionCustom>(expression)};
            for (std::size_t input_index{}; input_index < node.custom_inputs.size();
                 ++input_index) {
                custom->Inputs[static_cast<int32>(input_index)].Input.Connect(
                    0, expressions[node.custom_inputs[input_index].node.index]);
            }
        }
    }

    for (auto const& output : ir.outputs) {
        if (output.name == "emissive") {
            UMaterialEditingLibrary::ConnectMaterialProperty(
                expressions[output.node.index], TEXT(""), MP_EmissiveColor);
        } else if (output.name == "opacity") {
            UMaterialEditingLibrary::ConnectMaterialProperty(
                expressions[output.node.index], TEXT(""), MP_Opacity);
        }
    }

    package->GetMetaData().SetValue(material, ownership_key, generator_version);
    package->GetMetaData().SetValue(material, source_key, *source_filename);
    package->GetMetaData().SetValue(material, source_hash_key, *source_hash);
    package->GetMetaData().SetValue(material, version_key, generator_version);
    material->PostEditChange();
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
