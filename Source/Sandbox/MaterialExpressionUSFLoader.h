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

    /** Path to the USF file to include (relative to project shader directory) */
    UPROPERTY(EditAnywhere, Category = "USF Loader")
    FString usf_file_path{};

    /** The type of dummy output to generate */
    UPROPERTY(EditAnywhere, Category = "USF Loader")
    EUSFLoaderOutputType output_type{EUSFLoaderOutputType::Float1};

    /** Dummy value to return (only used for Float1 output type) */
    UPROPERTY(EditAnywhere,
              Category = "USF Loader",
              meta = (EditCondition = "output_type == EUSFLoaderOutputType::Float1"))
    float dummy_value{0.0f};

    virtual int32 Compile(class FMaterialCompiler* compiler, int32 output_index) override;
    virtual void GetCaption(TArray<FString>& out_captions) const override;
    virtual FText GetCreationDescription() const override;
    virtual FText GetCreationName() const override;
    virtual bool CanRenameNode() const override;
  private:
    FString get_output_type_hlsl() const;
    FString get_dummy_return_value() const;
    UMaterialExpressionCustom * custom_expression;
};
