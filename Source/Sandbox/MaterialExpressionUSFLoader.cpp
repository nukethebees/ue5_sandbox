#include "Sandbox/MaterialExpressionUSFLoader.h"

#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant2Vector.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionConstant4Vector.h"

UMaterialExpressionUSFLoader::UMaterialExpressionUSFLoader() {
    // Set up the material expression properties
    bShaderInputData = false; // This is a utility node, not shader input data
    bCollapsed = false;

    //// Set default USF file path
    // usf_file_path = TEXT("foo.usf");
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

    auto const output_type_hlsl{get_output_type_hlsl()};
    auto const dummy_return{get_dummy_return_value()};

    auto input_name{FName(TEXT("previous_block"))};
    previous_block.InputName = input_name;

    // Always provide an input - either connected or default constant expression
    FExpressionInput input_to_use = previous_block;

    if (!previous_block.IsConnected()) {
        // Create a default constant expression based on output type
        switch (output_type) {
            case EUSFLoaderOutputType::Float1: {
                auto* constant_expr = NewObject<UMaterialExpressionConstant>(this);
                constant_expr->R = dummy_value;
                input_to_use.Expression = constant_expr;
                input_to_use.OutputIndex = 0;
                break;
            }
            case EUSFLoaderOutputType::Float2: {
                auto* vector_expr = NewObject<UMaterialExpressionConstant2Vector>(this);
                vector_expr->R = 0.0f;
                vector_expr->G = 0.0f;
                input_to_use.Expression = vector_expr;
                input_to_use.OutputIndex = 0;
                break;
            }
            case EUSFLoaderOutputType::Float3: {
                auto* vector_expr = NewObject<UMaterialExpressionConstant3Vector>(this);
                vector_expr->Constant = FLinearColor(0.0f, 0.0f, 0.0f, 1.0f);
                input_to_use.Expression = vector_expr;
                input_to_use.OutputIndex = 0;
                break;
            }
            case EUSFLoaderOutputType::Float4: {
                auto* vector_expr = NewObject<UMaterialExpressionConstant4Vector>(this);
                vector_expr->Constant = FLinearColor(0.0f, 0.0f, 0.0f, 1.0f);
                input_to_use.Expression = vector_expr;
                input_to_use.OutputIndex = 0;
                break;
            }
        }
        input_to_use.InputName = input_name;
    }

    TArray<struct FCustomInput> named_inputs;
    named_inputs.Add(FCustomInput(input_to_use.InputName, input_to_use));

    // Always use the input name in shader code - it's guaranteed to exist now
    FString shader_code{TEXT("// USF Loader: End current function\n"
                             "return previous_block;\n"
                             "}\n\n")};

    // Handle USF file includes
    if (usf_file_paths.Num() == 0) {
        shader_code += TEXT("// No USF files specified\n\n");
    } else {
        for (FString const& file_path : usf_file_paths) {
            if (file_path.IsEmpty()) {
                shader_code += TEXT("// Empty file path - include skipped\n");
                continue;
            }

            // Combine prefix with file path
            FString full_path = path_prefix + file_path;

            // Basic validation - check if combined path looks reasonable
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
    // custom_expression->OutputType = TEnumAsByte<EUSFLoaderOutputType>(output_type);
    custom_expression->Inputs = std::move(named_inputs);

    debug_code = shader_code;

    // return compiler->CustomExpression(custom_expression, output_index, compiled_inputs);
    return custom_expression->Compile(compiler, output_index);
}

void UMaterialExpressionUSFLoader::GetCaption(TArray<FString>& out_captions) const {
    if (usf_file_paths.Num() == 0) {
        out_captions.Add(TEXT("USF Loader: <No files>"));
    } else if (usf_file_paths.Num() == 1) {
        out_captions.Add(FString::Printf(TEXT("USF Loader: %s"), *usf_file_paths[0]));
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

FString UMaterialExpressionUSFLoader::get_output_type_hlsl() const {
    switch (output_type) {
        case EUSFLoaderOutputType::Float1:
            return TEXT("float");
        case EUSFLoaderOutputType::Float2:
            return TEXT("float2");
        case EUSFLoaderOutputType::Float3:
            return TEXT("float3");
        case EUSFLoaderOutputType::Float4:
            return TEXT("float4");
        default:
            return TEXT("float");
    }
}

FString UMaterialExpressionUSFLoader::get_dummy_return_value() const {
    switch (output_type) {
        case EUSFLoaderOutputType::Float1:
            return FString::Printf(TEXT("%.6f"), dummy_value);
        case EUSFLoaderOutputType::Float2:
            return TEXT("float2(0.0, 0.0)");
        case EUSFLoaderOutputType::Float3:
            return TEXT("float3(0.0, 0.0, 0.0)");
        case EUSFLoaderOutputType::Float4:
            return TEXT("float4(0.0, 0.0, 0.0, 1.0)");
        default:
            return TEXT("0.0");
    }
}

bool UMaterialExpressionUSFLoader::is_valid_include_path(FString const& path) const {
    // Basic validation for include paths
    if (path.IsEmpty()) {
        return false;
    }

    // Check if it starts with a forward slash (absolute include path)
    if (!path.StartsWith(TEXT("/"))) {
        return false;
    }

    // Check if it has a reasonable file extension
    if (!path.EndsWith(TEXT(".usf")) && !path.EndsWith(TEXT(".ush"))) {
        return false;
    }

    // Check for invalid characters that could cause compilation issues
    if (path.Contains(TEXT("\"")) || path.Contains(TEXT("\n")) || path.Contains(TEXT("\r"))) {
        return false;
    }

    return true;
}
