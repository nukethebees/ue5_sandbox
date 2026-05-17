#include "BoxSizePropInput.h"

#include "SandboxEditor/logging/SandboxEditorLogCategories.h"

#include <DetailLayoutBuilder.h>
#include <DetailWidgetRow.h>
#include <IDetailChildrenBuilder.h>
#include <PropertyHandle.h>
#include <Widgets/Input/SComboBox.h>
#include <Widgets/Input/SVectorInputBox.h>
#include <Widgets/Layout/SBox.h>
#include <Widgets/Text/STextBlock.h>

namespace ml {
void SBoxSizePropInput::Construct(FArguments const& args) {
    x_handle = args._x_handle;
    y_handle = args._y_handle;
    z_handle = args._z_handle;

    edit_options = box_size_edit_mode::get_names();

    // clang-format off
    ChildSlot
    [
        SNew(SVerticalBox)
        +SVerticalBox::Slot()
        .AutoHeight()
        [
            make_edit_mode_row()
        ]
        +SVerticalBox::Slot()
        .AutoHeight()
        [
            make_box_size_row()
        ]
    ];
    // clang-format on
}

auto SBoxSizePropInput::get_value(TSharedPtr<IPropertyHandle> const& handle) const
    -> TOptional<value_type> {
    value_type value{0.0};

    if (!handle.IsValid()) {
        UE_LOG(LogSandboxEditor, Error, TEXT("(SBoxSizePropInput::get_value) Invalid handle."));
        return {};
    }

    auto const result{handle->GetValue(value)};

    switch (result) {
        case FPropertyAccess::Success: {
            return value;
        }
        case FPropertyAccess::MultipleValues: {
            UE_LOG(LogSandboxEditor,
                   Warning,
                   TEXT("(SBoxSizePropInput::get_value) FPropertyAccess::MultipleValues"));
            break;
        }
        case FPropertyAccess::Fail: {
            UE_LOG(LogSandboxEditor,
                   Error,
                   TEXT("(SBoxSizePropInput::get_value) FPropertyAccess::Fail"));
            break;
        }
    }

    return {};
}
void SBoxSizePropInput::set_value(TSharedPtr<IPropertyHandle> const& handle, value_type value) {
    if (handle.IsValid()) {
        handle->SetValue(value);
    } else {
        UE_LOG(LogSandboxEditor, Error, TEXT("(SBoxSizePropInput::set_value) Invalid handle."));
    }
}

auto SBoxSizePropInput::make_edit_mode_row() -> TSharedRef<SWidget> {
    return SAssignNew(edit_mode_combobox, SComboBox<FName>)
        .OptionsSource(&edit_options)
        .OnGenerateWidget_Lambda([](FName mode) -> TSharedRef<SWidget> {
            return SNew(STextBlock)
                .Text(FText::FromName(mode))
                .Font(IDetailLayoutBuilder::GetDetailFont());
        })
        .OnSelectionChanged_Lambda([this](FName new_selection, ESelectInfo::Type select_info) {
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
        })
            // clang-format off
        [
            SNew(STextBlock)
            .Text_Lambda([this] { 
                return box_size_edit_mode::to_text(edit_mode); 
            })
            .Font(IDetailLayoutBuilder::GetDetailFont())
        ];
    // clang-format on
}
auto SBoxSizePropInput::make_box_size_row() -> TSharedRef<SWidget> {
    // clang-format off
    return SAssignNew(box_size_widget_container, SBox)
        [
            make_box_size_value_widget()
        ];
    // clang-format on
}

auto SBoxSizePropInput::make_box_size_value_widget() -> TSharedRef<SWidget> {
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
auto SBoxSizePropInput::make_xyz_widget() -> TSharedRef<SWidget> {
    return SNew(Vec3Input)
        .Font(IDetailLayoutBuilder::GetDetailFont())
        .AllowSpin(true)
        .X_Lambda([&] { return get_value(x_handle); })
        .Y_Lambda([&] { return get_value(y_handle); })
        .Z_Lambda([&] { return get_value(z_handle); })
        .OnXChanged_Lambda([&](value_type value) { set_value(x_handle, value); })
        .OnYChanged_Lambda([&](value_type value) { set_value(y_handle, value); })
        .OnZChanged_Lambda([&](value_type value) { set_value(z_handle, value); });
}
auto SBoxSizePropInput::make_uniform_widget() -> TSharedRef<SWidget> {
    return SNew(ScalarInput)
        .Font(IDetailLayoutBuilder::GetDetailFont())
        .Value_Lambda([&]() -> TOptional<value_type> { return get_value(x_handle); })
        .OnValueChanged_Lambda([this](value_type value) {
            set_value(x_handle, value);
            set_value(y_handle, value);
            set_value(z_handle, value);
        });
}
auto SBoxSizePropInput::make_xy_and_z_widget() -> TSharedRef<SWidget> {
    // The X and Y are set by Vec2.X
    // Z is set by Vec2.Y

    return SNew(Vec2Input)
        .Font(IDetailLayoutBuilder::GetDetailFont())
        .AllowSpin(true)
        .X_Lambda([&] { return get_value(x_handle); })
        .Y_Lambda([&] { return get_value(z_handle); })
        .OnXChanged_Lambda([&](value_type value) {
            set_value(x_handle, value);
            set_value(y_handle, value);
        })
        .OnYChanged_Lambda([&](value_type value) { set_value(z_handle, value); });
}
}
