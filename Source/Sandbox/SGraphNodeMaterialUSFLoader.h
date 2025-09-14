#pragma once

#include "CoreMinimal.h"
#include "MaterialEditor/MaterialNodes/SGraphNodeMaterialBase.h"
#include "Text/HLSLSyntaxHighlighterMarshaller.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"

class UMaterialGraphNode_USFLoader;

class SANDBOX_API SGraphNodeMaterialUSFLoader : public SGraphNodeMaterialBase {
  public:
    SLATE_BEGIN_ARGS(SGraphNodeMaterialUSFLoader) {}
    SLATE_END_ARGS()

    void Construct(FArguments const& InArgs, UMaterialGraphNode* InNode);

    UMaterialGraphNode* GetMaterialGraphNode() const { return MaterialNode; }
  protected:
    //~ Begin SGraphNode Interface
    virtual void CreateBelowPinControls(TSharedPtr<SVerticalBox> MainBox) override;
    virtual void CreateAdvancedViewArrow(TSharedPtr<SVerticalBox> MainBox) override;
    virtual EVisibility AdvancedViewArrowVisibility() const override;
    virtual void OnAdvancedViewChanged(ECheckBoxState const NewCheckedState) override;
    virtual ECheckBoxState IsAdvancedViewChecked() const override;
    virtual FSlateBrush const* GetAdvancedViewArrow() const override;
    //~ End SGraphNode Interface
  private:
    /** Visibility of the generated code display */
    EVisibility CodeVisibility() const;

    /** Get the generated code text */
    FText GetGeneratedCodeText() const;
  protected:
    /** Syntax highlighter for HLSL code */
    TSharedPtr<FHLSLSyntaxHighlighterMarshaller> SyntaxHighlighter;

    /** The read-only text box showing generated shader code */
    TSharedPtr<SMultiLineEditableTextBox> GeneratedCodeTextBox;

    /** Reference to our specific USF Loader graph node */
    UMaterialGraphNode_USFLoader* USFLoaderGraphNode;
};
