#include "S7LabEditor/S7LabWorkbench.h"

#include "S7Lab/Interpreter.h"

#include "HAL/FileManager.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SListView.h"

namespace {
struct FExampleScript {
    FString name;
    FString path;
    FString source;
};

class SS7LabWorkbench final : public SCompoundWidget {
  public:
    SLATE_BEGIN_ARGS(SS7LabWorkbench) {}
    SLATE_END_ARGS()

    void Construct(FArguments const&) {
        load_scripts();

        ChildSlot
            [SNew(SBorder)
                 .BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
                 .Padding(12.0f)
                     [SNew(SVerticalBox) +
                      SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
                          [SNew(STextBlock)
                               .Text(NSLOCTEXT("S7LabWorkbench", "Title", "s7 Scheme Workbench"))
                               .Font(FAppStyle::Get().GetFontStyle("HeadingExtraSmall"))] +
                      SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 10.0f)
                          [SNew(STextBlock)
                               .Text(NSLOCTEXT("S7LabWorkbench",
                                               "Description",
                                               "Select an example, inspect its source, "
                                               "and evaluate it in an embedded s7 "
                                               "interpreter."))] +
                      SVerticalBox::Slot().FillHeight(1.0f)[make_content()] +
                      SVerticalBox::Slot().AutoHeight().Padding(0.0f, 10.0f, 0.0f, 0.0f)
                          [SNew(SButton)
                               .Text(
                                   NSLOCTEXT("S7LabWorkbench", "RunScript", "Run selected script"))
                               .IsEnabled(this, &SS7LabWorkbench::has_selection)
                               .OnClicked(this, &SS7LabWorkbench::run_script)]]];

        if (!scripts_.IsEmpty()) {
            script_list_->SetSelection(scripts_[0]);
        } else {
            source_->SetText(NSLOCTEXT("S7LabWorkbench",
                                       "NoScripts",
                                       "No .scm files were found in the plugin's "
                                       "Scripts/Examples directory."));
        }
    }
  private:
    auto make_content() -> TSharedRef<SWidget> {
        return SNew(SSplitter) +
               SSplitter::Slot().Value(0.25f)
                   [SNew(SBorder)
                        .BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
                        .Padding(
                            4.0f)[SAssignNew(script_list_, SListView<TSharedPtr<FExampleScript>>)
                                      .ListItemsSource(&scripts_)
                                      .SelectionMode(ESelectionMode::Single)
                                      .OnGenerateRow(this, &SS7LabWorkbench::make_script_row)
                                      .OnSelectionChanged(this, &SS7LabWorkbench::select_script)]] +
               SSplitter::Slot().Value(
                   0.75f)[SNew(SSplitter).Orientation(Orient_Vertical) +
                          SSplitter::Slot().Value(0.65f)[make_text_panel(
                              NSLOCTEXT("S7LabWorkbench", "Source", "Scheme source"), source_)] +
                          SSplitter::Slot().Value(0.35f)[make_text_panel(
                              NSLOCTEXT("S7LabWorkbench", "Output", "Evaluation output"),
                              output_)]];
    }

    auto make_text_panel(FText const& heading, TSharedPtr<SMultiLineEditableTextBox>& text_box)
        -> TSharedRef<SWidget> {
        return SNew(SBorder)
            .BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
            .Padding(6.0f)[SNew(SVerticalBox) +
                           SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
                               [SNew(STextBlock)
                                    .Text(heading)
                                    .Font(FAppStyle::Get().GetFontStyle("HeadingExtraSmall"))] +
                           SVerticalBox::Slot().FillHeight(
                               1.0f)[SAssignNew(text_box, SMultiLineEditableTextBox)
                                         .IsReadOnly(true)
                                         .AlwaysShowScrollbars(true)]];
    }

    void load_scripts() {
        auto const plugin{IPluginManager::Get().FindPlugin(TEXT("SbxLangLab"))};
        if (!plugin.IsValid()) {
            UE_LOG(LogTemp, Error, TEXT("S7Lab workbench could not find the SbxLangLab plugin."));
            return;
        }

        auto const script_directory{
            FPaths::Combine(plugin->GetBaseDir(), TEXT("Scripts"), TEXT("Examples"))};
        TArray<FString> filenames;
        IFileManager::Get().FindFiles(
            filenames, *FPaths::Combine(script_directory, TEXT("*.scm")), true, false);
        filenames.Sort();

        for (FString const& filename : filenames) {
            auto script{MakeShared<FExampleScript>()};
            script->name = FPaths::GetBaseFilename(filename);
            script->path = FPaths::Combine(script_directory, filename);
            if (FFileHelper::LoadFileToString(script->source, *script->path)) {
                scripts_.Add(MoveTemp(script));
            } else {
                UE_LOG(LogTemp,
                       Error,
                       TEXT("S7Lab workbench could not read script '%s'."),
                       *script->path);
            }
        }
    }

    auto make_script_row(TSharedPtr<FExampleScript> script,
                         TSharedRef<STableViewBase> const& owner) const -> TSharedRef<ITableRow> {
        return SNew(STableRow<TSharedPtr<FExampleScript>>,
                    owner)[SNew(STextBlock).Text(FText::FromString(script->name))];
    }

    void select_script(TSharedPtr<FExampleScript> script, ESelectInfo::Type) {
        selected_script_ = MoveTemp(script);
        source_->SetText(selected_script_.IsValid() ? FText::FromString(selected_script_->source)
                                                    : FText::GetEmpty());
        output_->SetText(FText::GetEmpty());
    }

    [[nodiscard]] auto has_selection() const -> bool { return selected_script_.IsValid(); }

    auto run_script() -> FReply {
        if (!selected_script_.IsValid()) {
            return FReply::Handled();
        }

        auto const result{interpreter_.evaluate(selected_script_->source)};
        auto const heading{result.succeeded ? TEXT("Result") : TEXT("Error")};
        auto const detail{result.succeeded ? result.value : result.error};
        output_->SetText(FText::FromString(
            FString::Printf(TEXT("%s: %s\n\n%s"), heading, *selected_script_->name, *detail)));
        return FReply::Handled();
    }

    S7Lab::FInterpreter interpreter_;
    TArray<TSharedPtr<FExampleScript>> scripts_;
    TSharedPtr<FExampleScript> selected_script_;
    TSharedPtr<SListView<TSharedPtr<FExampleScript>>> script_list_;
    TSharedPtr<SMultiLineEditableTextBox> source_;
    TSharedPtr<SMultiLineEditableTextBox> output_;
};
}

US7LabWorkbench::US7LabWorkbench() {
    TabDisplayName = NSLOCTEXT("S7LabWorkbench", "TabName", "s7 Scheme Workbench");
    bAlwaysReregisterWithWindowsMenu = true;
}

TSharedRef<SWidget> US7LabWorkbench::RebuildWidget() {
    return SNew(SS7LabWorkbench);
}
