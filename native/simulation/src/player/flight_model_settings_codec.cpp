#include <ioj/sim/player/flight_model_settings_codec.h>

#include <bit>
#include <cstdint>
#include <utility>

namespace ioj::flight_model_settings_codec {
namespace detail {
using namespace sim::player;

class Archive {
  public:
    Archive() = default;
    explicit Archive(std::string_view data)
        : input_{data}
        , loading_{true}
        , valid_{data.starts_with(prefix)} {}

    auto operator<<(std::uint8_t& value) -> Archive& {
        if (loading_) {
            if (cursor_ + 2 > input_.size()) {
                valid_ = false;
                return *this;
            }
            auto const high{digit(input_[cursor_++])};
            auto const low{digit(input_[cursor_++])};
            if (high < 0 || low < 0) {
                valid_ = false;
            } else {
                value = static_cast<std::uint8_t>((high << 4) | low);
            }
        } else {
            output_.push_back(digits[value >> 4]);
            output_.push_back(digits[value & 15]);
        }
        return *this;
    }

    auto operator<<(std::uint32_t& value) -> Archive& {
        auto raw{value};
        if (loading_) {
            raw = 0;
        }
        for (std::uint32_t shift{}; shift < 32; shift += 8) {
            auto byte{static_cast<std::uint8_t>(raw >> shift)};
            *this << byte;
            if (loading_) {
                raw |= static_cast<std::uint32_t>(byte) << shift;
            }
        }
        value = raw;
        return *this;
    }

    auto operator<<(float& value) -> Archive& {
        auto raw{std::bit_cast<std::uint32_t>(value)};
        *this << raw;
        value = std::bit_cast<float>(raw);
        return *this;
    }

    auto loading() const -> bool { return loading_; }
    auto complete() const -> bool { return valid_ && cursor_ == input_.size(); }
    auto take_output() -> std::string { return std::move(output_); }
    void invalidate() { valid_ = false; }

  private:
    static auto digit(char const value) -> int {
        if (value >= '0' && value <= '9') {
            return value - '0';
        }
        if (value >= 'a' && value <= 'f') {
            return value - 'a' + 10;
        }
        return -1;
    }

    inline static constexpr std::string_view prefix{"flight-model:2:"};
    inline static constexpr char digits[]{"0123456789abcdef"};
    std::string_view input_{};
    std::string output_{prefix};
    std::size_t cursor_{prefix.size()};
    bool loading_{};
    bool valid_{true};
};

template <typename Enum>
void transfer_enum(Archive& archive, Enum& value) {
    std::uint8_t raw{static_cast<std::uint8_t>(value)};
    archive << raw;
    if (archive.loading()) {
        value = static_cast<Enum>(raw);
    }
}

void transfer_bool(Archive& archive, bool& value) {
    std::uint8_t raw{value ? std::uint8_t{1} : std::uint8_t{0}};
    archive << raw;
    if (raw > 1) {
        archive.invalidate();
    }
    if (archive.loading()) {
        value = raw != 0;
    }
}

void transfer(Archive& archive, ResponseConfig& value) {
    transfer_enum(archive, value.mode);
    archive << value.rate_limited.increasing_rate;
    archive << value.rate_limited.decreasing_rate;
    archive << value.second_order.settling_time;
    archive << value.second_order.damping_ratio;
}

void transfer(Archive& archive, TranslationChannelConfig& value) {
    transfer_enum(archive, value.semantic);
    transfer_enum(archive, value.reference_frame);
    transfer_enum(archive, value.input_source);
    archive << value.automatic_value;
    transfer(archive, value.response);
}

void transfer(Archive& archive, TranslationDriveConfig& value) {
    archive << value.positive_target_speed;
    archive << value.negative_target_speed;
    archive << value.positive_speed_limit;
    archive << value.negative_speed_limit;
    archive << value.positive_acceleration;
    archive << value.negative_acceleration;
}

void transfer(Archive& archive, TranslationAxisConfig& value) {
    transfer(archive, value.manual);
    transfer(archive, value.automatic);
    transfer(archive, value.normal);
    transfer(archive, value.boosted);
    archive << value.passive_drag;
    transfer_enum(archive, value.passive_drag_reference_frame);
    archive << value.active_stabilization_rate;
    transfer_enum(archive, value.active_stabilization_reference_frame);
}

void transfer(Archive& archive, RotationStabilizationConfig& value) {
    transfer_bool(archive, value.enabled);
    archive << value.target_angle;
    archive << value.delay;
    transfer(archive, value.response);
}

void transfer(Archive& archive, RotationAxisConfig& value) {
    transfer_enum(archive, value.manual_semantic);
    archive << value.maximum_rate;
    archive << value.acceleration;
    transfer(archive, value.response);
    transfer(archive, value.stabilization);
}

void transfer(Archive& archive, BrakeConfig& value) {
    transfer_bool(archive, value.available);
    archive << value.target_speed;
    archive << value.deceleration;
    archive << value.energy_drain_per_second;
    transfer(archive, value.response);
}

void transfer(Archive& archive, FlightModelConfig& value) {
    transfer(archive, value.translation.forward);
    transfer(archive, value.translation.right);
    transfer(archive, value.translation.up);
    transfer(archive, value.rotation.pitch);
    transfer(archive, value.rotation.yaw);
    transfer(archive, value.rotation.roll);
    transfer_enum(archive, value.facing_velocity.mode);
    archive << value.facing_velocity.alignment_rate;
    transfer(archive, value.facing_velocity.response);
    transfer_bool(archive, value.boost.available);
    transfer_bool(archive, value.boost.accelerator_activates_boost);
    archive << value.boost.energy_drain_per_second;
    transfer(archive, value.boost.response);
    transfer(archive, value.brake);
    transfer(archive, value.emergency_brake);
    archive << value.energy_recharge_per_second;
    archive << value.maximum_resultant_speed;
    archive << value.boosted_maximum_resultant_speed;
}

void transfer(Archive& archive, FlightModelProfile& value) {
    transfer_enum(archive, value.base_preset);
    transfer_bool(archive, value.customized);
    transfer(archive, value.config);
}
} // namespace detail

auto encode(sim::player::FlightModelProfile const& profile) -> std::string {
    detail::Archive archive;
    auto value{profile};
    detail::transfer(archive, value);
    return archive.take_output();
}

auto decode(std::string_view const data, sim::player::FlightModelProfile& profile) -> bool {
    if (data.size() > 4096) {
        return false;
    }
    detail::Archive archive{data};
    auto candidate{profile};
    detail::transfer(archive, candidate);
    if (!archive.complete() || candidate.base_preset > sim::player::FlightModelPreset::Gunship ||
        !sim::player::validate_flight_model_config(candidate.config)) {
        return false;
    }
    profile = candidate;
    return true;
}

auto decode_loadout(std::array<std::string_view, 4> const& data) -> DecodedLoadout {
    DecodedLoadout result{sim::player::make_default_flight_model_loadout()};
    auto const count{data.size()};
    for (std::size_t index{}; index < count; ++index) {
        if (data[index].empty()) {
            continue;
        }
        auto& target{result.value.profiles[index]};
        auto candidate{target};
        if (!decode(data[index], candidate) || candidate.base_preset != target.base_preset ||
            !sim::player::matches_authored_flight_model_topology(candidate.config,
                                                                 target.base_preset)) {
            result.invalid_slots[index] = true;
            continue;
        }
        target = candidate;
    }
    return result;
}
} // namespace ioj::flight_model_settings_codec
