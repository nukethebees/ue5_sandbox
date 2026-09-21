#pragma once

#include <cstdint>
#include <expected>
#include <limits>

namespace ioj::sim::player {
inline constexpr float effectively_unlimited_speed{std::numeric_limits<float>::max()};

enum class FlightModelPreset : std::uint8_t {
    Starfox,
    Fighter,
    Skater,
    Gunship,
};

enum class FlightModelSlot : std::uint8_t {
    Up,
    Right,
    Down,
    Left,
};

enum class TranslationSemantic : std::uint8_t {
    Disabled,
    TargetSpeed,
    TargetVelocity,
    Acceleration,
};

enum class RotationSemantic : std::uint8_t {
    Disabled,
    TargetAngularVelocity,
    AngularAcceleration,
};

enum class ReferenceFrame : std::uint8_t {
    Ship,
    World,
};

enum class TranslationInputSource : std::uint8_t {
    Axis,
    Accelerator,
};

enum class ResponseMode : std::uint8_t {
    Direct,
    RateLimited,
    SecondOrder,
};

enum class FacingVelocityCoupling : std::uint8_t {
    Independent,
    AlignToFacing,
    LockedToFacing,
};

struct RateLimitedResponseConfig {
    float increasing_rate{};
    float decreasing_rate{};

    auto operator==(RateLimitedResponseConfig const&) const -> bool = default;
};

struct SecondOrderResponseConfig {
    float settling_time{3.f};
    float damping_ratio{0.5f};

    auto operator==(SecondOrderResponseConfig const&) const -> bool = default;
};

struct ResponseConfig {
    ResponseMode mode{ResponseMode::Direct};
    RateLimitedResponseConfig rate_limited{};
    SecondOrderResponseConfig second_order{};

    auto operator==(ResponseConfig const&) const -> bool = default;
};

struct TranslationChannelConfig {
    TranslationSemantic semantic{TranslationSemantic::Disabled};
    ReferenceFrame reference_frame{ReferenceFrame::Ship};
    TranslationInputSource input_source{TranslationInputSource::Axis};
    float automatic_value{};
    ResponseConfig response{};

    auto operator==(TranslationChannelConfig const&) const -> bool = default;
};

struct TranslationDriveConfig {
    float positive_target_speed{};
    float negative_target_speed{};
    float positive_speed_limit{effectively_unlimited_speed};
    float negative_speed_limit{effectively_unlimited_speed};
    float positive_acceleration{};
    float negative_acceleration{};

    auto operator==(TranslationDriveConfig const&) const -> bool = default;
};

struct TranslationAxisConfig {
    TranslationChannelConfig manual{};
    TranslationChannelConfig automatic{};
    TranslationDriveConfig normal{};
    TranslationDriveConfig boosted{};
    float passive_drag{};
    ReferenceFrame passive_drag_reference_frame{ReferenceFrame::Ship};
    float active_stabilization_rate{};
    ReferenceFrame active_stabilization_reference_frame{ReferenceFrame::Ship};

    auto operator==(TranslationAxisConfig const&) const -> bool = default;
};

struct TranslationAxesConfig {
    TranslationAxisConfig forward{};
    TranslationAxisConfig right{};
    TranslationAxisConfig up{};

    auto operator==(TranslationAxesConfig const&) const -> bool = default;
};

struct RotationStabilizationConfig {
    bool enabled{};
    float target_angle{};
    float delay{};
    ResponseConfig response{};

    auto operator==(RotationStabilizationConfig const&) const -> bool = default;
};

struct RotationAxisConfig {
    RotationSemantic manual_semantic{RotationSemantic::Disabled};
    float maximum_rate{};
    float acceleration{};
    ResponseConfig response{};
    RotationStabilizationConfig stabilization{};

    auto operator==(RotationAxisConfig const&) const -> bool = default;
};

struct RotationAxesConfig {
    RotationAxisConfig pitch{};
    RotationAxisConfig yaw{};
    RotationAxisConfig roll{};

    auto operator==(RotationAxesConfig const&) const -> bool = default;
};

struct FacingVelocityConfig {
    FacingVelocityCoupling mode{FacingVelocityCoupling::Independent};
    float alignment_rate{};
    ResponseConfig response{};

    auto operator==(FacingVelocityConfig const&) const -> bool = default;
};

struct BoostConfig {
    bool available{};
    float energy_drain_per_second{};
    ResponseConfig response{};

    auto operator==(BoostConfig const&) const -> bool = default;
};

struct BrakeConfig {
    bool available{};
    float target_speed{};
    float deceleration{};
    float energy_drain_per_second{};
    ResponseConfig response{};

    auto operator==(BrakeConfig const&) const -> bool = default;
};

struct FlightModelConfig {
    TranslationAxesConfig translation{};
    RotationAxesConfig rotation{};
    FacingVelocityConfig facing_velocity{};
    BoostConfig boost{};
    BrakeConfig brake{};
    BrakeConfig emergency_brake{};
    float energy_recharge_per_second{};
    float maximum_resultant_speed{effectively_unlimited_speed};
    float boosted_maximum_resultant_speed{effectively_unlimited_speed};

    auto operator==(FlightModelConfig const&) const -> bool = default;
};

struct FlightModelProfile {
    FlightModelPreset base_preset{FlightModelPreset::Starfox};
    bool customized{};
    FlightModelConfig config{};

    auto operator==(FlightModelProfile const&) const -> bool = default;
};

struct FlightModelLoadout {
    FlightModelProfile up{};
    FlightModelProfile right{};
    FlightModelProfile down{};
    FlightModelProfile left{};
    FlightModelSlot initial_slot{FlightModelSlot::Up};

    auto operator==(FlightModelLoadout const&) const -> bool = default;
};

enum class FlightModelConfigError : std::uint8_t {
    InvalidEnumValue,
    NonFiniteValue,
    NegativeValue,
    InputValueOutOfRange,
    InvalidManualChannel,
    InvalidAutomaticChannel,
    AmbiguousTargetChannels,
    InvalidSecondOrderSettlingTime,
    InvalidSecondOrderDampingRatio,
};

[[nodiscard]] auto validate_flight_model_config(FlightModelConfig const& config) noexcept
    -> std::expected<void, FlightModelConfigError>;
[[nodiscard]] auto make_flight_model_profile(FlightModelPreset preset) -> FlightModelProfile;
[[nodiscard]] auto make_default_flight_model_loadout() -> FlightModelLoadout;
[[nodiscard]] auto flight_model_profile(FlightModelLoadout& loadout, FlightModelSlot slot) noexcept
    -> FlightModelProfile&;
[[nodiscard]] auto flight_model_profile(FlightModelLoadout const& loadout,
                                        FlightModelSlot slot) noexcept -> FlightModelProfile const&;
}
