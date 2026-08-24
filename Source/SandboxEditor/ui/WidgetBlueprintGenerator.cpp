#include "SandboxEditor/ui/WidgetBlueprintGenerator.h"

#include "Sandbox/batch_game/TestBatchGameUiData.h"
#include "SandboxEditor/logging/SandboxEditorLogCategories.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Blueprint/WidgetTree.h"
#include "BlueprintEditorLibrary.h"
#include "Components/CanvasPanel.h"
#include "Components/PanelWidget.h"
#include "Misc/PackageName.h"
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
    auto directory{entry.output_directory.Path};
    directory.TrimStartAndEndInline();
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
                    FSoftObjectPath& object_path,
                    FProperty*& generator_root_property) {
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

    generator_root_property = nullptr;
    auto const widget_class_name{widget_class->GetPathName()};
    for (TFieldIterator<FProperty> property_iterator(widget_class,
                                                     EFieldIteratorFlags::IncludeSuper);
         property_iterator;
         ++property_iterator) {
        auto* const property{*property_iterator};
        if (!property->HasMetaData(TEXT("GeneratorRoot"))) {
            continue;
        }

        auto const property_name{property->GetName()};
        if (generator_root_property != nullptr) {
            LOG_ERR("Cannot generate '%s': properties '%s' and '%s' are both marked "
                    "GeneratorRoot.",
                    *widget_class_name,
                    *generator_root_property->GetName(),
                    *property_name);
            return false;
        }

        if (!property->HasMetaData(TEXT("BindWidget"))) {
            LOG_ERR("Cannot generate '%s': property '%s' is marked GeneratorRoot but is not "
                    "a BindWidget property.",
                    *widget_class_name,
                    *property_name);
            return false;
        }

        auto* const object_property{CastField<FObjectPropertyBase>(property)};
        auto* const property_class{object_property != nullptr ? object_property->PropertyClass.Get()
                                                              : nullptr};
        if (property_class == nullptr || !property_class->IsChildOf(UPanelWidget::StaticClass())) {
            auto const property_class_name{property_class != nullptr ? property_class->GetPathName()
                                                                     : TEXT("non-widget property")};
            LOG_ERR("Cannot generate '%s': property '%s' is marked GeneratorRoot but has type "
                    "'%s'. GeneratorRoot properties must derive from UPanelWidget.",
                    *widget_class_name,
                    *property_name,
                    *property_class_name);
            return false;
        }

        generator_root_property = property;
    }

    UTestBatchGameUiData* ui_data{nullptr};
    for (TFieldIterator<FProperty> property_iterator(widget_class,
                                                     EFieldIteratorFlags::IncludeSuper);
         property_iterator;
         ++property_iterator) {
        auto* const property{*property_iterator};
        if (!property->HasMetaData(TEXT("BindWidget")) &&
            !property->HasMetaData(TEXT("BindWidgetOptional"))) {
            continue;
        }

        auto* const object_property{CastField<FObjectPropertyBase>(property)};
        auto* const property_class{object_property != nullptr ? object_property->PropertyClass.Get()
                                                              : nullptr};
        if (property_class == nullptr || !property_class->IsChildOf(UWidget::StaticClass()) ||
            !is_project_widget_class(*property_class)) {
            continue;
        }

        if (!IsValid(ui_data)) {
            ui_data = ml::test_batch_game_ui_data::get_data_asset();
        }

        if (!IsValid(ui_data)) {
            LOG_ERR("Cannot generate '%s': binding '%s' requires the project UI data asset, "
                    "but it could not be loaded.",
                    *widget_class_name,
                    *property->GetName());
            return false;
        }

        if (!ui_data->get_widget_class(property_class)) {
            LOG_ERR("Cannot generate '%s': binding '%s' requires a valid configured WBP "
                    "mapping for '%s'.",
                    *widget_class_name,
                    *property->GetName(),
                    *property_class->GetPathName());
            return false;
        }
    }

    return true;
}

enum class EBindWidgetConstructionResult {
    constructed,
    skipped,
    failed,
};

