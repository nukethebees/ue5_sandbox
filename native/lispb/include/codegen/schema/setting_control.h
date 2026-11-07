#pragma once

#include <codegen/schema/setting_control_kind.h>
#include <lispb/indices.h>

#include <expected>
#include <optional>
#include <string_view>

namespace lispb::codegen {

struct SettingControlInput {
    SettingControlKind kind;
    std::optional<StringId> options_provider;
    std::optional<StringId> availability_provider;
    std::optional<double> minimum;
    std::optional<double> maximum;
    std::optional<double> step;
    std::optional<StringId> custom_row;
};

struct SettingControlRange {
    double minimum;
    double maximum;
    double step;
};

class SettingControl {
  public:
    [[nodiscard]] static auto try_make(SettingControlInput const& input)
        -> std::expected<SettingControl, std::string_view>;

    [[nodiscard]] auto kind() const noexcept -> SettingControlKind { return kind_; }
    [[nodiscard]] auto options_provider() const noexcept -> std::optional<StringId> {
        return options_provider_;
    }
    [[nodiscard]] auto availability_provider() const noexcept -> std::optional<StringId> {
        return availability_provider_;
    }
    [[nodiscard]] auto range() const noexcept -> SettingControlRange const* {
        return is_range(kind_) ? &range_ : nullptr;
    }
    [[nodiscard]] auto custom_row() const noexcept -> std::optional<StringId> {
        return kind_ == SettingControlKind::custom ? std::optional{custom_row_} : std::nullopt;
    }

  private:
    explicit SettingControl(SettingControlInput const& input)
        : kind_{input.kind}
        , options_provider_{input.options_provider}
        , availability_provider_{input.availability_provider} {}

    SettingControlKind kind_;
    std::optional<StringId> options_provider_;
    std::optional<StringId> availability_provider_;
    union {
        SettingControlRange range_{};
        StringId custom_row_;
    };
};

}
