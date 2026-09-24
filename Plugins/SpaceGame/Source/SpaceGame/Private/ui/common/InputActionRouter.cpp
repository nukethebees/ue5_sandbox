#include <SpaceGame/ui/common/InputActionRouter.h>

#include <Input/CommonAnalogCursor.h>

namespace ml::ioj {
class FGameAnalogCursor final : public FCommonAnalogCursor {
  public:
    explicit FGameAnalogCursor(UCommonUIActionRouterBase const& router)
        : FCommonAnalogCursor{router} {}

    bool ShouldVirtualAcceptSimulateMouseButton(FKeyEvent const& event,
                                                EInputEvent const input_event) const override {
        // A synthetic Accept click would reach the viewport's gameplay mouse bindings.
        return ActionRouter.GetActiveInputMode(ECommonInputMode::Game) != ECommonInputMode::Game &&
               FCommonAnalogCursor::ShouldVirtualAcceptSimulateMouseButton(event, input_event);
    }
};

TSharedRef<FCommonAnalogCursor> UInputActionRouter::MakeAnalogCursor() const {
    return FCommonAnalogCursor::CreateAnalogCursor<FGameAnalogCursor>(*this);
}
}
