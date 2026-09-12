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

    void Construct(FArguments const&, UEdGraphNode* node);
  protected:
    //~ Begin SGraphNode Interface
    virtual void CreateBelowPinControls(TSharedPtr<SVerticalBox> main_box) override;
    virtual void CreateAdvancedViewArrow(TSharedPtr<SVerticalBox> main_box) override;
    virtual EVisibility AdvancedViewArrowVisibility() const override;
    virtual void OnAdvancedViewChanged(ECheckBoxState const new_checked_state) override;
    virtual ECheckBoxState IsAdvancedViewChecked() const override;
    virtual FSlateBrush const* GetAdvancedViewArrow() const override;
    //~ End SGraphNode Interface
  private:
    FReply on_refresh_includes_clicked();
    EVisibility code_visibility() const;
    FText get_generated_code_text() const;
    UMaterialExpressionUSFLoader* get_usf_loader_expression() const;

    TSharedPtr<FHLSLSyntaxHighlighterMarshaller> syntax_highlighter_;
    TSharedPtr<SMultiLineEditableTextBox> generated_code_text_box_;
};
