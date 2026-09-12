#include "SGraphNodeMaterialUSFLoader.h"

#include "MaterialExpressionUSFLoader.h"
#include "USFLoader.h"

#include "GraphEditAction.h"
#include "GraphEditorSettings.h"
#include "MaterialGraph/MaterialGraphNode.h"
#include "Materials/Material.h"
#include "Text/HLSLSyntaxHighlighterMarshaller.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"

#define LOCTEXT_NAMESPACE "SGraphNodeMaterialUSFLoader"

void SGraphNodeMaterialUSFLoader::Construct(FArguments const&, UEdGraphNode* node) {
    auto* const material_graph_node{Cast<UMaterialGraphNode>(node)};
    if (material_graph_node == nullptr) {
        UE_LOG(LogUSFLoader,
               Error,
               TEXT("Unable to construct the USF Loader graph node from '%s'."),
               node != nullptr ? *node->GetClass()->GetName() : TEXT("nullptr"));
        return;
    }

    GraphNode = material_graph_node;
    MaterialNode = material_graph_node;

    auto get_style{[&app_style = FAppStyle::Get()](auto* style) {
        return app_style.GetWidgetStyle<FTextBlockStyle>(style);
    }};

    auto code_style{FHLSLSyntaxHighlighterMarshaller::FSyntaxTextStyle{
        get_style("SyntaxHighlight.SourceCode.Normal"),
        get_style("SyntaxHighlight.SourceCode.Operator"),
        get_style("SyntaxHighlight.SourceCode.Keyword"),
        get_style("SyntaxHighlight.SourceCode.String"),
        get_style("SyntaxHighlight.SourceCode.Number"),
        get_style("SyntaxHighlight.SourceCode.Comment"),
        get_style("SyntaxHighlight.SourceCode.PreProcessorKeyword"),
        get_style("SyntaxHighlight.SourceCode.Error")}};

    syntax_highlighter_ = FHLSLSyntaxHighlighterMarshaller::Create(code_style);

    SetCursor(EMouseCursor::CardinalCross);
    UpdateGraphNode();
}

void SGraphNodeMaterialUSFLoader::CreateBelowPinControls(TSharedPtr<SVerticalBox> main_box) {
    TAttribute<FText> generated_code;
    generated_code.Bind(this, &ThisClass::get_generated_code_text);

    constexpr auto padding{UMaterialExpressionUSFLoader::Constants::ui_padding};
    SAssignNew(generated_code_text_box_, SMultiLineEditableTextBox)
        .AutoWrapText(false)
        .IsReadOnly(true)
        .Margin(FMargin(padding, padding, padding, padding))
        .Text(generated_code)
        .Visibility(this, &ThisClass::code_visibility)
        .Marshaller(syntax_highlighter_)
        .ToolTipText(LOCTEXT("GeneratedCodeTooltip", "Generated USF Loader shader code"));

    TSharedPtr<SVerticalBox> preview_box;
    SAssignNew(preview_box, SVerticalBox);

    SGraphNodeMaterialBase::CreateBelowPinControls(preview_box);

    constexpr auto margin{Expr::Constants::ui_margin};
    // clang-format off
    main_box->AddSlot()
        .Padding(Settings->GetNonPinNodeBodyPadding())
        .AutoHeight()
        [
            SNew(SHorizontalBox) 
            + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(FMargin(margin, padding, margin, margin))
                [
                    generated_code_text_box_.ToSharedRef()
                ]
            + SHorizontalBox::Slot()
                .AutoWidth()
                [
                    preview_box.ToSharedRef()
                ]
            + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(FMargin(margin, padding, margin, margin))
                [
                    SNew(SButton)
                        .Text(LOCTEXT("RefreshIncludes", "Refresh Includes"))
                        .ToolTipText(LOCTEXT("RefreshIncludesTooltip", "Recompile the material after shader include files change"))
                        .OnClicked(this, &ThisClass::on_refresh_includes_clicked)
                ]
        ];
    // clang-format on
}

EVisibility SGraphNodeMaterialUSFLoader::code_visibility() const {
    return IsAdvancedViewChecked() == ECheckBoxState::Checked ? EVisibility::Visible
                                                              : EVisibility::Collapsed;
}

