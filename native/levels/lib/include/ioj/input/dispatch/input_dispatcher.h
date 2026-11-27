#pragma once

#include <ioj/input/dispatch/events.h>
#include <ioj/sim/player/player_input_frame.h>

#include <cstddef>
#include <span>
#include <vector>

namespace ioj {
struct LogicalInputState;

class InputDispatcher {
  public:
    void begin_frame();
    void push(host::PlayerAction event);
    void push(host::MenuButton event);
    void push(PhysicalKey event);
    void push(host::MenuKey event);
    void push(host::MenuPointer event);
    void push(host::MenuText event);
    void publish_ship(LogicalInputState const& state);

    auto ship_input() const -> sim::player::PlayerInputFrame;
    auto buttons() const -> std::span<host::MenuButton const> { return buttons_; }
    auto size() const -> std::size_t { return order_.size(); }
    void deliver_from(std::size_t first, auto& receiver) const {
        auto pending{std::span{order_}.subspan(first)};
        while (!pending.empty()) {
            auto const [kind, row]{pending.front()};
            std::size_t count{1};
            while (count < pending.size() && pending[count].kind == kind) {
                ++count;
            }
            switch (kind) {
                case Kind::PlayerAction:
                    receiver.consume(std::span{actions_}.subspan(row, count));
                    break;
                case Kind::MenuButton:
                    receiver.consume(std::span{buttons_}.subspan(row, count));
                    break;
                case Kind::PhysicalKey:
                    receiver.consume(std::span{candidates_}.subspan(row, count));
                    break;
                case Kind::MenuKey:
                    receiver.consume(std::span{keys_}.subspan(row, count));
                    break;
                case Kind::MenuPointer:
                    receiver.consume(std::span{pointers_}.subspan(row, count));
                    break;
                case Kind::MenuText:
                    receiver.consume(std::span{text_}.subspan(row, count));
                    break;
            }
            pending = pending.subspan(count);
        }
    }
  private:
    enum class Kind { PlayerAction, MenuButton, PhysicalKey, MenuKey, MenuPointer, MenuText };
    struct Entry {
        Kind kind;
        std::size_t row;
    };
    std::vector<Entry> order_;
    std::vector<host::PlayerAction> actions_;
    std::vector<host::MenuButton> buttons_;
    std::vector<PhysicalKey> candidates_;
    std::vector<host::MenuKey> keys_;
    std::vector<host::MenuPointer> pointers_;
    std::vector<host::MenuText> text_;
    std::vector<sim::player::PlayerInputKind> ship_events_;
    float pitch_{};
    float yaw_{};
    float accelerator_{};
};
}