EBindWidgetConstructionResult construct_bind_widget(UWidgetTree& widget_tree,
                                                    FProperty& property,
                                                    int32 const entry_index,
                                                    UTestBatchGameUiData*& ui_data,
                                                    UWidget*& generated_widget) {
    generated_widget = nullptr;
    auto const property_name{property.GetFName()};
    auto const property_name_string{property_name.ToString()};
    auto* const object_property{CastField<FObjectPropertyBase>(&property)};
    if (object_property == nullptr || object_property->PropertyClass == nullptr ||
        !object_property->PropertyClass->IsChildOf(UWidget::StaticClass())) {
        LOG_WARN("Widget generation row %d skipped binding '%s': property is not a UWidget "
                 "object property.",
                 entry_index,
                 *property_name_string);
        return EBindWidgetConstructionResult::skipped;
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
            return EBindWidgetConstructionResult::failed;
        }

        auto const mapped_widget_class{ui_data->get_widget_class(required_widget_class)};
        if (!mapped_widget_class) {
            LOG_ERR("Widget generation row %d requires a configured WBP mapping for binding "
                    "'%s' (%s).",
                    entry_index,
                    *property_name_string,
                    *required_widget_class->GetPathName());
            return EBindWidgetConstructionResult::failed;
        }

        widget_class_to_construct = mapped_widget_class.Get();
    }

    if (widget_class_to_construct->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated)) {
        LOG_WARN("Widget generation row %d skipped binding '%s': widget class '%s' cannot be "
                 "instantiated.",
                 entry_index,
                 *property_name_string,
                 *widget_class_to_construct->GetPathName());
        return is_project_widget ? EBindWidgetConstructionResult::failed
                                 : EBindWidgetConstructionResult::skipped;
    }

    generated_widget =
        widget_tree.ConstructWidget<UWidget>(widget_class_to_construct, property_name);
    if (generated_widget == nullptr) {
        LOG_ERR("Widget generation row %d could not construct binding '%s' as '%s'.",
                entry_index,
                *property_name_string,
                *widget_class_to_construct->GetPathName());
        return is_project_widget ? EBindWidgetConstructionResult::failed
                                 : EBindWidgetConstructionResult::skipped;
    }

    generated_widget->bIsVariable = true;
    return EBindWidgetConstructionResult::constructed;
}

bool add_bind_widgets(UWidgetBlueprint& widget_blueprint,
                      UPanelWidget& root_widget,
                      UClass const& widget_class,
                      int32 const entry_index,
                      FProperty const* const generator_root_property) {
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

        if (property == generator_root_property) {
            continue;
        }

        UWidget* generated_widget{nullptr};
        auto const construction_result{
            construct_bind_widget(*widget_tree, *property, entry_index, ui_data, generated_widget)};
        if (construction_result == EBindWidgetConstructionResult::failed) {
            return false;
        }

        if (construction_result == EBindWidgetConstructionResult::skipped) {
            continue;
        }

        auto const property_name{property->GetFName()};
        auto const property_name_string{property_name.ToString()};
        auto* const object_property{CastFieldChecked<FObjectPropertyBase>(property)};
        auto* const property_class{object_property->PropertyClass.Get()};
        auto const is_project_widget{is_project_widget_class(*property_class)};
        if (root_widget.AddChild(generated_widget) == nullptr) {
            LOG_WARN("Widget generation row %d could not attach binding '%s' to root '%s'.",
                     entry_index,
                     *property_name_string,
                     *root_widget.GetName());
            if (is_project_widget) {
                return false;
            }
            continue;
        }

        widget_blueprint.OnVariableAdded(property_name);
        LOG_LOG("Widget generation row %d added binding '%s' as '%s'.",
                entry_index,
                *property_name_string,
                *generated_widget->GetClass()->GetName());
    }

    return true;
}

