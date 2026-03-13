#pragma once

#include "CoreMinimal.h"
#include "MaterialEditor/MaterialNodes/SGraphNodeMaterialBase.h"

class UMaterialExpressionUSFLoader;
class UMaterialGraphNode;
class FHLSLSyntaxHighlighterMarshaller;
class SMultiLineEditableTextBox;

class SGraphNodeMaterialUSFLoader : public SGraphNodeMaterialBase {
  public:
    using ThisClass = SGraphNodeMaterialUSFLoader;
    using Expr = UMaterialExpressionUSFLoader;

    SLATE_BEGIN_ARGS(ThisClass) {}
    SLATE_END_ARGS()

    void Construct(FArguments const& InArgs, UEdGraphNode* InNode);

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
    FReply on_mark_dirty_clicked();
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

    /** Get the USF Loader expression from this node */
    UMaterialExpressionUSFLoader* GetUSFLoaderExpression() const;
};
