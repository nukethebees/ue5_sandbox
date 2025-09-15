#include "MaterialExpressionUSFLoader.h"

#include <type_traits>

#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant2Vector.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionConstant4Vector.h"
#include "Materials/MaterialExpressionCustom.h"
#include "SandboxEditor/subsystems/USFPathValidationSubsystem.h"
#include "SGraphNodeMaterialUSFLoader.h"

UMaterialExpressionUSFLoader::UMaterialExpressionUSFLoader() {
    bShaderInputData = false; // This is a utility node, not shader input data
    bCollapsed = false;
    bHidePreviewWindow = true;
    instance_name = TEXT("USF Loader");
}

template <typename ExprT>
void set_input(UMaterialExpressionUSFLoader* loader,
               FExpressionInput& input_to_use,
               FUSFLoaderDefaults const& defaults) {
    auto* expr = NewObject<ExprT>(loader);

    if constexpr (std::is_same_v<ExprT, UMaterialExpressionConstant>) {
        expr->R = defaults.float1_value;
    } else if constexpr (std::is_same_v<ExprT, UMaterialExpressionConstant2Vector>) {
        expr->R = defaults.float2_value.X;
        expr->G = defaults.float2_value.Y;
    } else if constexpr (std::is_same_v<ExprT, UMaterialExpressionConstant3Vector>) {
        expr->Constant = FLinearColor(
            defaults.float3_value.X, defaults.float3_value.Y, defaults.float3_value.Z, 1.0f);
    } else if constexpr (std::is_same_v<ExprT, UMaterialExpressionConstant4Vector>) {
        expr->Constant = defaults.float4_value;
    } else {
        static_assert(!sizeof(ExprT), "Unhandled branch.");
    }

    input_to_use.Expression = expr;
    input_to_use.OutputIndex = 0;
}

int32 UMaterialExpressionUSFLoader::Compile(FMaterialCompiler* compiler, int32 output_index) {
    if (!compiler) {
        return INDEX_NONE;
    }

    // Generate the USF loader pattern:
    // 1. End the current function with a return so we can inject our custom code
    // 2. Include the USF file
    // 3. Create a dummy function
    // 4. Return the dummy value

    static auto const input_name{FName(TEXT("previous_block"))};
    previous_block.InputName = input_name;

    // Always provide an input - either connected or default constant expression
    FExpressionInput input_to_use{previous_block};

    if (!previous_block.IsConnected()) {
        // Create a default constant expression based on output type
        switch (output_type) {
            using enum ECustomMaterialOutputType;
            case CMOT_Float1: {
                set_input<UMaterialExpressionConstant>(this, input_to_use, default_values);
                break;
            }
            case CMOT_Float2: {
                set_input<UMaterialExpressionConstant2Vector>(this, input_to_use, default_values);
                break;
            }
            case CMOT_Float3: {
                set_input<UMaterialExpressionConstant3Vector>(this, input_to_use, default_values);
                break;
            }
            case CMOT_Float4: {
                set_input<UMaterialExpressionConstant4Vector>(this, input_to_use, default_values);
                break;
            }
            default: {
                // Fallback to Float1 for any other types
                set_input<UMaterialExpressionConstant>(this, input_to_use, default_values);
                break;
            }
        }
    }

    TArray<struct FCustomInput> named_inputs;
    named_inputs.Add(FCustomInput(input_to_use.InputName, input_to_use));

    // Always use the input name in shader code - it's guaranteed to exist now
    FString shader_code{Constants::ShaderHeaderTemplate()};

    // Handle USF file includes
    if (usf_file_paths.Num() == 0) {
        shader_code += Constants::NoFilesComment();
    } else {
        for (FString const& file_path : usf_file_paths) {
            if (file_path.IsEmpty()) {
                shader_code += Constants::EmptyPathComment();
                continue;
            }

            FString const full_path{path_prefix + file_path};

            if (is_valid_include_path(full_path)) {
                shader_code += FString::Printf(TEXT("#include \"%s\"\n"), *full_path);
            } else {
                shader_code += FString::Printf(
                    TEXT("// Invalid include path: \"%s\" - include failed\n"), *full_path);
            }
        }
        shader_code += TEXT("\n");
    }

    shader_code += FString::Printf(TEXT("void DummyUSFLoaderFn_%p() {\n"
                                        "    return;\n"),
                                   this);

    auto* custom_expression{NewObject<UMaterialExpressionCustom>(this)};
    custom_expression->Code = shader_code;
    custom_expression->OutputType = output_type;
    custom_expression->Inputs = std::move(named_inputs);

    debug_code = shader_code;

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
    return Constants::CreationDescription();
}

FText UMaterialExpressionUSFLoader::GetCreationName() const {
    return Constants::DisplayName();
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

bool UMaterialExpressionUSFLoader::is_valid_include_path(FString const& path) const {
    return UUSFPathValidationSubsystem::ValidateUSFPath(path);
}

TSharedPtr<class SGraphNodeMaterialBase>
    UMaterialExpressionUSFLoader::CreateCustomGraphNodeWidget() {
    UE_LOGFMT(LogTemp, Display, "UMaterialExpressionUSFLoader::CreateCustomGraphNodeWidget called");

    // Pass GraphNode directly to widget - it will handle the cast to UMaterialGraphNode internally
    if (GraphNode) {
        UE_LOGFMT(LogTemp, Display, "GraphNode exists, creating custom widget");
        return SNew(SGraphNodeMaterialUSFLoader, GraphNode);
    } else {
        UE_LOGFMT(LogTemp, Display, "GraphNode is null, cannot create widget");
        return nullptr;
    }
}
