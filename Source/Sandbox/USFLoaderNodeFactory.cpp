#include "Sandbox/USFLoaderNodeFactory.h"

#include "Sandbox/MaterialExpressionUSFLoader.h"
#include "Sandbox/MaterialGraphNode_USFLoader.h"
#include "Sandbox/SGraphNodeMaterialUSFLoader.h"

TSharedPtr<SGraphNode> FUSFLoaderNodeFactory::CreateNode(UEdGraphNode* Node) const {
    // Check if this is a material graph node
    if (UMaterialGraphNode* MaterialNode = Cast<UMaterialGraphNode>(Node)) {
        // Check if the material expression is our USF Loader type
        if (MaterialNode->MaterialExpression &&
            MaterialNode->MaterialExpression->IsA<UMaterialExpressionUSFLoader>()) {
            // Create our custom Slate widget for USF Loader nodes
            return SNew(SGraphNodeMaterialUSFLoader, MaterialNode);
        }
    }

    // Return nullptr for nodes we don't handle - let other factories try
    return nullptr;
}
