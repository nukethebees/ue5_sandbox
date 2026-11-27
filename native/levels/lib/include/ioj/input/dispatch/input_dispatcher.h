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
    void push(host::MenuEvent event);
    void push(host::PresentationEvent event);
    void publish_ship(LogicalInputState const& state);

    auto ship_input() const -> sim::player::PlayerInputFrame;
    auto menu_input() const -> std::span<host::MenuEvent const> { return menu_; }
    auto presentation_input() const -> std::span<host::PresentationEvent const> {
        return presentation_;
    }
    auto size() const -> std::size_t { return order_.size(); }

    void deliver_from(std::size_t first, auto& receiver) const {
        auto pending{std::span{order_}.subspan(first)};
        while (!pending.empty()) {
            auto const [presentation, row]{pending.front()};
            std::size_t count{1};
            while (count < pending.size() && pending[count].presentation == presentation) {
                ++count;
            }

            if (presentation) {
                receiver.consume(presentation_input().subspan(row, count));
            } else {
                receiver.consume(menu_input().subspan(row, count));
            }
            pending = pending.subspan(count);
        }
    }

  private:
    struct Entry {
        bool presentation;
        std::size_t row;
    };

    std::vector<Entry> order_;
    std::vector<host::MenuEvent> menu_;
    std::vector<host::PresentationEvent> presentation_;
    std::vector<sim::player::PlayerInputKind> ship_events_;
    float pitch_{};
    float yaw_{};
    float accelerator_{};
};
}
