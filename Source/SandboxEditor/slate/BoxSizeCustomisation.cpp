#include "BoxSizeCustomisation.h"

#include "SandboxEditor/logging/SandboxEditorLogCategories.h"
#include "SandboxEditor/slate/BoxSizeEditMode.h"

#include "Sandbox/utilities/BoxSize.h"

#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SVectorInputBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"

void FBoxSizeCustomisation::CustomizeHeader(TSharedRef<IPropertyHandle> prop_handle,
                                            FDetailWidgetRow& header_row,
                                            IPropertyTypeCustomizationUtils& customisation_utils) {
    // clang-format off
    header_row
        .WholeRowContent()
        [
            prop_handle->CreatePropertyNameWidget()
        ];
    // clang-format on
}

void
    FBoxSizeCustomisation::CustomizeChildren(TSharedRef<IPropertyHandle> prop_handle,
                                             IDetailChildrenBuilder& child_builder,
                                             IPropertyTypeCustomizationUtils& customisation_utils) {

    if (!prop_handle->IsValidHandle()) {
        return;
    }

    property_handle = prop_handle;
    box_size_handle = property_handle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FBoxSize, box_size));
    x_handle = box_size_handle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FVector, X));
    y_handle = box_size_handle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FVector, Y));
    z_handle = box_size_handle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FVector, Z));

    edit_options = box_size_edit_mode::get_names();

    // clang-format off
    child_builder.AddCustomRow(INVTEXT("Vector Edit Mode"))
        .NameContent()
        [
            SNew(STextBlock)
            .Text(INVTEXT("Edit mode"))
            .Font(IDetailLayoutBuilder::GetDetailFont())
        ]
        .ValueContent()
        .HAlign(HAlign_Fill)
        [
            SAssignNew(edit_mode_combobox, SComboBox<FName>)
		    .OptionsSource(&edit_options)
            .OnGenerateWidget_Lambda([](FName mode)->TSharedRef<SWidget>
		    {
			    return SNew(STextBlock)
				    .Text(FText::FromName(mode))
                    .Font(IDetailLayoutBuilder::GetDetailFont());
		    })
		    .OnSelectionChanged_Lambda(
                [this](FName new_selection, ESelectInfo::Type select_info) {
                     // clang-format on
                     auto const old_edit_mode{edit_mode};

                     auto const new_edit_mode{box_size_edit_mode::from_name(new_selection)};
                     if (!new_edit_mode) {
                         UE_LOG(LogSandboxEditor,
                                Error,
                                TEXT("Invalid selection: %s"),
                                *new_selection.ToString());
                     } else {
                         edit_mode = new_edit_mode.GetValue();
                     }

                     if (old_edit_mode != edit_mode) {
                         box_size_widget_container->SetContent(make_box_size_value_widget());
                     }
                     // clang-format off
                }
            )
            [
                SNew(STextBlock)
                .Text_Lambda([this] { return box_size_edit_mode::to_text(edit_mode); })
                .Font(IDetailLayoutBuilder::GetDetailFont())
            ]
        ];

    child_builder.AddCustomRow(INVTEXT("Box Size"))
        .NameContent()
        [
            SNew(STextBlock)
            .Text(INVTEXT("Box size"))
            .Font(IDetailLayoutBuilder::GetDetailFont())
        ]
        .ValueContent()
        .HAlign(HAlign_Fill)
        [
            SAssignNew(box_size_widget_container, SBox)
            [
                make_box_size_value_widget()
            ]
        ];
    // clang-format on
}

auto FBoxSizeCustomisation::MakeInstance() -> TSharedRef<IPropertyTypeCustomization> {
    return MakeShareable(new ThisClass);
}

auto FBoxSizeCustomisation::make_box_size_value_widget() -> TSharedRef<SWidget> {
    switch (edit_mode) {
        case EBoxSizeEditMode::xyz:
            return make_xyz_widget();

        case EBoxSizeEditMode::uniform:
            return make_uniform_widget();

        case EBoxSizeEditMode::xy_and_z:
            return make_xy_and_z_widget();
    }

    return SNullWidget::NullWidget;
}
auto FBoxSizeCustomisation::make_xyz_widget() -> TSharedRef<SWidget> {
    return SNew(Vec3Input)
        .Font(IDetailLayoutBuilder::GetDetailFont())
        .AllowSpin(true)
        .X_Lambda([&]() -> TOptional<value_type> { return read_prop(x_handle); })
        .Y_Lambda([&]() -> TOptional<value_type> { return read_prop(y_handle); })
        .Z_Lambda([&]() -> TOptional<value_type> { return read_prop(z_handle); })
        .OnXChanged_Lambda([&](value_type value) { write_prop(x_handle, value); })
        .OnYChanged_Lambda([&](value_type value) { write_prop(y_handle, value); })
        .OnZChanged_Lambda([&](value_type value) { write_prop(z_handle, value); });
}
auto FBoxSizeCustomisation::make_uniform_widget() -> TSharedRef<SWidget> {
    return SNew(ScalarInput)
        .Font(IDetailLayoutBuilder::GetDetailFont())
        .Value_Lambda([&]() -> TOptional<value_type> { return read_prop(x_handle); })
        .OnValueChanged_Lambda([this](value_type value) {
            write_prop(x_handle, value);
            write_prop(y_handle, value);
            write_prop(z_handle, value);
        });
}
auto FBoxSizeCustomisation::make_xy_and_z_widget() -> TSharedRef<SWidget> {
    return SNew(Vec2Input)
        .Font(IDetailLayoutBuilder::GetDetailFont())
        .AllowSpin(true)
        .X_Lambda([&]() -> TOptional<value_type> { return read_prop(x_handle); })
        .Y_Lambda([&]() -> TOptional<value_type> { return read_prop(z_handle); })
        .OnXChanged_Lambda([&](value_type value) {
            write_prop(x_handle, value);
            write_prop(y_handle, value);
        })
        .OnYChanged_Lambda([&](value_type value) { write_prop(z_handle, value); });
}
auto FBoxSizeCustomisation::read_prop(TSharedPtr<IPropertyHandle> handle) -> TOptional<value_type> {
    value_type value{0.0};

    if (!handle.IsValid() || handle->GetValue(value) != FPropertyAccess::Success) {
        return {};
    }

    return value;
}
void FBoxSizeCustomisation::write_prop(TSharedPtr<IPropertyHandle> handle, value_type value) {
    if (handle.IsValid()) {
        handle->SetValue(value);
    }
}
