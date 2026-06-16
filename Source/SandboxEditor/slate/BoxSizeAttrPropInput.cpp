#include "BoxSizeAttrPropInput.h"

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
void SBoxSizeAttrPropInput::Construct(FArguments const& args) {
    value = args._value;
    on_value_changed = args._on_value_changed;

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

auto SBoxSizeAttrPropInput::get_vector() const -> TOptional<vector_type> {
    return value.Get();
}
auto SBoxSizeAttrPropInput::get_vector_value() const -> vector_type {
    return get_vector().Get(vector_type::ZeroVector);
}
void SBoxSizeAttrPropInput::set_vector(vector_type const& new_value) {
    if (on_value_changed.IsBound()) { on_value_changed.Execute(new_value); }
}

auto SBoxSizeAttrPropInput::get_x() const -> TOptional<value_type> {
    auto const v{get_vector()};

    if (!v.IsSet()) { return {}; }

    return v->X;
}
auto SBoxSizeAttrPropInput::get_y() const -> TOptional<value_type> {
    auto const v{get_vector()};

    if (!v.IsSet()) { return {}; }

    return v->Y;
}
auto SBoxSizeAttrPropInput::get_z() const -> TOptional<value_type> {
    auto const v{get_vector()};

    if (!v.IsSet()) { return {}; }

    return v->Z;
}

void SBoxSizeAttrPropInput::set_x(value_type new_value) {
    auto v{get_vector_value()};
    v.X = new_value;
    set_vector(v);
}
void SBoxSizeAttrPropInput::set_y(value_type new_value) {
    auto v{get_vector_value()};
    v.Y = new_value;
    set_vector(v);
}
void SBoxSizeAttrPropInput::set_z(value_type new_value) {
    auto v{get_vector_value()};
    v.Z = new_value;
    set_vector(v);
}

auto SBoxSizeAttrPropInput::make_edit_mode_row() -> TSharedRef<SWidget> {
    using Box = SHorizontalBox;

    auto add_slot{[&]() -> Box::FSlot::FSlotArguments {
        // clang-format off
        return MoveTemp(Box::Slot() 
            .FillWidth(1.f)
            .VAlign(VAlign_Center)
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
auto SBoxSizeAttrPropInput::make_box_size_row() -> TSharedRef<SWidget> {
    // clang-format off
    return SAssignNew(box_size_widget_container, SBox)
        [
            make_box_size_value_widget()
        ];
    // clang-format on
}

auto SBoxSizeAttrPropInput::make_box_size_value_widget() -> TSharedRef<SWidget> {
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
auto SBoxSizeAttrPropInput::make_xyz_widget() -> TSharedRef<SWidget> {
    return SNew(Vec3Input)
        .Font(IDetailLayoutBuilder::GetDetailFont())
        .AllowSpin(allow_spin)
        .bColorAxisLabels(colour_axis_labels)
        .X_Lambda([this] -> TOptional<value_type> { return get_x(); })
        .Y_Lambda([this] -> TOptional<value_type> { return get_y(); })
        .Z_Lambda([this] -> TOptional<value_type> { return get_z(); })
        .OnXChanged_Lambda([this](value_type new_value) { set_x(new_value); })
        .OnYChanged_Lambda([this](value_type new_value) { set_y(new_value); })
        .OnZChanged_Lambda([this](value_type new_value) { set_z(new_value); });
}
auto SBoxSizeAttrPropInput::make_uniform_widget() -> TSharedRef<SWidget> {
    return SNew(ScalarInput)
        .Font(IDetailLayoutBuilder::GetDetailFont())
        .Value_Lambda([this]() -> TOptional<value_type> {
            auto const v{get_vector_value()};
            if (FMath::IsNearlyEqual(v.X, v.Y) && FMath::IsNearlyEqual(v.X, v.Z)) { return v.X; }
            return {};
        })
        .OnValueChanged_Lambda([this](value_type value) { set_vector({value, value, value}); });
}
auto SBoxSizeAttrPropInput::make_xy_and_z_widget() -> TSharedRef<SWidget> {
    // The X and Y are set by Vec2.X
    // Z is set by Vec2.Y

    return SNew(Vec2Input)
        .Font(IDetailLayoutBuilder::GetDetailFont())
        .AllowSpin(allow_spin)
        .bColorAxisLabels(colour_axis_labels)
        .X_Lambda([this] -> TOptional<value_type> {
            auto const v{get_vector_value()};
            if (FMath::IsNearlyEqual(v.X, v.Y)) { return v.X; }
            return {};
        })
        .Y_Lambda([&] -> TOptional<value_type> { return get_z(); })
        .OnXChanged_Lambda([this](value_type value) {
            auto const v{get_vector_value()};
            set_vector({value, value, v.Z});
        })
        .OnYChanged_Lambda([this](value_type value) {
            auto const v{get_vector_value()};
            set_vector({v.X, v.Y, value});
        });
}

auto SBoxSizeAttrPropInput::make_mode_button(EBoxSizeEditMode const mode, FText const& label)
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
