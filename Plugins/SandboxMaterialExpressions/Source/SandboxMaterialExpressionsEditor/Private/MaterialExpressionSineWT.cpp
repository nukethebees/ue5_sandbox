#include "MaterialExpressionSineWt.h"

#include "MaterialCompiler.h"

#include <numbers>

UMaterialExpressionSineWT::UMaterialExpressionSineWT() {
#if WITH_EDITORONLY_DATA
    MenuCategories.Add(FText::FromName(TEXT("Math")));
#endif
}

#if WITH_EDITOR
int32 UMaterialExpressionSineWT::Compile(FMaterialCompiler* compiler, int32 output_index) {
    auto const w{
        compiler->Mul(compiler->Constant(2.f * std::numbers::pi_v<float>), freq.Compile(compiler))};
    auto const wt{compiler->Mul(w, compiler->GameTime(false, 0.0f))};

    return compiler->Sine(wt);
}

void UMaterialExpressionSineWT::GetCaption(TArray<FString>& out_captions) const {
    out_captions.Add(TEXT("Sine(wt)"));
}
FName UMaterialExpressionSineWT::GetInputName(int32 i) const {
    switch (i) {
        case 0:
            return TEXT("freq");
        default:
            break;
    }

    return TEXT("???");
}
#endif
