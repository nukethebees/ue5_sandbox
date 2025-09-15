//#include "Sandbox/MaterialExpressionUSFLoader.h"
//
//#if WITH_EDITOR
//#include "SandboxEditor/SGraphNodeMaterialUSFLoader.h"
//
//TSharedPtr<class SGraphNodeMaterialBase> UMaterialExpressionUSFLoader::CreateCustomGraphNodeWidget() {
//    UE_LOGFMT(LogTemp, Display, "UMaterialExpressionUSFLoader::CreateCustomGraphNodeWidget called");
//
//    // Pass GraphNode directly to widget - it will handle the cast internally
//    if (GraphNode) {
//        UE_LOGFMT(LogTemp, Display, "GraphNode exists, creating custom widget");
//        return SNew(SGraphNodeMaterialUSFLoader, GraphNode);
//    } else {
//        UE_LOGFMT(LogTemp, Display, "GraphNode is null, cannot create widget");
//        return nullptr;
//    }
//}
//#endif