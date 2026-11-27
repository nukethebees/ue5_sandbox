#pragma once

#include <ioj/input/physical/event_kind.h>
#include <ioj/input/physical/physical_input_tables.h>

#include <cstddef>
#include <string>
#include <vector>

namespace ioj {
struct PhysicalInputBatch {
    struct Entry {
        PhysicalEventKind kind;
        std::size_t row;
    };

    void
        button(PhysicalKey key, host::Modifiers modifiers, ButtonState state, bool repeat = false) {
        order.push_back({PhysicalEventKind::Button, buttons.add(key, modifiers, state, repeat)});
    }
    void
        pointer_button(Vector2f position, PhysicalKey key, ButtonState state, std::uint8_t clicks) {
        order.push_back(
            {PhysicalEventKind::PointerButton, pointer_buttons.add(position, key, state, clicks)});
    }
    void axis(PhysicalKey key, float position) {
        order.push_back({PhysicalEventKind::Axis, axes.add(key, position)});
    }
    void motion(Vector2f position, Vector2f delta) {
        order.push_back({PhysicalEventKind::PointerMotion, motions.add(position, delta)});
    }
    void wheel(Vector2f position, float steps) {
        order.push_back({PhysicalEventKind::Wheel, wheels.add(position, steps)});
    }
    void clear() {
        order.clear();
        text.clear();
        buttons.reset();
        pointer_buttons.reset();
        axes.reset();
        motions.reset();
        wheels.reset();
    }

    std::vector<Entry> order;
    std::vector<std::string> text;
    PhysicalButtonsRows buttons;
    PhysicalPointerButtonsRows pointer_buttons;
    PhysicalAxesRows axes;
    PhysicalMotionsRows motions;
    PhysicalWheelsRows wheels;
};
}
