#include "Sandbox/ui/widgets/CppWidgetExample.h"

#include <Blueprint/WidgetTree.h>
#include <Components/Border.h>
#include <Components/Button.h>
#include <Components/CheckBox.h>
#include <Components/EditableTextBox.h>
#include <Components/GridPanel.h>
#include <Components/GridSlot.h>
#include <Components/HorizontalBox.h>
#include <Components/HorizontalBoxSlot.h>
#include <Components/Image.h>
#include <Components/Overlay.h>
#include <Components/OverlaySlot.h>
#include <Components/ProgressBar.h>
#include <Components/RichTextBlock.h>
#include <Components/ScrollBox.h>
#include <Components/ScrollBoxSlot.h>
#include <Components/SizeBox.h>
#include <Components/Slider.h>
#include <Components/SpinBox.h>
#include <Components/TextBlock.h>
#include <Components/VerticalBox.h>
#include <Components/VerticalBoxSlot.h>

void UCppWidgetExample::NativePreConstruct() {
    Super::NativePreConstruct();

    if (!WidgetTree->RootWidget) {
        build_widget_tree();
    }

    apply_preview_properties();
}

void UCppWidgetExample::build_widget_tree() {
    check(WidgetTree);
    check(!WidgetTree->RootWidget);

    root_scroll_box =
        WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("root_scroll_box"));
    WidgetTree->RootWidget = root_scroll_box;

    content_border =
        WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("content_border"));
    auto* const scroll_slot{root_scroll_box->AddChild(content_border)};
    check(scroll_slot);

    content_size_box =
        WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("content_size_box"));
    content_size_box->SetWidthOverride(520.f);
    content_border->SetContent(content_size_box);

    content_vertical_box = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(),
                                                                     TEXT("content_vertical_box"));
    content_size_box->SetContent(content_vertical_box);

    title_text =
        WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("title_text"));
    auto* const title_slot{content_vertical_box->AddChildToVerticalBox(title_text)};
    check(title_slot);
    title_slot->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
    title_slot->SetHorizontalAlignment(HAlign_Center);

    status_rich_text = WidgetTree->ConstructWidget<URichTextBlock>(URichTextBlock::StaticClass(),
                                                                   TEXT("status_rich_text"));
    auto* const status_slot{content_vertical_box->AddChildToVerticalBox(status_rich_text)};
    check(status_slot);
    status_slot->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));
    status_slot->SetHorizontalAlignment(HAlign_Fill);

    preview_overlay_size_box = WidgetTree->ConstructWidget<USizeBox>(
        USizeBox::StaticClass(), TEXT("preview_overlay_size_box"));
    preview_overlay_size_box->SetHeightOverride(84.f);
    auto* const overlay_size_slot{
        content_vertical_box->AddChildToVerticalBox(preview_overlay_size_box)};
    check(overlay_size_slot);
    overlay_size_slot->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));
    overlay_size_slot->SetHorizontalAlignment(HAlign_Fill);

    preview_overlay =
        WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("preview_overlay"));
    preview_overlay_size_box->SetContent(preview_overlay);

    preview_image =
        WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("preview_image"));
    auto* const image_slot{preview_overlay->AddChildToOverlay(preview_image)};
    check(image_slot);
    image_slot->SetHorizontalAlignment(HAlign_Fill);
    image_slot->SetVerticalAlignment(VAlign_Fill);

    overlay_text =
        WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("overlay_text"));
    overlay_text->SetText(FText::FromString(TEXT("UOverlay: text over an image")));
    auto* const overlay_text_slot{preview_overlay->AddChildToOverlay(overlay_text)};
    check(overlay_text_slot);
    overlay_text_slot->SetPadding(FMargin(12.f));
    overlay_text_slot->SetHorizontalAlignment(HAlign_Center);
    overlay_text_slot->SetVerticalAlignment(VAlign_Center);

    progress_bar_size_box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(),
                                                                  TEXT("progress_bar_size_box"));
    auto* const progress_size_slot{
        content_vertical_box->AddChildToVerticalBox(progress_bar_size_box)};
    check(progress_size_slot);
    progress_size_slot->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));
    progress_size_slot->SetHorizontalAlignment(HAlign_Fill);

    progress_bar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(),
                                                             TEXT("progress_bar"));
    progress_bar_size_box->SetContent(progress_bar);

    controls_grid =
        WidgetTree->ConstructWidget<UGridPanel>(UGridPanel::StaticClass(), TEXT("controls_grid"));
    controls_grid->SetColumnFill(0, 0.35f);
    controls_grid->SetColumnFill(1, 0.65f);
    auto* const grid_slot{content_vertical_box->AddChildToVerticalBox(controls_grid)};
    check(grid_slot);
    grid_slot->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));
    grid_slot->SetHorizontalAlignment(HAlign_Fill);

    auto add_grid_label{[this](FName const name, FText const& text, int32 const row) {
        auto* const label{WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), name)};
        label->SetText(text);
        auto* const slot{controls_grid->AddChildToGrid(label, row, 0)};
        check(slot);
        slot->SetPadding(FMargin(0.f, 4.f, 8.f, 4.f));
        slot->SetHorizontalAlignment(HAlign_Right);
        slot->SetVerticalAlignment(VAlign_Center);
    }};

    add_grid_label(TEXT("slider_label"), FText::FromString(TEXT("Slider")), 0);
    preview_slider =
        WidgetTree->ConstructWidget<USlider>(USlider::StaticClass(), TEXT("preview_slider"));
    preview_slider->SetMinValue(0.f);
    preview_slider->SetMaxValue(1.f);
    auto* const slider_slot{controls_grid->AddChildToGrid(preview_slider, 0, 1)};
    check(slider_slot);
    slider_slot->SetPadding(FMargin(0.f, 4.f));
    slider_slot->SetHorizontalAlignment(HAlign_Fill);
    slider_slot->SetVerticalAlignment(VAlign_Center);

    add_grid_label(TEXT("check_box_label"), FText::FromString(TEXT("Checked")), 1);
    preview_check_box =
        WidgetTree->ConstructWidget<UCheckBox>(UCheckBox::StaticClass(), TEXT("preview_check_box"));
    auto* const check_box_slot{controls_grid->AddChildToGrid(preview_check_box, 1, 1)};
    check(check_box_slot);
    check_box_slot->SetPadding(FMargin(0.f, 4.f));
    check_box_slot->SetHorizontalAlignment(HAlign_Left);
    check_box_slot->SetVerticalAlignment(VAlign_Center);

    add_grid_label(TEXT("editable_text_label"), FText::FromString(TEXT("Editable text")), 2);
    preview_editable_text = WidgetTree->ConstructWidget<UEditableTextBox>(
        UEditableTextBox::StaticClass(), TEXT("preview_editable_text"));
    auto* const editable_text_slot{controls_grid->AddChildToGrid(preview_editable_text, 2, 1)};
    check(editable_text_slot);
    editable_text_slot->SetPadding(FMargin(0.f, 4.f));
    editable_text_slot->SetHorizontalAlignment(HAlign_Fill);
    editable_text_slot->SetVerticalAlignment(VAlign_Center);

    add_grid_label(TEXT("spin_box_label"), FText::FromString(TEXT("Numeric value")), 3);
    preview_spin_box =
        WidgetTree->ConstructWidget<USpinBox>(USpinBox::StaticClass(), TEXT("preview_spin_box"));
    preview_spin_box->SetMinValue(0.f);
    preview_spin_box->SetMaxValue(100.f);
    auto* const spin_box_slot{controls_grid->AddChildToGrid(preview_spin_box, 3, 1)};
    check(spin_box_slot);
    spin_box_slot->SetPadding(FMargin(0.f, 4.f));
    spin_box_slot->SetHorizontalAlignment(HAlign_Fill);
    spin_box_slot->SetVerticalAlignment(VAlign_Center);

    auto* const button_row = WidgetTree->ConstructWidget<UHorizontalBox>(
        UHorizontalBox::StaticClass(), TEXT("button_row"));
    auto* const button_row_slot{content_vertical_box->AddChildToVerticalBox(button_row)};
    check(button_row_slot);
    button_row_slot->SetHorizontalAlignment(HAlign_Center);

    auto* const button_label{
        WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("button_label"))};
    button_label->SetText(FText::FromString(TEXT("Button")));
    auto* const button_label_slot{button_row->AddChildToHorizontalBox(button_label)};
    check(button_label_slot);
    button_label_slot->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));
    button_label_slot->SetVerticalAlignment(VAlign_Center);
    button_label_slot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));

    example_button =
        WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("example_button"));
    auto* const button_slot{button_row->AddChildToHorizontalBox(example_button)};
    check(button_slot);
    button_slot->SetVerticalAlignment(VAlign_Center);
    button_slot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));

    auto* const button_text{
        WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("button_text"))};
    button_text->SetText(FText::FromString(TEXT("Example button")));
    example_button->SetContent(button_text);
}

void UCppWidgetExample::apply_preview_properties() {
    checkf(title_text && status_rich_text && content_border && preview_image && overlay_text &&
               progress_bar_size_box && progress_bar && preview_slider && preview_check_box &&
               preview_editable_text && preview_spin_box,
           TEXT("UCppWidgetExample: Widget tree was not constructed."));

    title_text->SetText(preview_title);
    status_rich_text->SetText(preview_status);
    content_border->SetPadding(content_padding);
    content_border->SetBrushColor(accent_color.CopyWithNewOpacity(0.18f));

    preview_image->SetColorAndOpacity(accent_color.CopyWithNewOpacity(0.45f));
    preview_image->SetVisibility(b_show_preview_image ? ESlateVisibility::Visible
                                                      : ESlateVisibility::Collapsed);
    overlay_text->SetColorAndOpacity(FLinearColor::White);

    progress_bar_size_box->SetHeightOverride(progress_bar_height);
    progress_bar->SetPercent(preview_progress);
    progress_bar->SetFillColorAndOpacity(accent_color);

    preview_slider->SetValue(preview_slider_value);
    preview_check_box->SetIsChecked(b_preview_checked);
    preview_editable_text->SetText(preview_status);
    preview_spin_box->SetValue(preview_numeric_value);
}