FText SGraphNodeMaterialUSFLoader::get_generated_code_text() const {
    if (auto const* usf_expression{get_usf_loader_expression()}) {
        return FText::FromString(usf_expression->debug_code);
    }
    return LOCTEXT("NoGeneratedCode", "// No code generated");
}

UMaterialExpressionUSFLoader* SGraphNodeMaterialUSFLoader::get_usf_loader_expression() const {
    if (MaterialNode != nullptr && MaterialNode->MaterialExpression != nullptr) {
        return Cast<UMaterialExpressionUSFLoader>(MaterialNode->MaterialExpression.Get());
    }
    return nullptr;
}

void SGraphNodeMaterialUSFLoader::CreateAdvancedViewArrow(TSharedPtr<SVerticalBox> main_box) {
    if (get_usf_loader_expression() == nullptr) {
        return;
    }

    constexpr auto margin{UMaterialExpressionUSFLoader::Constants::ui_margin_small};
    // clang-format off
    main_box->AddSlot()
        .AutoHeight()
        .HAlign(HAlign_Fill)
        .VAlign(VAlign_Top)
        .Padding(margin, 0, margin, margin)
        [
           SNew(SCheckBox)
               .Visibility(this, &ThisClass::AdvancedViewArrowVisibility)
               .OnCheckStateChanged(this, &ThisClass::OnAdvancedViewChanged)
               .IsChecked(this, &ThisClass::IsAdvancedViewChecked)
               .Cursor(EMouseCursor::Default)
               .Style(FAppStyle::Get(), "Graph.Node.AdvancedView")
               [
                   SNew(SHorizontalBox) 
                       + SHorizontalBox::Slot()
                       .VAlign(VAlign_Center)
                       .HAlign(HAlign_Center)
                       [
                           SNew(SImage).Image(this, &ThisClass::GetAdvancedViewArrow)
                       ]
               ]
        ];
    // clang-format on
}

EVisibility SGraphNodeMaterialUSFLoader::AdvancedViewArrowVisibility() const {
    return GraphNode != nullptr ? EVisibility::Visible : EVisibility::Collapsed;
}

void SGraphNodeMaterialUSFLoader::OnAdvancedViewChanged(ECheckBoxState const new_checked_state) {
    if (auto* usf_expression{get_usf_loader_expression()}) {
        bool const show_code_preview{new_checked_state == ECheckBoxState::Checked};
        if (usf_expression->bShowCodePreview != show_code_preview) {
            usf_expression->Modify();
            usf_expression->bShowCodePreview = show_code_preview;
            usf_expression->MarkPackageDirty();
        }
    }
}

ECheckBoxState SGraphNodeMaterialUSFLoader::IsAdvancedViewChecked() const {
    using enum ECheckBoxState;

    if (auto const* usf_expression{get_usf_loader_expression()}) {
        return usf_expression->bShowCodePreview ? Checked : Unchecked;
    }
    return Unchecked;
}

FSlateBrush const* SGraphNodeMaterialUSFLoader::GetAdvancedViewArrow() const {
    static auto const chevron_up{TEXT("Icons.ChevronUp")};
    static auto const chevron_down{TEXT("Icons.ChevronDown")};

    if (get_usf_loader_expression() == nullptr) {
        return FAppStyle::GetBrush(chevron_down);
    }

    auto const state{IsAdvancedViewChecked()};
    return FAppStyle::GetBrush(state == ECheckBoxState::Checked ? chevron_up : chevron_down);
}

FReply SGraphNodeMaterialUSFLoader::on_refresh_includes_clicked() {
    auto* const usf_expression{get_usf_loader_expression()};
    if (usf_expression == nullptr) {
        UE_LOG(
            LogUSFLoader, Warning, TEXT("Unable to refresh includes without a loader expression."));
        return FReply::Unhandled();
    }

    auto* const material{usf_expression->Material.Get()};
    if (material == nullptr) {
        UE_LOG(LogUSFLoader, Warning, TEXT("Unable to refresh includes without a material."));
        return FReply::Unhandled();
    }

    material->PreEditChange(nullptr);
    material->PostEditChange();

    if (auto* const graph{GraphNode != nullptr ? GraphNode->GetGraph() : nullptr}) {
        graph->NotifyGraphChanged();
        graph->NotifyNodeChanged(MaterialNode);
    } else {
        UE_LOG(LogUSFLoader, Warning, TEXT("Unable to refresh the USF Loader graph node."));
    }

    return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
