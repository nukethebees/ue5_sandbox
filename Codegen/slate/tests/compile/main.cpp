#include "support.h"

#include "CompileSmoke/Widgets.slate.generated.h"
#include "CompileSmoke/Host.slate.generated.h"

#include <iostream>
#include <memory>

namespace {

struct CopyOnlyCallback {
    explicit CopyOnlyCallback(int& copies) : copies_{copies} {}
    CopyOnlyCallback(CopyOnlyCallback const& other) : copies_{other.copies_} { ++copies_; }
    CopyOnlyCallback(CopyOnlyCallback&&) = delete;
    auto operator()() -> int { return 31; }

  private:
    int& copies_;
};

}

auto main() -> int {
    namespace Widgets = SlateGenerated::CompileSmoke::Widgets;
    int failures{};
    auto const check{[&](bool passed, char const* message) {
        if (!passed) {
            std::cerr << message << '\n';
            ++failures;
        }
    }};

    check(CompileSmoke::other_empty_function() == &Widgets::empty,
          "Inline library function must have one identity across translation units");
    auto empty{CompileSmoke::other_empty_function()()};
    check(empty.invoke() == 17, "Parameterless library function must work from either TU");
    check(CompileSmoke::invoke_other_callback() == 29, "Callback in second TU must run");

    auto callback{[value = std::make_unique<int>(23)] { return *value; }};
    auto widget{Widgets::callback(std::move(callback))};
    check(widget.invoke() == 23, "Library must forward a move-only callback");
    auto child{Widgets::existing(std::move(widget))};
    check(child.invoke() == 23, "Library must forward a move-only existing widget");

    int copies{};
    CopyOnlyCallback copy_only{copies};
    auto copied{Widgets::callback(copy_only)};
    check(copied.invoke() == 31 && copies == 1, "Lvalue callback must be copied, not moved");

    auto factory{[value = std::make_unique<int>(37)](int offset) {
        return Widgets::callback([result = *value + offset] { return result; });
    }};
    auto first{Widgets::factory(factory, 1)};
    auto second{Widgets::factory(factory, 2)};
    check(first.invoke() == 38 && second.invoke() == 39,
          "Move-only lvalue factory must remain reusable");

    CompileSmoke::Host host;
    SlateGenerated::CompileSmoke::HostBuilder builder{host};
    auto owned{builder.callback([value = std::make_unique<int>(41)] { return *value; })};
    check(owned.invoke() == 41, "Class builder must forward a move-only callback");
    auto member{builder.member()};
    auto object{builder.object()};
    check(member.invoke() == 1 && object.invoke() == 2,
          "Friend builder must bind private callbacks to the same host");

    return failures == 0 ? 0 : 1;
}
