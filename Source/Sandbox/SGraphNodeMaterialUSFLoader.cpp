#include "SGraphNodeMaterialUSFLoader.h"

#include "GraphEditorSettings.h"
#include "MaterialGraph/MaterialGraphNode.h"
#include "MaterialExpressionUSFLoader.h"
#include "Text/HLSLSyntaxHighlighterMarshaller.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"

void SGraphNodeMaterialUSFLoader::Construct(FArguments const& InArgs, UEdGraphNode* InNode) {
    // Cast UEdGraphNode to UMaterialGraphNode - should succeed for material expressions
    UMaterialGraphNode* MaterialGraphNode = Cast<UMaterialGraphNode>(InNode);
    if (!MaterialGraphNode) {
        UE_LOG(LogTemp, Error, TEXT("SGraphNodeMaterialUSFLoader: Failed to cast UEdGraphNode to UMaterialGraphNode"));
        return;
    }

    this->GraphNode = MaterialGraphNode;
    this->MaterialNode = MaterialGraphNode;

    // Create HLSL syntax highlighter
    FHLSLSyntaxHighlighterMarshaller::FSyntaxTextStyle CodeStyle(
        FAppStyle::Get().GetWidgetStyle<FTextBlockStyle>("SyntaxHighlight.SourceCode.Normal"),
        FAppStyle::Get().GetWidgetStyle<FTextBlockStyle>("SyntaxHighlight.SourceCode.Operator"),
        FAppStyle::Get().GetWidgetStyle<FTextBlockStyle>("SyntaxHighlight.SourceCode.Keyword"),
        FAppStyle::Get().GetWidgetStyle<FTextBlockStyle>("SyntaxHighlight.SourceCode.String"),
        FAppStyle::Get().GetWidgetStyle<FTextBlockStyle>("SyntaxHighlight.SourceCode.Number"),
        FAppStyle::Get().GetWidgetStyle<FTextBlockStyle>("SyntaxHighlight.SourceCode.Comment"),
        FAppStyle::Get().GetWidgetStyle<FTextBlockStyle>(
            "SyntaxHighlight.SourceCode.PreProcessorKeyword"),
        FAppStyle::Get().GetWidgetStyle<FTextBlockStyle>("SyntaxHighlight.SourceCode.Error"));

    SyntaxHighlighter = FHLSLSyntaxHighlighterMarshaller::Create(CodeStyle);

    this->SetCursor(EMouseCursor::CardinalCross);
    this->UpdateGraphNode();
}

void SGraphNodeMaterialUSFLoader::CreateBelowPinControls(TSharedPtr<SVerticalBox> MainBox) {
    // Bind text to our getter method using Slate widget binding
    TAttribute<FText> GetText;
    GetText.Bind(this, &SGraphNodeMaterialUSFLoader::GetGeneratedCodeText);

    // Create read-only text box for generated code
    SAssignNew(GeneratedCodeTextBox, SMultiLineEditableTextBox)
        .AutoWrapText(false)
        .IsReadOnly(true) // Read-only unlike Custom expression
        .Margin(FMargin(5, 5, 5, 5))
        .Text(GetText)
        .Visibility(this, &SGraphNodeMaterialUSFLoader::CodeVisibility)
        .Marshaller(SyntaxHighlighter)
        .ToolTipText(FText::FromString(TEXT("Generated shader code from USF Loader (read-only)")));

    TSharedPtr<SVerticalBox> PreviewBox;
    SAssignNew(PreviewBox, SVerticalBox);

    SGraphNodeMaterialBase::CreateBelowPinControls(PreviewBox);

    MainBox->AddSlot()
        .Padding(Settings->GetNonPinNodeBodyPadding())
        .AutoHeight()[SNew(SHorizontalBox) +
                      SHorizontalBox::Slot().AutoWidth().Padding(
                          FMargin(10.f, 5.f, 10.f, 10.f))[GeneratedCodeTextBox.ToSharedRef()] +
                      SHorizontalBox::Slot().AutoWidth()[PreviewBox.ToSharedRef()]];
}

EVisibility SGraphNodeMaterialUSFLoader::CodeVisibility() const {
    ECheckBoxState State = IsAdvancedViewChecked();
    return (State == ECheckBoxState::Checked) ? EVisibility::Visible : EVisibility::Collapsed;
}

FText SGraphNodeMaterialUSFLoader::GetGeneratedCodeText() const {
    if (UMaterialExpressionUSFLoader* USFExpression = GetUSFLoaderExpression()) {
        return FText::FromString(USFExpression->debug_code);
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

    MainBox->AddSlot()
        .AutoHeight()
        .HAlign(HAlign_Fill)
        .VAlign(VAlign_Top)
        .Padding(
            3,
            0,
            3,
            3)[SNew(SCheckBox)
                   .Visibility(this, &SGraphNodeMaterialUSFLoader::AdvancedViewArrowVisibility)
                   .OnCheckStateChanged(this, &SGraphNodeMaterialUSFLoader::OnAdvancedViewChanged)
                   .IsChecked(this, &SGraphNodeMaterialUSFLoader::IsAdvancedViewChecked)
                   .Cursor(EMouseCursor::Default)
                   .Style(FAppStyle::Get(), "Graph.Node.AdvancedView")
                       [SNew(SHorizontalBox) +
                        SHorizontalBox::Slot()
                            .VAlign(VAlign_Center)
                            .HAlign(HAlign_Center)[SNew(SImage).Image(
                                this, &SGraphNodeMaterialUSFLoader::GetAdvancedViewArrow)]]];
}

EVisibility SGraphNodeMaterialUSFLoader::AdvancedViewArrowVisibility() const {
    // Always show the arrow for USF Loader
    return GraphNode ? EVisibility::Visible : EVisibility::Collapsed;
}

void SGraphNodeMaterialUSFLoader::OnAdvancedViewChanged(ECheckBoxState const NewCheckedState) {
    // Code view state is now handled locally in this widget
    // Could store state in UMaterialExpressionUSFLoader if needed
}

ECheckBoxState SGraphNodeMaterialUSFLoader::IsAdvancedViewChecked() const {
    // Default to showing code for USF Loader
    return GetUSFLoaderExpression() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

#define CHEVRON_UP TEXT("Icons.ChevronUp")
#define CHEVRON_DOWN TEXT("Icons.ChevronDown")

const FSlateBrush* SGraphNodeMaterialUSFLoader::GetAdvancedViewArrow() const {
    if (!GetUSFLoaderExpression()) {
        return FAppStyle::GetBrush(CHEVRON_DOWN);
    }

    ECheckBoxState State = IsAdvancedViewChecked();
    return FAppStyle::GetBrush((State == ECheckBoxState::Checked) ? CHEVRON_UP : CHEVRON_DOWN);
}
