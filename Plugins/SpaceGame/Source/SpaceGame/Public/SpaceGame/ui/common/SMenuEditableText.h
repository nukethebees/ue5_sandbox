#pragma once

#include <SpaceGame/UnrealMessages.h>

#include <Widgets/Input/SEditableText.h>

namespace ioj {
class SMenuEditableText final : public SEditableText {
  public:
    void Construct(FArguments const& args, MenuControl field) {
        field_ = field;
        SEditableText::Construct(args);
    }
    auto OnMouseButtonDown(FGeometry const& geometry, FPointerEvent const& event)
        -> FReply override {
        hit_menu_control(field_);
        return SEditableText::OnMouseButtonDown(geometry, event);
    }
  private:
    MenuControl field_{};
};
}
