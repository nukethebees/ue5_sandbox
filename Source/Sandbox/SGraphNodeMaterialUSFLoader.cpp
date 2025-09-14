#include "Sandbox/SGraphNodeMaterialUSFLoader.h"

#include "Sandbox/MaterialGraphNode_USFLoader.h"
#include "GraphEditorSettings.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Images/SImage.h"

void SGraphNodeMaterialUSFLoader::Construct(FArguments const& InArgs, UMaterialGraphNode* InNode) {
    this->GraphNode = InNode;
    this->MaterialNode = InNode;
    this->USFLoaderGraphNode = CastChecked<UMaterialGraphNode_USFLoader>(InNode);

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
    TAttribute<FText> GetText;
    GetText.BindUObject(USFLoaderGraphNode, &UMaterialGraphNode_USFLoader::GetGeneratedCode);

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
    if (USFLoaderGraphNode) {
        return USFLoaderGraphNode->GetGeneratedCode();
    }
    return FText::FromString(TEXT("// No code generated"));
}

void SGraphNodeMaterialUSFLoader::CreateAdvancedViewArrow(TSharedPtr<SVerticalBox> MainBox) {
    if (!USFLoaderGraphNode) {
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
    if (USFLoaderGraphNode) {
        USFLoaderGraphNode->OnCodeViewChanged(NewCheckedState);
    }
}

ECheckBoxState SGraphNodeMaterialUSFLoader::IsAdvancedViewChecked() const {
    if (USFLoaderGraphNode) {
        return USFLoaderGraphNode->IsCodeViewChecked();
    }
    return ECheckBoxState::Unchecked;
}

#define CHEVRON_UP TEXT("Icons.ChevronUp")
#define CHEVRON_DOWN TEXT("Icons.ChevronDown")

const FSlateBrush* SGraphNodeMaterialUSFLoader::GetAdvancedViewArrow() const {
    if (!USFLoaderGraphNode) {
        return FAppStyle::GetBrush(CHEVRON_UP);
    }

    ECheckBoxState State = USFLoaderGraphNode->IsCodeViewChecked();
    return FAppStyle::GetBrush((State == ECheckBoxState::Checked) ? CHEVRON_UP : CHEVRON_DOWN);
}
