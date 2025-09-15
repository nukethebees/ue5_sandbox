#pragma once

#include "CoreMinimal.h"
#include "MaterialCompiler.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionCustom.h"

#include "MaterialExpressionUSFLoader.generated.h"

class SGraphNodeMaterialBase;

USTRUCT(BlueprintType)
struct FUSFLoaderDefaults {
    GENERATED_BODY()

    /** Default value for Float1 output type */
    UPROPERTY(EditAnywhere, meta = (DisplayName = "Float1 Default"))
    float float1_value{0.0f};

    /** Default value for Float2 output type */
    UPROPERTY(EditAnywhere, meta = (DisplayName = "Float2 Default"))
    FVector2D float2_value{0.0f, 0.0f};

    /** Default value for Float3 output type */
    UPROPERTY(EditAnywhere, meta = (DisplayName = "Float3 Default"))
    FVector float3_value{0.0f, 0.0f, 0.0f};

    /** Default value for Float4 output type */
    UPROPERTY(EditAnywhere, meta = (DisplayName = "Float4 Default"))
    FLinearColor float4_value{0.0f, 0.0f, 0.0f, 1.0f};
};

/**
 * Material expression that loads a USF file and makes its functions available to subsequent nodes.
 * This automates the pattern of terminating a function, including a USF file, and creating a dummy
 * function.
 */
UCLASS(meta = (DisplayName = "USF Loader", Category = "Custom|Shader"))
class SANDBOX_API UMaterialExpressionUSFLoader : public UMaterialExpression {
    GENERATED_BODY()
  public:
    UMaterialExpressionUSFLoader();

    struct Constants {
        static FText const& DisplayName() {
            static FText const text{FText::FromString(TEXT("USF Loader"))};
            return text;
        }
        static FText const& CreationDescription() {
            static FText const text{
                FText::FromString(TEXT("Loads a USF file and makes its functions available to "
                                       "subsequent material nodes"))};
            return text;
        }
    };

    /** Optional prefix to add to all file paths (e.g., "/Project/", "/Engine/",
     * "/Plugin/MyPlugin/") */
    UPROPERTY(EditAnywhere, Category = "USF Loader", meta = (DisplayName = "Path Prefix"))
    FString path_prefix{};

    /** Paths to USF files to include (combined with prefix to form full include paths) */
    UPROPERTY(EditAnywhere, Category = "USF Loader", meta = (DisplayName = "USF File Paths"))
    TArray<FString> usf_file_paths{};

    /** The type of output to generate */
    UPROPERTY(EditAnywhere, Category = "USF Loader", meta = (DisplayName = "Output Type"))
    TEnumAsByte<enum ECustomMaterialOutputType> output_type{ECustomMaterialOutputType::CMOT_Float1};

    /** Default values for different output types when no input is connected */
    UPROPERTY(EditAnywhere, Category = "USF Loader", meta = (DisplayName = "Default Values"))
    FUSFLoaderDefaults default_values;

    UPROPERTY(VisibleAnywhere, Category = "USF Loader", meta = (MultiLine = "true"))
    FString debug_code;

    UPROPERTY(meta = (RequiredInput = "false", ToolTip = "Optional input to chain USF blocks"))
    FExpressionInput previous_block;

    //~ Begin UMaterialExpression Interface
    virtual int32 Compile(class FMaterialCompiler* compiler, int32 output_index) override;
    virtual void GetCaption(TArray<FString>& out_captions) const override;
    virtual FText GetCreationDescription() const override;
    virtual FText GetCreationName() const override;
    virtual bool CanRenameNode() const override;
    virtual FString GetEditableName() const override;
#if WITH_EDITOR
    virtual TSharedPtr<class SGraphNodeMaterialBase> CreateCustomGraphNodeWidget() override;
#endif
    //~ End UMaterialExpression Interface
  private:
    bool is_valid_include_path(FString const& path) const;

    UPROPERTY(EditAnywhere, Category = "USF Loader")
    FString instance_name{};
};
