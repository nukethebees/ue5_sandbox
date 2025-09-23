#pragma once

#include <optional>
#include <variant>

// Row data structure for variant-based storage
template <typename Config>
struct RowData {
    using SettingT = typename Config::SettingT;

    Config const * config{nullptr};
    SettingT current_value{};
    std::optional<SettingT> pending_value{std::nullopt};

    RowData() = default;
    RowData(Config const * cfg, SettingT current)
        : config{cfg}
        , current_value{current} {}

    bool has_pending_change() const { return pending_value.has_value(); }
    void set_pending_value(SettingT value) { pending_value = value; }
    void reset_pending() { pending_value = std::nullopt; }
    SettingT get_display_value() const { return pending_value.value_or(current_value); }
};

template <typename... Args>
using VideoRowSettingsVariantT = std::variant<RowData<Args>...>;