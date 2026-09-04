#pragma once

#include <functional>
#include <utility>

namespace SlateGenerated::CompileSmoke {
struct HostBuilder;
}

namespace CompileSmoke {

// Only the construction surface used by the fixtures; this does not emulate Slate ownership.
class Widget {
  public:
    template <typename Callback>
    auto OnClicked_Lambda(Callback&& callback) -> Widget {
        callback_ = std::move_only_function<int()>{std::forward<Callback>(callback)};
        return std::move(*this);
    }

    template <typename Owner>
    auto OnClicked(Owner* owner, int (Owner::*member)()) -> Widget {
        return OnClicked_Lambda([owner, member] { return (owner->*member)(); });
    }

    template <typename Owner>
    auto OnClicked_UObject(Owner* owner, int (Owner::*member)()) -> Widget {
        return OnClicked(owner, member);
    }

    auto invoke() -> int { return callback_ ? callback_() : 17; }

  private:
    std::move_only_function<int()> callback_;
};

class Host {
    friend struct SlateGenerated::CompileSmoke::HostBuilder;

  private:
    auto clicked() -> int { return ++clicks_; }
    int clicks_{};
};

using EmptyFunction = Widget (*)();
auto other_empty_function() -> EmptyFunction;
auto invoke_other_callback() -> int;

}

#define SNew(type) type{}
