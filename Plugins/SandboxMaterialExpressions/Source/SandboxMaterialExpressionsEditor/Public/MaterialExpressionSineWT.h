#pragma once

#include "Materials/MaterialExpression.h"

#include "MaterialExpressionSineWt.generated.h"

UCLASS()
class SANDBOXMATERIALEXPRESSIONSEDITOR_API UMaterialExpressionSineWT final
    : public UMaterialExpression {
    GENERATED_BODY()
  public:
    UMaterialExpressionSineWT();

    UPROPERTY()
    FExpressionInput freq;

#if WITH_EDITOR
    virtual int32 Compile(class FMaterialCompiler* compiler, int32 output_index) override;
    virtual void GetCaption(TArray<FString>& out_captions) const override;
    virtual FName GetInputName(int32 i) const override;
#endif
};
