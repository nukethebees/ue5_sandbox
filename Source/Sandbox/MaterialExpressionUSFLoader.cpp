#include "Sandbox/MaterialExpressionUSFLoader.h"

#include "Materials/MaterialExpressionCustom.h"

UMaterialExpressionUSFLoader::UMaterialExpressionUSFLoader() {
    // Set up the material expression properties
    bShaderInputData = false; // This is a utility node, not shader input data
    bCollapsed = false;

    // Set default USF file path
    usf_file_path = TEXT("foo.usf");
}

int32 UMaterialExpressionUSFLoader::Compile(FMaterialCompiler* compiler, int32 output_index) {
    if (!compiler) {
        return INDEX_NONE;
    }

    // Generate the USF loader pattern:
    // 1. End the current function with a return
    // 2. Include the USF file
    // 3. Create a dummy function
    // 4. Return the dummy value

    auto const output_type_hlsl{get_output_type_hlsl()};
    auto const dummy_return{get_dummy_return_value()};

    auto const shader_code{FString::Printf(TEXT("// USF Loader: End current function\n"
                                                "return %s;\n"
                                                "}\n\n"
                                                "#include \"/Project/%s\"\n\n"
                                                "void DummyUSFLoaderFn_%p() {\n"
                                                "    return;\n"),
                                           *dummy_return,
                                           *usf_file_path,
                                           this)};

    //auto* custom_expression{NewObject<UMaterialExpressionCustom>(this)};
    if (!custom_expression) {
        custom_expression = NewObject<UMaterialExpressionCustom>(this);
    }
    custom_expression->Code = shader_code;

    TArray<int32> dummy_inputs{};

    return compiler->CustomExpression(custom_expression, output_index, dummy_inputs);
}

void UMaterialExpressionUSFLoader::GetCaption(TArray<FString>& out_captions) const {
    if (usf_file_path.IsEmpty()) {
        out_captions.Add(TEXT("USF Loader: <No file>"));
    } else {
        out_captions.Add(FString::Printf(TEXT("USF Loader: %s"), *usf_file_path));
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
