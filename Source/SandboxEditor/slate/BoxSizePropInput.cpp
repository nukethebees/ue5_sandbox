#include "BoxSizePropInput.h"

#include "SandboxEditor/logging/SandboxEditorLogCategories.h"

#include <DetailLayoutBuilder.h>
#include <DetailWidgetRow.h>
#include <IDetailChildrenBuilder.h>
#include <PropertyHandle.h>
#include <Widgets/Input/SCheckBox.h>
#include <Widgets/Input/SComboBox.h>
#include <Widgets/Input/SVectorInputBox.h>
#include <Widgets/Layout/SBox.h>
#include <Widgets/SBoxPanel.h>
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
    using Box = SHorizontalBox;

    auto add_slot{[&]() -> Box::FSlot::FSlotArguments {
        // clang-format off
        return MoveTemp(Box::Slot() 
            .FillWidth(1.f)
            .VAlign(VAlign_Fill)
            .HAlign(EHorizontalAlignment::HAlign_Center));
        // clang-format on
    }};

    // clang-format off
    return SNew(Box)
        +add_slot()
        [
            make_mode_button(EBoxSizeEditMode::xyz, INVTEXT("X | Y | Z"))
        ]
        +add_slot()
        [
            make_mode_button(EBoxSizeEditMode::xy_and_z, INVTEXT("XY | Z"))
        ]
        +add_slot()
        [
            make_mode_button(EBoxSizeEditMode::uniform, INVTEXT("XYZ"))
        ]
    ;
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

auto SBoxSizePropInput::make_mode_button(EBoxSizeEditMode const mode, FText const& label)
    -> TSharedRef<SWidget> {

    // clang-format off
    return SNew(SCheckBox)
        .Style(FAppStyle::Get(), "RadioButton")
        .IsChecked_Lambda([this, mode]() -> ECheckBoxState {
            return edit_mode == mode ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
        })
        .OnCheckStateChanged_Lambda([this, mode](ECheckBoxState const new_state) {
            if (new_state != ECheckBoxState::Checked) {
                return;
            }

            edit_mode = mode;

            box_size_widget_container->SetContent(make_box_size_value_widget());
        })
        [
            SNew(STextBlock)
            .Text(label)
        ];
    // clang-format on
}
}
