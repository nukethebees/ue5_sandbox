#pragma once
#include <ioj/sim/player/player_event_kind.h>
#include <ioj/sim/player/player_input_frame.h>
#include <ioj/sim/player/trigger_mode.h>

#include <algorithm>
#include <memory_resource>
#include <vector>

namespace ioj::sim::player {
struct PlayerInputEvents {
    explicit PlayerInputEvents(std::pmr::memory_resource& memory)
        : events_{&memory} {}

    auto view() const -> PlayerInputFrame { return {events_, pitch_, yaw_, accelerator_}; }
    void clear_events() { events_.clear(); }

    void append_frame(PlayerInputFrame const input) {
        for (auto event : input.events) {
            add(event);
        }

        pitch_ = input.pitch;
        yaw_ = input.yaw;
        accelerator_ = input.accelerator;
    }

    void set_pitch_input(float input) noexcept { pitch_ = std::clamp(input, -1.f, 1.f); }
    void set_yaw_input(float input) noexcept { yaw_ = std::clamp(input, -1.f, 1.f); }
    void set_accelerator(float input) noexcept { accelerator_ = std::clamp(input, 0.f, 1.f); }

    void start_boost() { add(PlayerInputKind::BoostPressed); }
    void stop_boost() { add(PlayerInputKind::BoostReleased); }
    void start_brake() { add(PlayerInputKind::BrakePressed); }
    void stop_brake() { add(PlayerInputKind::BrakeReleased); }
    void start_emergency_brake() { add(PlayerInputKind::EmergencyBrakePressed); }
    void stop_emergency_brake() { add(PlayerInputKind::EmergencyBrakeReleased); }
    void start_fire_laser() { add(PlayerInputKind::FirePressed); }
    void stop_fire_laser() { add(PlayerInputKind::FireReleased); }
    void upgrade_laser() { add(PlayerInputKind::UpgradeLaser); }
    void select_next_laser_trigger_mode() { add(PlayerInputKind::NextTriggerMode); }
    void select_previous_laser_trigger_mode() { add(PlayerInputKind::PreviousTriggerMode); }
    void set_laser_trigger_mode(ShipTriggerMode mode) {
        switch (mode) {
            case ShipTriggerMode::Single:
                add(PlayerInputKind::SelectSingle);
                break;
            case ShipTriggerMode::Burst3:
                add(PlayerInputKind::SelectBurst3);
                break;
            case ShipTriggerMode::FullAuto:
                add(PlayerInputKind::SelectFullAuto);
                break;
        }
    }

  private:
    void add(PlayerInputKind event) { events_.push_back(event); }

    std::pmr::vector<PlayerInputKind> events_;
    float pitch_{};
    float yaw_{};
    float accelerator_{};
};
using PlayerEvents = std::pmr::vector<PlayerEventKind>;
}