UWidgetBlueprint* create_widget_blueprint(FWidgetBlueprintGenerationEntry const& entry,
                                          int32 const entry_index,
                                          FString const& package_path,
                                          FProperty* const generator_root_property) {
    auto* const requested_widget_class{entry.widget_class.Get()};
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
                                                              UUserWidget::StaticClass(),
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

    widget_blueprint->Modify();
    widget_tree->Modify();

    UPanelWidget* root_widget{nullptr};
    if (generator_root_property != nullptr) {
        UTestBatchGameUiData* ui_data{nullptr};
        UWidget* generated_root_widget{nullptr};
        auto const construction_result{construct_bind_widget(
            *widget_tree, *generator_root_property, entry_index, ui_data, generated_root_widget)};
        root_widget = Cast<UPanelWidget>(generated_root_widget);
        if (construction_result != EBindWidgetConstructionResult::constructed ||
            root_widget == nullptr) {
            LOG_ERR("Cannot generate '%s': GeneratorRoot property '%s' could not be "
                    "constructed as a UPanelWidget.",
                    *requested_widget_class->GetPathName(),
                    *generator_root_property->GetName());
            package->MarkPackageDirty();
            return nullptr;
        }

        widget_tree->RootWidget = root_widget;
        widget_blueprint->OnVariableAdded(generator_root_property->GetFName());
        LOG_LOG("Widget generation row %d created GeneratorRoot '%s' as '%s'.",
                entry_index,
                *generator_root_property->GetName(),
                *root_widget->GetClass()->GetName());
    } else {
        auto* const root_canvas{widget_tree->ConstructWidget<UCanvasPanel>(
            UCanvasPanel::StaticClass(), TEXT("RootCanvas"))};
        if (root_canvas == nullptr) {
            LOG_ERR("Widget generation row %d could not create RootCanvas for '%s'.",
                    entry_index,
                    *package_path);
            package->MarkPackageDirty();
            return nullptr;
        }

        root_widget = root_canvas;
        widget_tree->RootWidget = root_widget;
        widget_blueprint->OnVariableAdded(root_widget->GetFName());
        LOG_LOG("Widget generation row %d created RootCanvas.", entry_index);
    }

    if (!add_bind_widgets(*widget_blueprint,
                          *root_widget,
                          *requested_widget_class,
                          entry_index,
                          generator_root_property)) {
        LOG_ERR("Widget generation row %d failed while adding BindWidget children.", entry_index);
        package->MarkPackageDirty();
        return nullptr;
    }

    LOG_LOG("Widget generation row %d is reparenting '%s' to '%s'.",
            entry_index,
            *package_path,
            *requested_widget_class->GetPathName());
    UBlueprintEditorLibrary::ReparentBlueprint(widget_blueprint, requested_widget_class);

    auto* const generated_class{widget_blueprint->GeneratedClass.Get()};
    if (widget_blueprint->ParentClass != requested_widget_class || generated_class == nullptr ||
        !generated_class->IsChildOf(requested_widget_class) ||
        widget_blueprint->Status == BS_Error) {
        LOG_ERR("Widget generation row %d failed to reparent and compile '%s' as '%s'.",
                entry_index,
                *package_path,
                *requested_widget_class->GetPathName());
        package->MarkPackageDirty();
        return nullptr;
    }

    FAssetRegistryModule::AssetCreated(widget_blueprint);
    package->MarkPackageDirty();
    LOG_LOG("Widget generation row %d reparented and compiled '%s'.", entry_index, *package_path);
    LOG_LOG("Generated Widget Blueprint '%s' from '%s'.",
            *package_path,
            *requested_widget_class->GetPathName());
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
        LOG_WARN("Widget generation was requested, but no rows are selected.");
        return;
    }

    LOG_LOG("Preparing to generate %d Widget Blueprint row(s).", selected_widget_count);

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
        FProperty* generator_root_property{nullptr};
        if (!validate_entry(
                entry, widget_index, package_path, object_path, generator_root_property)) {
            continue;
        }

        if (FAssetRegistryModule::GetRegistry().GetAssetByObjectPath(object_path).IsValid()) {
            LOG_ERR("Widget generation row %d cannot create '%s' because the asset already "
                    "exists. Delete the existing asset before generating it again.",
                    widget_index,
                    *object_path.ToString());
            continue;
        }

        auto* const widget_blueprint{
            create_widget_blueprint(entry, widget_index, package_path, generator_root_property)};
        if (widget_blueprint == nullptr) {
            continue;
        }

        Modify();
        entry.generate = false;
        LOG_LOG("Widget generation row %d completed successfully.", widget_index);
    }

    MarkPackageDirty();
    LOG_LOG("Widget Blueprint generation finished.");
}

void UWidgetBlueprintGenerationDataAsset::PostEditChangeProperty(FPropertyChangedEvent& event) {
    Super::PostEditChangeProperty(event);

    if (event.GetMemberPropertyName() ==
        GET_MEMBER_NAME_CHECKED(UWidgetBlueprintGenerationDataAsset, widgets)) {
        fill_empty_output_directories();
    }
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

#undef LOG_WARN
#undef LOG_LOG
#undef LOG_ERR
