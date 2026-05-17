#pragma once

#include "SandboxEditor/slate/BoxSizeEditMode.h"

#include <Templates/SharedPointer.h>
#include <Widgets/Input/SVectorInputBox.h>
#include <Widgets/SCompoundWidget.h>

class IPropertyHandle;
class SWidget;
template <typename T>
class SComboBox;
class SBox;

namespace ml {
class SBoxSizePropInput final : public SCompoundWidget {
  public:
    using ThisClass = SBoxSizePropInput;
    using Super = SCompoundWidget;

    using value_type = double;

    using ScalarInput = SNumericEntryBox<value_type>;
    using Vec2Input = SNumericVectorInputBox<value_type, UE::Math::TVector2<value_type>, 2>;
    using Vec3Input = SNumericVectorInputBox<value_type, UE::Math::TVector<value_type>, 3>;

    // clang-format off
    SLATE_BEGIN_ARGS(SBoxSizePropInput) {}
        SLATE_ARGUMENT(TSharedPtr<IPropertyHandle>, x_handle)
        SLATE_ARGUMENT(TSharedPtr<IPropertyHandle>, y_handle)
        SLATE_ARGUMENT(TSharedPtr<IPropertyHandle>, z_handle)
    SLATE_END_ARGS()
    // clang-format on

    void Construct(FArguments const& args);
  private:
    auto get_value(TSharedPtr<IPropertyHandle> const& handle) const -> TOptional<value_type>;
    void set_value(TSharedPtr<IPropertyHandle> const& handle, value_type value);

    auto make_edit_mode_row() -> TSharedRef<SWidget>;
    auto make_box_size_row() -> TSharedRef<SWidget>;

    auto make_box_size_value_widget() -> TSharedRef<SWidget>;
    auto make_xyz_widget() -> TSharedRef<SWidget>;
    auto make_uniform_widget() -> TSharedRef<SWidget>;
    auto make_xy_and_z_widget() -> TSharedRef<SWidget>;

    // Edit mode
    TSharedPtr<SComboBox<FName>> edit_mode_combobox{nullptr};
    EBoxSizeEditMode edit_mode{EBoxSizeEditMode::uniform};
    TArray<FName> edit_options;

    // Value
    TSharedPtr<SBox> box_size_widget_container;

    TSharedPtr<IPropertyHandle> x_handle{nullptr};
    TSharedPtr<IPropertyHandle> y_handle{nullptr};
    TSharedPtr<IPropertyHandle> z_handle{nullptr};
};
}
