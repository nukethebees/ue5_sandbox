#include "Sandbox/MaterialGraphNode_USFLoader.h"
#include "Sandbox/MaterialExpressionUSFLoader.h"

UMaterialGraphNode_USFLoader::UMaterialGraphNode_USFLoader(
    FObjectInitializer const& ObjectInitializer)
    : Super(ObjectInitializer) {}

TSharedPtr<SGraphNode> UMaterialGraphNode_USFLoader::CreateVisualWidget() {
    // This method won't be called directly - the factory handles widget creation
    // But we need to provide a fallback implementation
    return nullptr;
}

FText UMaterialGraphNode_USFLoader::GetGeneratedCode() const {
    if (UMaterialExpressionUSFLoader* USFLoaderExpression = GetUSFLoaderExpression()) {
        return FText::FromString(USFLoaderExpression->debug_code);
    }
    return FText::FromString(TEXT("// No code generated"));
}

void UMaterialGraphNode_USFLoader::OnCodeViewChanged(ECheckBoxState const NewCheckedState) {
    // For now, we don't have a ShowCode property like Custom expression
    // Could add this to UMaterialExpressionUSFLoader later if needed
}

ECheckBoxState UMaterialGraphNode_USFLoader::IsCodeViewChecked() const {
    // Default to showing code for USF Loader
    return ECheckBoxState::Checked;
}

UMaterialExpressionUSFLoader* UMaterialGraphNode_USFLoader::GetUSFLoaderExpression() const {
    return Cast<UMaterialExpressionUSFLoader>(MaterialExpression.Get());
}
