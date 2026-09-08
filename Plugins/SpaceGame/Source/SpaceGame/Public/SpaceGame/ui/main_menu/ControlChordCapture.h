#pragma once

#include "InputCoreTypes.h"

namespace ml::ioj {

class FControlChordCapture {
  public:
    auto accept(FKey const key, bool const can_be_held) -> bool {
        if (!key.IsValid() || is_complete() || key == held_key_) {
            return false;
        }
        if (!held_key_.IsValid()) {
            if (can_be_held) {
                held_key_ = key;
            }
            return false;
        }
        activator_key_ = held_key_;
        action_key_ = key;
        return true;
    }

    void release(FKey const key) {
        if (!is_complete() && held_key_ == key) {
            held_key_ = EKeys::Invalid;
        }
    }

    void clear() {
        held_key_ = EKeys::Invalid;
        activator_key_ = EKeys::Invalid;
        action_key_ = EKeys::Invalid;
    }

    [[nodiscard]] auto is_complete() const -> bool {
        return activator_key_.IsValid() && action_key_.IsValid();
    }
    [[nodiscard]] auto held_key() const -> FKey { return held_key_; }
    [[nodiscard]] auto activator_key() const -> FKey { return activator_key_; }
    [[nodiscard]] auto action_key() const -> FKey { return action_key_; }
  private:
    FKey held_key_{};
    FKey activator_key_{};
    FKey action_key_{};
};

} // namespace ml::ioj
