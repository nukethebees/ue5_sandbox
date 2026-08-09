#include "SandboxEditor/ui/WidgetBlueprintGenerator.h"

#include "Sandbox/batch_game/TestBatchGameUiData.h"
#include "SandboxEditor/logging/SandboxEditorLogCategories.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/CompilerResultsLog.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "ObjectTools.h"
#include "ScopedTransaction.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"
#include "WidgetBlueprint.h"
#include "WidgetBlueprintOperationUtils.h"

#define LOG_ERR(FORMAT_STR, ...) UE_LOG(LogSandboxEditor, Error, TEXT(FORMAT_STR), ##__VA_ARGS__)
#define LOG_LOG(FORMAT_STR, ...) UE_LOG(LogSandboxEditor, Log, TEXT(FORMAT_STR), ##__VA_ARGS__)
#define LOG_WARN(FORMAT_STR, ...) UE_LOG(LogSandboxEditor, Warning, TEXT(FORMAT_STR), ##__VA_ARGS__)

namespace {
FName const generation_context{TEXT("SandboxWidgetBlueprintGenerator")};

bool is_project_widget_class(UClass const& widget_class) {
    auto const native_widget_classes{UTestBatchGameUiData::get_native_widget_classes()};
    auto const n_native_widget_classes{native_widget_classes.Num()};
    for (int32 i{0}; i < n_native_widget_classes; ++i) {
        if (native_widget_classes[i] == &widget_class) {
            return true;
        }
    }

    return false;
}

bool try_make_asset_paths(FWidgetBlueprintGenerationEntry const& entry,
                          FString& package_path,
                          FSoftObjectPath& object_path,
                          FText& error) {
    auto directory{entry.output_directory.Path.TrimStartAndEnd()};
    while (directory.RemoveFromEnd(TEXT("/"))) {}

    if (directory.IsEmpty()) {
        error = FText::FromString(TEXT("Output directory is empty."));
        return false;
    }

    if (entry.widget_name.IsNone()) {
        error = FText::FromString(TEXT("Widget name is empty."));
        return false;
    }

    auto const widget_name{entry.widget_name.ToString()};
    package_path = FString::Printf(TEXT("%s/%s"), *directory, *widget_name);

    if (!FPackageName::IsValidLongPackageName(package_path, false, &error)) {
        return false;
    }

    auto const object_path_string{FString::Printf(TEXT("%s.%s"), *package_path, *widget_name)};
    if (!FPackageName::IsValidObjectPath(object_path_string, &error)) {
        return false;
    }

    object_path = FSoftObjectPath{object_path_string};
    return true;
}

bool validate_entry(FWidgetBlueprintGenerationEntry const& entry,
                    int32 const entry_index,
                    FString& package_path,
                    FSoftObjectPath& object_path) {
    auto* const widget_class{entry.widget_class.Get()};
    if (widget_class == nullptr) {
        LOG_ERR("Widget generation row %d has no widget class.", entry_index);
        return false;
    }

    if (!widget_class->IsChildOf(UUserWidget::StaticClass())) {
        LOG_ERR("Widget generation row %d class '%s' does not derive from UUserWidget.",
                entry_index,
                *widget_class->GetPathName());
        return false;
    }

    FText path_error;
    if (!try_make_asset_paths(entry, package_path, object_path, path_error)) {
        LOG_ERR("Widget generation row %d has an invalid output path: %s",
                entry_index,
                *path_error.ToString());
        return false;
    }

    return true;
}

bool delete_existing_asset(FWidgetBlueprintGenerationEntry& entry,
                           int32 const entry_index,
                           FSoftObjectPath const& object_path) {
    auto& asset_registry{FAssetRegistryModule::GetRegistry()};
    auto const asset_data{asset_registry.GetAssetByObjectPath(object_path)};
    if (!asset_data.IsValid()) {
        LOG_LOG("Widget generation row %d has no existing asset at '%s'.",
                entry_index,
                *object_path.ToString());
        return true;
    }

    LOG_LOG("Widget generation row %d is deleting existing asset '%s'.",
            entry_index,
            *object_path.ToString());
    entry.existing_widget = nullptr;
    TArray<FAssetData> assets_to_delete{asset_data};
    auto const deleted_asset_count{ObjectTools::DeleteAssets(assets_to_delete, false)};
    if (deleted_asset_count != assets_to_delete.Num() ||
        asset_registry.GetAssetByObjectPath(object_path).IsValid()) {
        LOG_ERR("Widget generation row %d could not delete existing asset '%s'.",
                entry_index,
                *object_path.ToString());
        return false;
    }

    return true;
}

bool add_bind_widgets(UWidgetBlueprint& widget_blueprint,
                      UCanvasPanel& root_canvas,
                      UClass const& widget_class,
                      int32 const entry_index) {
    auto const widget_tree{widget_blueprint.WidgetTree};
    UTestBatchGameUiData* ui_data{nullptr};
    for (TFieldIterator<FProperty> property_iterator(&widget_class,
                                                     EFieldIteratorFlags::IncludeSuper);
         property_iterator;
         ++property_iterator) {
        auto* const property{*property_iterator};
        if (!property->HasMetaData(TEXT("BindWidget")) &&
            !property->HasMetaData(TEXT("BindWidgetOptional"))) {
            continue;
        }

        auto const property_name{property->GetFName()};
        auto const property_name_string{property_name.ToString()};

        auto* const object_property{CastField<FObjectPropertyBase>(property)};
        if (object_property == nullptr || object_property->PropertyClass == nullptr ||
            !object_property->PropertyClass->IsChildOf(UWidget::StaticClass())) {
            LOG_WARN("Widget generation row %d skipped binding '%s': property is not a UWidget "
                     "object property.",
                     entry_index,
                     *property_name_string);
            continue;
        }

        auto* const required_widget_class{object_property->PropertyClass.Get()};
        auto const is_project_widget{is_project_widget_class(*required_widget_class)};
        auto* widget_class_to_construct{required_widget_class};
        if (is_project_widget) {
            if (!IsValid(ui_data)) {
                ui_data = ml::test_batch_game_ui_data::get_data_asset();
            }

            if (!IsValid(ui_data)) {
                LOG_ERR("Widget generation row %d could not load the project UI data asset.",
                        entry_index);
                return false;
            }

            auto const mapped_widget_class{ui_data->get_widget_class(required_widget_class)};
            if (!mapped_widget_class) {
                LOG_ERR("Widget generation row %d requires a configured WBP mapping for "
                        "binding '%s' (%s).",
                        entry_index,
                        *property_name_string,
                        *required_widget_class->GetPathName());
                return false;
            }

            widget_class_to_construct = mapped_widget_class.Get();
        }

        if (widget_class_to_construct->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated)) {
            LOG_WARN("Widget generation row %d skipped binding '%s': widget class '%s' cannot "
                     "be instantiated.",
                     entry_index,
                     *property_name_string,
                     *widget_class_to_construct->GetPathName());
            if (is_project_widget) {
                return false;
            }
            continue;
        }

        auto* const generated_widget{
            widget_tree->ConstructWidget<UWidget>(widget_class_to_construct, property_name)};
        if (generated_widget == nullptr) {
            LOG_ERR("Widget generation row %d could not construct binding '%s' as '%s'.",
                    entry_index,
                    *property_name_string,
                    *widget_class_to_construct->GetPathName());
            if (is_project_widget) {
                return false;
            }
            continue;
        }

        if (root_canvas.AddChild(generated_widget) == nullptr) {
            LOG_WARN("Widget generation row %d could not attach binding '%s' to RootCanvas.",
                     entry_index,
                     *property_name_string);
            if (is_project_widget) {
                return false;
            }
            continue;
        }

        widget_blueprint.OnVariableAdded(property_name);
        LOG_LOG("Widget generation row %d added binding '%s' as '%s'.",
                entry_index,
                *property_name_string,
                *widget_class_to_construct->GetName());
    }

