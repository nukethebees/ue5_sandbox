#include "SpaceGame/settings/FlightModelSettingsCodec.h"

#include "Misc/Base64.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"

namespace ml::ioj::flight_model_settings_codec {
namespace detail {
using namespace ::ioj::sim::player;

template <typename Enum>
void transfer_enum(FArchive& archive, Enum& value) {
    uint8 raw{static_cast<uint8>(value)};
    archive << raw;
    if (archive.IsLoading()) {
        value = static_cast<Enum>(raw);
    }
}

void transfer_bool(FArchive& archive, bool& value) {
    uint8 raw{value ? uint8{1} : uint8{0}};
    archive << raw;
    if (archive.IsLoading()) {
        value = raw != 0;
    }
}

void transfer(FArchive& archive, ResponseConfig& value) {
    transfer_enum(archive, value.mode);
    archive << value.rate_limited.increasing_rate;
    archive << value.rate_limited.decreasing_rate;
    archive << value.second_order.settling_time;
    archive << value.second_order.damping_ratio;
}

void transfer(FArchive& archive, TranslationChannelConfig& value) {
    transfer_enum(archive, value.semantic);
    transfer_enum(archive, value.reference_frame);
    transfer_enum(archive, value.input_source);
    archive << value.automatic_value;
    transfer(archive, value.response);
}

void transfer(FArchive& archive, TranslationDriveConfig& value) {
    archive << value.positive_target_speed;
    archive << value.negative_target_speed;
    archive << value.positive_speed_limit;
    archive << value.negative_speed_limit;
    archive << value.positive_acceleration;
    archive << value.negative_acceleration;
}

void transfer(FArchive& archive, TranslationAxisConfig& value) {
    transfer(archive, value.manual);
    transfer(archive, value.automatic);
    transfer(archive, value.normal);
    transfer(archive, value.boosted);
    archive << value.passive_drag;
    transfer_enum(archive, value.passive_drag_reference_frame);
    archive << value.active_stabilization_rate;
    transfer_enum(archive, value.active_stabilization_reference_frame);
}

void transfer(FArchive& archive, RotationStabilizationConfig& value) {
    transfer_bool(archive, value.enabled);
    archive << value.target_angle;
    archive << value.delay;
    transfer(archive, value.response);
}

void transfer(FArchive& archive, RotationAxisConfig& value) {
    transfer_enum(archive, value.manual_semantic);
    archive << value.maximum_rate;
    archive << value.acceleration;
    transfer(archive, value.response);
    transfer(archive, value.stabilization);
}

void transfer(FArchive& archive, BrakeConfig& value) {
    transfer_bool(archive, value.available);
    archive << value.target_speed;
    archive << value.deceleration;
    archive << value.energy_drain_per_second;
    transfer(archive, value.response);
}

void transfer(FArchive& archive, FlightModelConfig& value) {
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

void transfer(FArchive& archive, FlightModelProfile& value) {
    transfer_enum(archive, value.base_preset);
    transfer_bool(archive, value.customized);
    transfer(archive, value.config);
}
} // namespace detail

auto encode(::ioj::sim::player::FlightModelProfile const& profile) -> FString {
    TArray<uint8> bytes;
    FMemoryWriter archive{bytes, true};
    uint32 version{1};
    archive << version;
    auto value{profile};
    detail::transfer(archive, value);
    return FBase64::Encode(bytes);
}

auto decode(FString const& data, ::ioj::sim::player::FlightModelProfile& profile) -> bool {
    TArray<uint8> bytes;
    if (data.IsEmpty() || !FBase64::Decode(data, bytes) || bytes.Num() > 1024) {
        return false;
    }
    FMemoryReader archive{bytes, true};
    uint32 version{};
    archive << version;
    if (version != 1) {
        return false;
    }
    auto candidate{profile};
    detail::transfer(archive, candidate);
    if (archive.IsError() || archive.Tell() != bytes.Num() ||
        !::ioj::sim::player::validate_flight_model_config(candidate.config)) {
        return false;
    }
    profile = candidate;
    return true;
}
} // namespace ml::ioj::flight_model_settings_codec
