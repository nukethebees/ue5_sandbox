#pragma once

#include "SandboxEditor/slate/BoxSizeEditMode.h"

#include <Misc/Optional.h>
#include <Templates/SharedPointer.h>
#include <Widgets/Input/SVectorInputBox.h>
#include <Widgets/SCompoundWidget.h>

class IPropertyHandle;
class SWidget;
template <typename T>
class SComboBox;
class SBox;

namespace ml {
class SBoxSizeAttrPropInput final : public SCompoundWidget {
  public:
    using ThisClass = SBoxSizeAttrPropInput;
    using Super = SCompoundWidget;

    using value_type = double;
    using vector_type = UE::Math::TVector<value_type>;

    using ScalarInput = SNumericEntryBox<value_type>;
    using Vec2Input = SNumericVectorInputBox<value_type, UE::Math::TVector2<value_type>, 2>;
    using Vec3Input = SNumericVectorInputBox<value_type, vector_type, 3>;

    static inline constexpr bool allow_spin{true};
    static inline constexpr bool colour_axis_labels{true};

    DECLARE_DELEGATE_OneParam(FOnVectorChanged, vector_type const&);

    // clang-format off
    SLATE_BEGIN_ARGS(ThisClass) {}
        SLATE_ATTRIBUTE(TOptional<vector_type>, value)
        SLATE_EVENT(FOnVectorChanged, on_value_changed)
    SLATE_END_ARGS()
    // clang-format on

    void Construct(FArguments const& args);
  private:
    auto get_vector() const -> TOptional<vector_type>;
    auto get_vector_value() const -> vector_type;
    void set_vector(vector_type const& new_value);

    auto get_x() const -> TOptional<value_type>;
    auto get_y() const -> TOptional<value_type>;
    auto get_z() const -> TOptional<value_type>;

    void set_x(value_type value);
    void set_y(value_type value);
    void set_z(value_type value);

    auto make_edit_mode_row() -> TSharedRef<SWidget>;
    auto make_box_size_row() -> TSharedRef<SWidget>;

    auto make_box_size_value_widget() -> TSharedRef<SWidget>;
    auto make_xyz_widget() -> TSharedRef<SWidget>;
    auto make_uniform_widget() -> TSharedRef<SWidget>;
    auto make_xy_and_z_widget() -> TSharedRef<SWidget>;

    auto make_mode_button(EBoxSizeEditMode const mode, FText const& label) -> TSharedRef<SWidget>;

    // Edit mode
    EBoxSizeEditMode edit_mode{EBoxSizeEditMode::uniform};
    TArray<FName> edit_options;

    // Value
    TSharedPtr<SBox> box_size_widget_container;

    TAttribute<TOptional<vector_type>> value;
    FOnVectorChanged on_value_changed;
};
}
