#pragma once

#include "CoreMinimal.h"
#include "MaterialGraph/MaterialGraphNode.h"
#include "Types/SlateEnums.h"
#include "UObject/ObjectMacros.h"

#include "MaterialGraphNode_USFLoader.generated.h"

class UMaterialExpressionUSFLoader;

UCLASS(MinimalAPI)
class UMaterialGraphNode_USFLoader : public UMaterialGraphNode {
    GENERATED_UCLASS_BODY()
  public:
    //~ Begin UEdGraphNode Interface
    virtual TSharedPtr<SGraphNode> CreateVisualWidget() override;
    //~ End UEdGraphNode Interface

    /** Get the generated shader code from the USF loader expression */
    SANDBOX_API FText GetGeneratedCode() const;

    /** Handle code view toggle */
    SANDBOX_API void OnCodeViewChanged(ECheckBoxState const NewCheckedState);
    SANDBOX_API ECheckBoxState IsCodeViewChecked() const;
  private:
    /** Get the USF Loader expression this node represents */
    UMaterialExpressionUSFLoader* GetUSFLoaderExpression() const;
};