    return true;
}

UWidgetBlueprint* create_widget_blueprint(FWidgetBlueprintGenerationEntry const& entry,
                                          int32 const entry_index,
                                          FString const& package_path) {
    LOG_LOG(
        "Widget generation row %d is creating Widget Blueprint '%s'.", entry_index, *package_path);
    auto* const package{CreatePackage(*package_path)};
    if (package == nullptr) {
        LOG_ERR(
            "Widget generation row %d could not create package '%s'.", entry_index, *package_path);
        return nullptr;
    }

    auto* const widget_blueprint{
        FWidgetBlueprintOperationUtils::CreateWidgetBlueprint(package,
                                                              entry.widget_name,
                                                              BPTYPE_Normal,
                                                              entry.widget_class,
                                                              nullptr,
                                                              generation_context,
                                                              false)};
    auto* const widget_tree{widget_blueprint ? widget_blueprint->WidgetTree.Get() : nullptr};
    if (widget_tree == nullptr) {
        LOG_ERR("Widget generation row %d could not create Widget Blueprint '%s'.",
                entry_index,
                *package_path);
        return nullptr;
    }

    FAssetRegistryModule::AssetCreated(widget_blueprint);
    widget_blueprint->Modify();
    widget_tree->Modify();

    auto* const root_canvas{widget_tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(),
                                                                       TEXT("RootCanvas"))};
    if (root_canvas == nullptr) {
        LOG_ERR("Widget generation row %d could not create RootCanvas for '%s'.",
                entry_index,
                *package_path);
        package->MarkPackageDirty();
        return nullptr;
    }

    widget_tree->RootWidget = root_canvas;
    widget_blueprint->OnVariableAdded(root_canvas->GetFName());
    LOG_LOG("Widget generation row %d created RootCanvas.", entry_index);
    if (!add_bind_widgets(
            *widget_blueprint, *root_canvas, *entry.widget_class.Get(), entry_index)) {
        LOG_ERR("Widget generation row %d failed while adding BindWidget children.", entry_index);
        package->MarkPackageDirty();
        return nullptr;
    }

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(widget_blueprint);
    package->MarkPackageDirty();

    FCompilerResultsLog compiler_results;
    FKismetEditorUtilities::CompileBlueprint(
        widget_blueprint, EBlueprintCompileOptions::None, &compiler_results);
    package->MarkPackageDirty();

    if (compiler_results.NumErrors > 0 || widget_blueprint->Status == BS_Error) {
        LOG_ERR("Widget generation row %d failed to compile '%s' with %d error(s).",
                entry_index,
                *package_path,
                compiler_results.NumErrors);
        return nullptr;
    }

    LOG_LOG("Widget generation row %d compiled '%s' with %d warning(s).",
            entry_index,
            *package_path,
            compiler_results.NumWarnings);
    LOG_LOG("Generated Widget Blueprint '%s' from '%s'.",
            *package_path,
            *entry.widget_class->GetPathName());
    return widget_blueprint;
}
}

