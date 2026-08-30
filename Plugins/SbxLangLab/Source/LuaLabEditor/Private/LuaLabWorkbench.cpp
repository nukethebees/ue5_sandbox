#include "LuaLabEditor/LuaLabWorkbench.h"

#include "LuaLab/Interpreter.h"

#include "HAL/FileManager.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
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

class SLuaLabWorkbench final : public SCompoundWidget {
  public:
    SLATE_BEGIN_ARGS(SLuaLabWorkbench) {}
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
                               .Text(NSLOCTEXT("LuaLabWorkbench", "Title", "Lua 5.5 Workbench"))
                               .Font(FAppStyle::Get().GetFontStyle("HeadingExtraSmall"))] +
                      SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 10.0f)
                          [SNew(STextBlock)
                               .Text(NSLOCTEXT("LuaLabWorkbench",
                                               "Description",
                                               "Select an example, inspect its source, "
                                               "and evaluate it in an embedded Lua "
                                               "interpreter."))] +
                      SVerticalBox::Slot().FillHeight(1.0f)[make_content()] +
                      SVerticalBox::Slot().AutoHeight().Padding(0.0f, 10.0f, 0.0f, 0.0f)
                          [SNew(SButton)
                               .Text(
                                   NSLOCTEXT("LuaLabWorkbench", "RunScript", "Run selected script"))
                               .IsEnabled(this, &SLuaLabWorkbench::has_selection)
                               .OnClicked(this, &SLuaLabWorkbench::run_script)]]];

        if (!scripts_.IsEmpty()) {
            script_list_->SetSelection(scripts_[0]);
        } else {
            source_->SetText(NSLOCTEXT("LuaLabWorkbench",
                                       "NoScripts",
                                       "No .lua files were found in the plugin's "
                                       "Scripts/LuaExamples directory."));
        }
    }
  private:
    auto make_content() -> TSharedRef<SWidget> {
        return SNew(SSplitter) +
               SSplitter::Slot().Value(
                   0.25f)[SNew(SBorder)
                              .BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
                              .Padding(4.0f)
                                  [SAssignNew(script_list_, SListView<TSharedPtr<FExampleScript>>)
                                       .ListItemsSource(&scripts_)
                                       .SelectionMode(ESelectionMode::Single)
                                       .OnGenerateRow(this, &SLuaLabWorkbench::make_script_row)
                                       .OnSelectionChanged(this,
                                                           &SLuaLabWorkbench::select_script)]] +
               SSplitter::Slot().Value(
                   0.75f)[SNew(SSplitter).Orientation(Orient_Vertical) +
                          SSplitter::Slot().Value(0.65f)[make_text_panel(
                              NSLOCTEXT("LuaLabWorkbench", "Source", "Lua source"), source_)] +
                          SSplitter::Slot().Value(0.35f)[make_text_panel(
                              NSLOCTEXT("LuaLabWorkbench", "Output", "Evaluation output"),
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
            UE_LOG(LogTemp, Error, TEXT("LuaLab workbench could not find the SbxLangLab plugin."));
            return;
        }

        auto const script_directory{
            FPaths::Combine(plugin->GetBaseDir(), TEXT("Scripts"), TEXT("LuaExamples"))};
        TArray<FString> filenames;
        IFileManager::Get().FindFiles(
            filenames, *FPaths::Combine(script_directory, TEXT("*.lua")), true, false);
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
                       TEXT("LuaLab workbench could not read script '%s'."),
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

    LuaLab::FInterpreter interpreter_;
    TArray<TSharedPtr<FExampleScript>> scripts_;
    TSharedPtr<FExampleScript> selected_script_;
    TSharedPtr<SListView<TSharedPtr<FExampleScript>>> script_list_;
    TSharedPtr<SMultiLineEditableTextBox> source_;
    TSharedPtr<SMultiLineEditableTextBox> output_;
};
}

ULuaLabWorkbench::ULuaLabWorkbench() {
    TabDisplayName = NSLOCTEXT("LuaLabWorkbench", "TabName", "Lua 5.5 Workbench");
    bAlwaysReregisterWithWindowsMenu = true;
}

TSharedRef<SWidget> ULuaLabWorkbench::RebuildWidget() {
    return SNew(SLuaLabWorkbench);
}
