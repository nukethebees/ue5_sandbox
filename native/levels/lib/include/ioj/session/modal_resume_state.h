#pragma once

#include <ioj/levels/control_context.h>

#include <optional>

namespace ioj::session {
class ModalResumeState {
  public:
    void suspend(levels::ControlContext const context) { context_ = context; }
    void clear() { context_.reset(); }
    void on_player_changed(bool const has_player) {
        if (context_) {
            if (has_player) {
                context_ = levels::ControlContext::Player;
            } else if (*context_ == levels::ControlContext::Player) {
                context_ = levels::ControlContext::None;
            }
        }
    }
    [[nodiscard]] auto context(levels::ControlContext const fallback) const
        -> levels::ControlContext {
        return context_.value_or(fallback);
    }
    [[nodiscard]] auto pending() const -> bool { return context_.has_value(); }
  private:
    std::optional<levels::ControlContext> context_;
};
}
