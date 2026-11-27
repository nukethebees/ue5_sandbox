#pragma once

#include "player/input/physical/physical_input.h"
#include <ioj/input/physical/event_kind.h>
#include <ioj/input/physical/physical_input_tables.h>

#include <cstddef>
#include <utility>
#include <vector>

namespace ioj {
class PhysicalInputBatch {
  public:
    void push(PhysicalButton event) {
        order_.push_back({PhysicalEventKind::Button,
                          buttons_.add(event.key, event.modifiers, event.state, event.repeat)});
    }
    void push(PhysicalPointerButton event) {
        order_.push_back(
            {PhysicalEventKind::PointerButton,
             pointer_buttons_.add(event.position, event.key, event.state, event.clicks)});
    }
    void push(PhysicalAxis event) {
        order_.push_back({PhysicalEventKind::Axis, axes_.add(event.key, event.position)});
    }
    void push(PointerMotion event) {
        order_.push_back(
            {PhysicalEventKind::PointerMotion, motions_.add(event.position, event.delta)});
    }
    void push(WheelMotion event) {
        order_.push_back({PhysicalEventKind::Wheel, wheels_.add(event.position, event.steps)});
    }
    void push(TextInput event) {
        text_.push_back(std::move(event));
        order_.push_back({PhysicalEventKind::Text, text_.size() - 1});
    }
    void push(GamepadDisconnected) { order_.push_back({PhysicalEventKind::Disconnect, 0}); }
    void push(WindowFocus focus) {
        order_.push_back({focus == WindowFocus::Gained ? PhysicalEventKind::FocusGained
                                                       : PhysicalEventKind::FocusLost,
                          0});
    }
    auto size() const -> std::size_t { return order_.size(); }
    auto kind(std::size_t index) const -> PhysicalEventKind { return order_[index].kind; }
    void visit(std::size_t index, auto&& handle) const {
        auto const [kind, row]{order_[index]};
        switch (kind) {
            case PhysicalEventKind::Button: {
                auto const rows{buttons_.view()};
                handle(PhysicalButton{rows.keys()[row],
                                      rows.modifiers()[row],
                                      rows.states()[row],
                                      rows.repeats()[row]});
                break;
            }
            case PhysicalEventKind::PointerButton: {
                auto const rows{pointer_buttons_.view()};
                handle(PhysicalPointerButton{rows.positions()[row],
                                             rows.keys()[row],
                                             rows.states()[row],
                                             rows.clicks()[row]});
                break;
            }
            case PhysicalEventKind::Axis: {
                auto const rows{axes_.view()};
                handle(PhysicalAxis{rows.keys()[row], rows.positions()[row]});
                break;
            }
            case PhysicalEventKind::PointerMotion: {
                auto const rows{motions_.view()};
                handle(PointerMotion{rows.positions()[row], rows.deltas()[row]});
                break;
            }
            case PhysicalEventKind::Wheel: {
                auto const rows{wheels_.view()};
                handle(WheelMotion{rows.positions()[row], rows.steps()[row]});
                break;
            }
            case PhysicalEventKind::Text:
                handle(text_[row]);
                break;
            case PhysicalEventKind::Disconnect:
                handle(GamepadDisconnected{});
                break;
            case PhysicalEventKind::FocusGained:
                handle(WindowFocus::Gained);
                break;
            case PhysicalEventKind::FocusLost:
                handle(WindowFocus::Lost);
                break;
        }
    }
    void clear() {
        order_.clear();
        text_.clear();
        buttons_.reset();
        pointer_buttons_.reset();
        axes_.reset();
        motions_.reset();
        wheels_.reset();
    }

  private:
    struct Entry {
        PhysicalEventKind kind;
        std::size_t row;
    };
    std::vector<Entry> order_;
    std::vector<TextInput> text_;
    PhysicalButtonsRows buttons_;
    PhysicalPointerButtonsRows pointer_buttons_;
    PhysicalAxesRows axes_;
    PhysicalMotionsRows motions_;
    PhysicalWheelsRows wheels_;
};
}
