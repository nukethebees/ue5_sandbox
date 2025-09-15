#include "SGraphNodeMaterialUSFLoader.h"

#include "GraphEditorSettings.h"
#include "MaterialGraph/MaterialGraphNode.h"
#include "MaterialExpressionUSFLoader.h"
#include "Text/HLSLSyntaxHighlighterMarshaller.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"

void SGraphNodeMaterialUSFLoader::Construct(FArguments const& InArgs, UEdGraphNode* InNode) {
    auto* material_graph_node{Cast<UMaterialGraphNode>(InNode)};
    if (!material_graph_node) {
        UE_LOG(
            LogTemp,
            Error,
            TEXT("SGraphNodeMaterialUSFLoader: Failed to cast UEdGraphNode to UMaterialGraphNode"));
        return;
    }

    this->GraphNode = material_graph_node;
    this->MaterialNode = material_graph_node;

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

    SyntaxHighlighter = FHLSLSyntaxHighlighterMarshaller::Create(code_style);

    this->SetCursor(EMouseCursor::CardinalCross);
    this->UpdateGraphNode();
}

void SGraphNodeMaterialUSFLoader::CreateBelowPinControls(TSharedPtr<SVerticalBox> MainBox) {
    TAttribute<FText> GetText{};
    GetText.Bind(this, &SGraphNodeMaterialUSFLoader::GetGeneratedCodeText);

    // Create read-only text box for generated code
    constexpr auto padding{UMaterialExpressionUSFLoader::Constants::UI_PADDING};
    SAssignNew(GeneratedCodeTextBox, SMultiLineEditableTextBox)
        .AutoWrapText(false)
        .IsReadOnly(true) // Read-only unlike Custom expression
        .Margin(FMargin(padding, padding, padding, padding))
        .Text(GetText)
        .Visibility(this, &SGraphNodeMaterialUSFLoader::CodeVisibility)
        .Marshaller(SyntaxHighlighter)
        .ToolTipText(FText::FromString(TEXT("Generated shader code from USF Loader (read-only)")));

    TSharedPtr<SVerticalBox> PreviewBox{};
    SAssignNew(PreviewBox, SVerticalBox);

    SGraphNodeMaterialBase::CreateBelowPinControls(PreviewBox);

    constexpr auto margin{UMaterialExpressionUSFLoader::Constants::UI_MARGIN};
    MainBox->AddSlot()
        .Padding(Settings->GetNonPinNodeBodyPadding())
        .AutoHeight()[SNew(SHorizontalBox) +
                      SHorizontalBox::Slot().AutoWidth().Padding(FMargin(
                          margin, padding, margin, margin))[GeneratedCodeTextBox.ToSharedRef()] +
                      SHorizontalBox::Slot().AutoWidth()[PreviewBox.ToSharedRef()]];
}

EVisibility SGraphNodeMaterialUSFLoader::CodeVisibility() const {
    auto State{IsAdvancedViewChecked()};
    return (State == ECheckBoxState::Checked) ? EVisibility::Visible : EVisibility::Collapsed;
}

FText SGraphNodeMaterialUSFLoader::GetGeneratedCodeText() const {
    if (auto* usf_expression{GetUSFLoaderExpression()}) {
        return FText::FromString(usf_expression->debug_code);
    }
    return FText::FromString(TEXT("// No code generated"));
}

UMaterialExpressionUSFLoader* SGraphNodeMaterialUSFLoader::GetUSFLoaderExpression() const {
    if (MaterialNode && MaterialNode->MaterialExpression) {
        return Cast<UMaterialExpressionUSFLoader>(MaterialNode->MaterialExpression.Get());
    }
    return nullptr;
}

void SGraphNodeMaterialUSFLoader::CreateAdvancedViewArrow(TSharedPtr<SVerticalBox> MainBox) {
    if (!GetUSFLoaderExpression()) {
        return;
    }

    using My = SGraphNodeMaterialUSFLoader;

    constexpr auto margin{UMaterialExpressionUSFLoader::Constants::UI_MARGIN_SMALL};
    MainBox->AddSlot()
        .AutoHeight()
        .HAlign(HAlign_Fill)
        .VAlign(VAlign_Top)
        .Padding(
            margin,
            0,
            margin,
            margin)[SNew(SCheckBox)
                        .Visibility(this, &My::AdvancedViewArrowVisibility)
                        .OnCheckStateChanged(this, &My::OnAdvancedViewChanged)
                        .IsChecked(this, &My::IsAdvancedViewChecked)
                        .Cursor(EMouseCursor::Default)
                        .Style(FAppStyle::Get(), "Graph.Node.AdvancedView")
                            [SNew(SHorizontalBox) + SHorizontalBox::Slot()
                                                        .VAlign(VAlign_Center)
                                                        .HAlign(HAlign_Center)[SNew(SImage).Image(
                                                            this, &My::GetAdvancedViewArrow)]]];
}

EVisibility SGraphNodeMaterialUSFLoader::AdvancedViewArrowVisibility() const {
    // Always show the arrow for USF Loader
    return GraphNode ? EVisibility::Visible : EVisibility::Collapsed;
}

void SGraphNodeMaterialUSFLoader::OnAdvancedViewChanged(ECheckBoxState const NewCheckedState) {
    if (auto* usf_expression{GetUSFLoaderExpression()}) {
        usf_expression->bShowCodePreview = (NewCheckedState == ECheckBoxState::Checked);
        usf_expression->MarkPackageDirty();
    }
}

ECheckBoxState SGraphNodeMaterialUSFLoader::IsAdvancedViewChecked() const {
    using enum ECheckBoxState;

    if (auto* usf_expression{GetUSFLoaderExpression()}) {
        return usf_expression->bShowCodePreview ? Checked : Unchecked;
    }
    return Unchecked;
}

FSlateBrush const* SGraphNodeMaterialUSFLoader::GetAdvancedViewArrow() const {
    static auto const CHEVRON_UP{TEXT("Icons.ChevronUp")};
    static auto const CHEVRON_DOWN{TEXT("Icons.ChevronDown")};

    if (!GetUSFLoaderExpression()) {
        return FAppStyle::GetBrush(CHEVRON_DOWN);
    }

    auto const state{IsAdvancedViewChecked()};
    return FAppStyle::GetBrush((state == ECheckBoxState::Checked) ? CHEVRON_UP : CHEVRON_DOWN);
}
