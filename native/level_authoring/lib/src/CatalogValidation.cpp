#include <sandbox/level_authoring/CatalogValidation.h>

#include <algorithm>
#include <cstdint>
#include <functional>
#include <unordered_map>
#include <unordered_set>

namespace ml::level_authoring {
namespace {
template <typename Entry>
auto initial_invalid(std::vector<Entry> const& entries) -> std::vector<bool> {
    std::vector<bool> result;
    result.reserve(entries.size());
    for (auto const& entry : entries) {
        result.push_back(!entry.definition.has_value());
    }
    return result;
}

auto collect_issues(std::vector<std::optional<std::string>> const& messages)
    -> std::vector<CatalogValidationIssue> {
    std::vector<CatalogValidationIssue> result;
    for (std::size_t index{}; index < messages.size(); ++index) {
        if (messages[index]) {
            result.push_back({index, *messages[index]});
        }
    }
    return result;
}

void set_issue(std::size_t const index,
               std::string message,
               std::vector<bool>& invalid,
               std::vector<std::optional<std::string>>& messages) {
    invalid[index] = true;
    if (!messages[index]) {
        messages[index] = std::move(message);
    }
}

auto available_level_indices(std::vector<LevelCatalogEntry> const& entries,
                             std::vector<bool> const& invalid)
    -> std::unordered_map<std::string, std::size_t> {
    std::unordered_map<std::string, std::size_t> result;
    for (std::size_t index{}; index < entries.size(); ++index) {
        if (!invalid[index]) {
            result.emplace(entries[index].definition->metadata.id, index);
        }
    }
    return result;
}

void invalidate_duplicate_levels(std::vector<LevelCatalogEntry> const& entries,
                                 std::vector<bool>& invalid,
                                 std::vector<std::optional<std::string>>& messages) {
    std::unordered_map<std::string, std::size_t> first_indices;
    for (std::size_t index{}; index < entries.size(); ++index) {
        if (invalid[index]) {
            continue;
        }
        auto const& id{entries[index].definition->metadata.id};
        auto const [found, inserted]{first_indices.emplace(id, index)};
        if (inserted) {
            continue;
        }

        auto const first_index{found->second};
        auto const message{"Level id '" + id + "' is declared by both '" +
                           entries[first_index].filename + "' and '" + entries[index].filename +
                           "'."};
        set_issue(first_index, message, invalid, messages);
        set_issue(index, message, invalid, messages);
    }
}

void invalidate_unavailable_unlocks(std::vector<LevelCatalogEntry> const& entries,
                                    std::vector<bool>& invalid,
                                    std::vector<std::optional<std::string>>& messages) {
    bool changed{true};
    while (changed) {
        changed = false;
        auto const available{available_level_indices(entries, invalid)};
        for (std::size_t index{}; index < entries.size(); ++index) {
            if (invalid[index]) {
                continue;
            }
            auto const& definition{*entries[index].definition};
            for (auto const& target : definition.unlock_level_ids) {
                if (available.contains(target)) {
                    continue;
                }
                set_issue(index,
                          "Level '" + definition.metadata.id + "' requires unavailable level '" +
                              target + "'.",
                          invalid,
                          messages);
                changed = true;
                break;
            }
        }
    }
}

void invalidate_unlock_cycles(std::vector<LevelCatalogEntry> const& entries,
                              std::vector<bool>& invalid,
                              std::vector<std::optional<std::string>>& messages) {
    auto const indices{available_level_indices(entries, invalid)};
    std::unordered_map<std::string, std::uint8_t> visit_states;
    std::vector<std::string> stack;

    std::function<void(std::string const&)> visit = [&](std::string const& id) {
        visit_states[id] = 1;
        stack.push_back(id);

        auto const entry_index{indices.at(id)};
        for (auto const& target : entries[entry_index].definition->unlock_level_ids) {
            auto const state{visit_states[target]};
            if (state == 0) {
                visit(target);
                continue;
            }
            if (state != 1) {
                continue;
            }

            auto const cycle_start{std::ranges::find(stack, target)};
            std::string message{"Unlock dependency cycle: "};
            for (auto current{cycle_start}; current != stack.end(); ++current) {
                if (current != cycle_start) {
                    message += " -> ";
                }
                message += *current;
            }
            message += " -> " + target + ".";
            for (auto current{cycle_start}; current != stack.end(); ++current) {
                set_issue(indices.at(*current), message, invalid, messages);
            }
        }

        stack.pop_back();
        visit_states[id] = 2;
    };

    for (std::size_t index{}; index < entries.size(); ++index) {
        if (invalid[index]) {
            continue;
        }
        auto const& id{entries[index].definition->metadata.id};
        if (visit_states[id] == 0) {
            visit(id);
        }
    }
}
} // namespace

auto validate_level_catalog(std::vector<LevelCatalogEntry> const& entries)
    -> std::vector<CatalogValidationIssue> {
    auto invalid{initial_invalid(entries)};
    std::vector<std::optional<std::string>> messages(entries.size());
    invalidate_duplicate_levels(entries, invalid, messages);
    invalidate_unavailable_unlocks(entries, invalid, messages);
    invalidate_unlock_cycles(entries, invalid, messages);
    invalidate_unavailable_unlocks(entries, invalid, messages);
    return collect_issues(messages);
}

auto validate_campaign_catalog(std::vector<CampaignCatalogEntry> const& campaigns,
                               std::vector<LevelCatalogEntry> const& levels,
                               std::vector<CatalogValidationIssue> const& level_issues)
    -> std::vector<CatalogValidationIssue> {
    auto invalid{initial_invalid(campaigns)};
    std::vector<std::optional<std::string>> messages(campaigns.size());
    std::unordered_map<std::string, std::size_t> first_indices;
    for (std::size_t index{}; index < campaigns.size(); ++index) {
        if (invalid[index]) {
            continue;
        }
        auto const& id{campaigns[index].definition->id};
        auto const [found, inserted]{first_indices.emplace(id, index)};
        if (inserted) {
            continue;
        }
        auto const first_index{found->second};
        auto const message{"Campaign id '" + id + "' is declared by both '" +
                           campaigns[first_index].filename + "' and '" + campaigns[index].filename +
                           "'."};
        set_issue(first_index, message, invalid, messages);
        set_issue(index, message, invalid, messages);
    }

    auto level_invalid{initial_invalid(levels)};
    for (auto const& issue : level_issues) {
        level_invalid[issue.entry_index] = true;
    }
    auto const available_levels{available_level_indices(levels, level_invalid)};
    for (std::size_t index{}; index < campaigns.size(); ++index) {
        if (invalid[index]) {
            continue;
        }
        auto const& campaign{*campaigns[index].definition};
        for (auto const& level_id : campaign.level_ids) {
            if (available_levels.contains(level_id)) {
                continue;
            }
            set_issue(index,
                      "Campaign '" + campaign.id + "' references unavailable level '" + level_id +
                          "'.",
                      invalid,
                      messages);
            break;
        }
    }
    return collect_issues(messages);
}
} // namespace ml::level_authoring
