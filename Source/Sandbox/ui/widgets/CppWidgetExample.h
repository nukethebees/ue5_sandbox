#pragma once

#include <Blueprint/UserWidget.h>
#include <CoreMinimal.h>

#include "CppWidgetExample.generated.h"

class UBorder;
class UButton;
class UCheckBox;
class UEditableTextBox;
class UGridPanel;
class UImage;
class UOverlay;
class UProgressBar;
class URichTextBlock;
class UScrollBox;
class USizeBox;
class USlider;
class USpinBox;
class UTextBlock;
class UVerticalBox;

// An editor-preview example that builds its entire UMG hierarchy from C++.
UCLASS()
class SANDBOX_API UCppWidgetExample : public UUserWidget {
    GENERATED_BODY()
  public:
    void NativePreConstruct() override;
  protected:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preview")
    FText preview_title{FText::FromString(TEXT("C++ UMG Preview Example"))};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preview", meta = (MultiLine = true))
    FText preview_status{FText::FromString(TEXT("Edit these parent properties in the WBP Details panel to refresh this preview."))};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Preview",
              meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
    float preview_progress{0.65f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Preview",
              meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
    float preview_slider_value{0.35f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Preview",
              meta = (ClampMin = "0.0", ClampMax = "100.0", UIMin = "0.0", UIMax = "100.0"))
    float preview_numeric_value{42.f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preview")
    bool b_preview_checked{true};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preview")
    bool b_show_preview_image{true};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preview")
    FLinearColor accent_color{0.1f, 0.55f, 1.f, 1.f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preview")
    FMargin content_padding{16.f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Preview",
              meta = (ClampMin = "1.0", ClampMax = "100.0", UIMin = "1.0", UIMax = "100.0"))
    float progress_bar_height{18.f};
  private:
    void build_widget_tree();
    void apply_preview_properties();

    UPROPERTY(Transient)
    UScrollBox* root_scroll_box{nullptr};
    UPROPERTY(Transient)
    UBorder* content_border{nullptr};
    UPROPERTY(Transient)
    USizeBox* content_size_box{nullptr};
    UPROPERTY(Transient)
    UVerticalBox* content_vertical_box{nullptr};
    UPROPERTY(Transient)
    UTextBlock* title_text{nullptr};
    UPROPERTY(Transient)
    URichTextBlock* status_rich_text{nullptr};
    UPROPERTY(Transient)
    USizeBox* preview_overlay_size_box{nullptr};
    UPROPERTY(Transient)
    UOverlay* preview_overlay{nullptr};
    UPROPERTY(Transient)
    UImage* preview_image{nullptr};
    UPROPERTY(Transient)
    UTextBlock* overlay_text{nullptr};
    UPROPERTY(Transient)
    USizeBox* progress_bar_size_box{nullptr};
    UPROPERTY(Transient)
    UProgressBar* progress_bar{nullptr};
    UPROPERTY(Transient)
    UGridPanel* controls_grid{nullptr};
    UPROPERTY(Transient)
    USlider* preview_slider{nullptr};
    UPROPERTY(Transient)
    UCheckBox* preview_check_box{nullptr};
    UPROPERTY(Transient)
    UEditableTextBox* preview_editable_text{nullptr};
    UPROPERTY(Transient)
    USpinBox* preview_spin_box{nullptr};
    UPROPERTY(Transient)
    UButton* example_button{nullptr};
};