void UWidgetBlueprintGenerationDataAsset::generate_widgets() {
    int32 selected_widget_count{0};
    for (FWidgetBlueprintGenerationEntry const& entry : widgets) {
        if (entry.generate) {
            ++selected_widget_count;
        }
    }

    if (selected_widget_count == 0) {
        refresh_existing_widgets();
        LOG_WARN("Widget generation was requested, but no rows are selected.");
        return;
    }

    LOG_LOG("Preparing to generate %d Widget Blueprint row(s).", selected_widget_count);
    FScopedTransaction const transaction{
        NSLOCTEXT("SandboxEditor", "GenerateWidgetBlueprints", "Generate Widget Blueprints")};
    Modify();
    refresh_existing_widgets();

    auto const widget_count{widgets.Num()};
    for (int32 widget_index{0}; widget_index < widget_count; ++widget_index) {
        auto& entry{widgets[widget_index]};
        if (!entry.generate) {
            continue;
        }

        LOG_LOG("Preparing widget generation row %d for '%s'.",
                widget_index,
                *entry.widget_name.ToString());
        FString package_path;
        FSoftObjectPath object_path;
        if (!validate_entry(entry, widget_index, package_path, object_path)) {
            continue;
        }

        if (!delete_existing_asset(entry, widget_index, object_path)) {
            continue;
        }

        auto* const widget_blueprint{create_widget_blueprint(entry, widget_index, package_path)};
        if (widget_blueprint == nullptr) {
            continue;
        }

        entry.existing_widget = widget_blueprint;
        entry.generate = false;
        LOG_LOG("Widget generation row %d completed successfully.", widget_index);
    }

    refresh_existing_widgets();
    MarkPackageDirty();
    LOG_LOG("Widget Blueprint generation finished.");
}

void UWidgetBlueprintGenerationDataAsset::PostLoad() {
    Super::PostLoad();
    refresh_existing_widgets();
}

void UWidgetBlueprintGenerationDataAsset::PostEditChangeProperty(FPropertyChangedEvent& event) {
    Super::PostEditChangeProperty(event);

    if (event.GetMemberPropertyName() ==
        GET_MEMBER_NAME_CHECKED(UWidgetBlueprintGenerationDataAsset, widgets)) {
        fill_empty_output_directories();
    }

    refresh_existing_widgets();
}

void UWidgetBlueprintGenerationDataAsset::fill_empty_output_directories() {
    auto const package_directory{FPackageName::GetLongPackagePath(GetOutermost()->GetName())};
    if (package_directory.IsEmpty()) {
        return;
    }

    for (auto& entry : widgets) {
        if (entry.output_directory.Path.IsEmpty()) {
            entry.output_directory.Path = package_directory;
        }
    }
}

void UWidgetBlueprintGenerationDataAsset::refresh_existing_widgets() {
    auto& asset_registry{FAssetRegistryModule::GetRegistry()};
    for (auto& entry : widgets) {
        FString package_path;
        FSoftObjectPath object_path;
        FText path_error;
        if (!try_make_asset_paths(entry, package_path, object_path, path_error)) {
            entry.existing_widget = nullptr;
            continue;
        }

        auto const asset_data{asset_registry.GetAssetByObjectPath(object_path)};
        entry.existing_widget =
            asset_data.IsValid() ? Cast<UWidgetBlueprint>(asset_data.GetAsset()) : nullptr;
    }
}

#undef LOG_WARN
#undef LOG_LOG
#undef LOG_ERR
