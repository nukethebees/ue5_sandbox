#pragma once

#include "CoreMinimal.h"
#include "MaterialCompiler.h"
#include "Materials/MaterialExpression.h"

#include "MaterialExpressionUSFLoader.generated.h"

UENUM(BlueprintType)
enum class EUSFLoaderOutputType : uint8 {
    Float1 UMETA(DisplayName = "Float (1)"),
    Float2 UMETA(DisplayName = "Float2 (2)"),
    Float3 UMETA(DisplayName = "Float3 (3)"),
    Float4 UMETA(DisplayName = "Float4 (4)")
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

    /** The type of dummy output to generate */
    UPROPERTY(EditAnywhere, Category = "USF Loader")
    EUSFLoaderOutputType output_type{EUSFLoaderOutputType::Float1};

    /** Dummy value to return (only used for Float1 output type) */
    UPROPERTY(EditAnywhere, Category = "USF Loader")
    float dummy_value{0.0f};

    UPROPERTY(VisibleAnywhere, Category = "USF Loader", meta = (MultiLine = "true"))
    FString debug_code;

    UPROPERTY(meta = (RequiredInput = "false", ToolTip = "Optional input to chain USF blocks"))
    FExpressionInput previous_block;

    virtual int32 Compile(class FMaterialCompiler* compiler, int32 output_index) override;
    virtual void GetCaption(TArray<FString>& out_captions) const override;
    virtual FText GetCreationDescription() const override;
    virtual FText GetCreationName() const override;
    virtual bool CanRenameNode() const override;
    virtual FString GetEditableName() const override;
  private:
    FString get_output_type_hlsl() const;
    FString get_dummy_return_value() const;
    bool is_valid_include_path(FString const& path) const;

    UPROPERTY(EditAnywhere, Category = "USF Loader")
    FString instance_name{};
};
