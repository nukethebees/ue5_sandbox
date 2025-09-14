#pragma once

#include "CoreMinimal.h"
#include "EdGraphUtilities.h"
#include "MaterialGraph/MaterialGraphNode.h"

class FUSFLoaderNodeFactory : public FGraphPanelNodeFactory {
  public:
    //~ Begin FGraphPanelNodeFactory Interface
    virtual TSharedPtr<class SGraphNode> CreateNode(UEdGraphNode* Node) const override;
    //~ End FGraphPanelNodeFactory Interface
};
