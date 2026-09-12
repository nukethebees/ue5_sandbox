#include "MaterialExpressionUSFLoader.h"

#include <type_traits>

#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant2Vector.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionConstant4Vector.h"
#include "Materials/MaterialExpressionCustom.h"
#include "SGraphNodeMaterialUSFLoader.h"
#include "USFPathValidation.h"

#define LOCTEXT_NAMESPACE "MaterialExpressionUSFLoader"

UMaterialExpressionUSFLoader::UMaterialExpressionUSFLoader() {
    bShaderInputData = false;
    bCollapsed = false;
    bHidePreviewWindow = true;
    instance_name = TEXT("USF Loader");
}

namespace usf_loader::material_expression {

template <typename ExprT>
void set_input(UMaterialExpressionUSFLoader* loader,
               FExpressionInput& input_to_use,
               FUSFLoaderDefaults const& defaults) {
    auto* const expression{NewObject<ExprT>(loader)};

    if constexpr (std::is_same_v<ExprT, UMaterialExpressionConstant>) {
        expression->R = defaults.float1_value;
    } else if constexpr (std::is_same_v<ExprT, UMaterialExpressionConstant2Vector>) {
        expression->R = defaults.float2_value.X;
        expression->G = defaults.float2_value.Y;
    } else if constexpr (std::is_same_v<ExprT, UMaterialExpressionConstant3Vector>) {
        expression->Constant = FLinearColor(
            defaults.float3_value.X, defaults.float3_value.Y, defaults.float3_value.Z, 1.0f);
    } else if constexpr (std::is_same_v<ExprT, UMaterialExpressionConstant4Vector>) {
        expression->Constant = defaults.float4_value;
    } else {
        static_assert(!sizeof(ExprT), "Unhandled branch.");
    }

    input_to_use.Expression = expression;
    input_to_use.OutputIndex = 0;
}

auto make_debug_code(TConstArrayView<FString> const include_paths) -> FString {
    FString result;
    for (auto const& include_path : include_paths) {
        result += FString::Printf(TEXT("#include \"%s\"\n"), *include_path);
    }
    if (!result.IsEmpty()) {
        result += TEXT("\n");
    }
    result += TEXT("return previous_block;");
    return result;
}

}

#if WITH_EDITOR
int32 UMaterialExpressionUSFLoader::Compile(FMaterialCompiler* compiler, int32 output_index) {
    if (compiler == nullptr) {
        return INDEX_NONE;
    }

    static auto const input_name{FName(TEXT("previous_block"))};
    previous_block.InputName = input_name;

    FExpressionInput input_to_use{previous_block};
    if (!previous_block.IsConnected()) {
        switch (output_type) {
            using enum ECustomMaterialOutputType;
            case CMOT_Float1: {
                usf_loader::material_expression::set_input<UMaterialExpressionConstant>(
                    this, input_to_use, default_values);
                break;
            }
            case CMOT_Float2: {
                usf_loader::material_expression::set_input<UMaterialExpressionConstant2Vector>(
                    this, input_to_use, default_values);
                break;
            }
            case CMOT_Float3: {
                usf_loader::material_expression::set_input<UMaterialExpressionConstant3Vector>(
                    this, input_to_use, default_values);
                break;
            }
            case CMOT_Float4: {
                usf_loader::material_expression::set_input<UMaterialExpressionConstant4Vector>(
                    this, input_to_use, default_values);
                break;
            }
            default: {
                usf_loader::material_expression::set_input<UMaterialExpressionConstant>(
                    this, input_to_use, default_values);
                break;
            }
        }
    }

    auto include_paths{usf_loader::resolve_include_paths(path_prefix, usf_file_paths)};
    if (!include_paths) {
        debug_code = FString::Printf(TEXT("// %s"), *include_paths.error());
        return compiler->Errorf(TEXT("USF Loader: %s"), *include_paths.error());
    }

    TArray<FCustomInput> named_inputs;
    named_inputs.Emplace(input_to_use.InputName, input_to_use);

    auto* custom_expression{NewObject<UMaterialExpressionCustom>(this)};
    custom_expression->Code = TEXT("return previous_block;");
    custom_expression->OutputType = output_type;
    custom_expression->Inputs = MoveTemp(named_inputs);
    custom_expression->IncludeFilePaths = *include_paths;

    debug_code = usf_loader::material_expression::make_debug_code(*include_paths);

    return custom_expression->Compile(compiler, output_index);
}
void UMaterialExpressionUSFLoader::GetCaption(TArray<FString>& out_captions) const {
    if (usf_file_paths.Num() == 0) {
        out_captions.Add(TEXT("USF Loader: <No files>"));
    } else if (usf_file_paths.Num() == 1) {
        FString const display_path{usf_file_paths[0].Len() > 30
                                       ? usf_file_paths[0].Right(27) + TEXT("...")
                                       : usf_file_paths[0]};
        out_captions.Add(FString::Printf(TEXT("USF Loader: %s"), *display_path));
    } else {
        out_captions.Add(FString::Printf(TEXT("USF Loader: %d files"), usf_file_paths.Num()));
    }
}
FText UMaterialExpressionUSFLoader::GetCreationDescription() const {
    return LOCTEXT("CreationDescription",
                   "Loads USF files and makes their functions available to subsequent material "
                   "nodes");
}
FText UMaterialExpressionUSFLoader::GetCreationName() const {
    return LOCTEXT("CreationName", "USF Loader");
}
bool UMaterialExpressionUSFLoader::CanRenameNode() const {
    return true;
}
FString UMaterialExpressionUSFLoader::GetEditableName() const {
    return instance_name;
}
void UMaterialExpressionUSFLoader::SetEditableName(FString const& NewName) {
    instance_name = NewName;
}
void
    UMaterialExpressionUSFLoader::GetIncludeFilePaths(TSet<FString>& out_include_file_paths) const {
    auto const include_paths{usf_loader::resolve_include_paths(path_prefix, usf_file_paths)};
    if (include_paths) {
        out_include_file_paths.Append(*include_paths);
    }
}
TSharedPtr<class SGraphNodeMaterialBase>
    UMaterialExpressionUSFLoader::CreateCustomGraphNodeWidget() {
    if (GraphNode != nullptr) {
        return SNew(SGraphNodeMaterialUSFLoader, GraphNode);
    }
    return nullptr;
}
#endif

#undef LOCTEXT_NAMESPACE
