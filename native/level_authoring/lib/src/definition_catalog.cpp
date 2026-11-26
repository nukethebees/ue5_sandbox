#include <ioj/levels/authoring/definition_catalog.h>

#include <ioj/ascii.h>
#include <ioj/files.h>
#include <ioj/levels/authoring/level_catalog_category.h>

#include <algorithm>
#include <unordered_map>
#include <unordered_set>

namespace ioj::levels::authoring {
namespace {
struct EntryState {
    enum class Visit { Unvisited, Active, Complete };

    Diagnostics issues{};
    Visit visit{Visit::Unvisited};
};
void add_issue(EntryState& state, DiagnosticCode const code, std::string message) {
    if (std::ranges::none_of(state.issues,
                             [code](Diagnostic const& issue) { return issue.code == code; })) {
        state.issues.emplace_back(code, "catalog", std::move(message));
    }
}
template <typename Entries>
void invalidate(Entries& entries, std::vector<EntryState>& states) {
    auto const count{entries.size()};
    for (std::size_t i{}; i < count; ++i) {
        auto& errors{states[i].issues};
        if (!errors.empty()) {
            for (auto& error : errors) {
                error.source_path = entries[i].source_path;
            }
            entries[i].definition =
                decltype(entries[i].definition){std::unexpect, std::move(errors)};
        }
    }
}
void validate_levels(std::vector<DefinitionEntry<LevelDefinition>>& entries) {
    std::vector<EntryState> states(entries.size());
    std::unordered_map<LevelId, std::size_t> indices;
    auto const count{entries.size()};
    for (std::size_t index{}; index < count; ++index) {
        if (!entries[index].definition) {
            continue;
        }
        auto const& id{entries[index].definition->metadata().id};
        auto const [found, inserted]{indices.emplace(id, index)};
        if (!inserted) {
            auto const message{std::format("Level id '{}' is declared in '{}' and '{}'",
                                           id.value,
                                           path_to_utf8(entries[found->second].source_path),
                                           path_to_utf8(entries[index].source_path))};
            add_issue(states[index], DiagnosticCode::DuplicateLevelId, message);
            add_issue(states[found->second], DiagnosticCode::DuplicateLevelId, message);
        }
    }

    // Explicit frames bound host stack usage even for long catalog dependency chains.
    struct Frame {
        std::size_t index{};
        std::size_t next{};
    };
    std::vector<Frame> stack;
    for (std::size_t start{}; start < count; ++start) {
        if (!entries[start].definition || states[start].visit != EntryState::Visit::Unvisited ||
            !states[start].issues.empty()) {
            continue;
        }
        stack.emplace_back(start, 0);
        states[start].visit = EntryState::Visit::Active;
        while (!stack.empty()) {
            auto& frame{stack.back()};
            auto const dependencies{entries[frame.index].definition->unlock_level_ids()};
            if (frame.next == dependencies.size()) {
                states[frame.index].visit = EntryState::Visit::Complete;
                stack.pop_back();
                continue;
            }
            auto const& target{dependencies[frame.next++]};
            auto const found{indices.find(target)};
            if (found == indices.end() || !states[found->second].issues.empty()) {
                add_issue(states[frame.index],
                          DiagnosticCode::UnavailableLevel,
                          std::format("Requires unavailable level '{}'", target.value));
                continue;
            }
            auto const next{found->second};
            if (states[next].visit == EntryState::Visit::Active) {
                auto const cycle{std::ranges::find(stack, next, &Frame::index)};
                std::string cycle_path;
                for (auto position{cycle}; position != stack.end(); ++position) {
                    std::format_to(std::back_inserter(cycle_path),
                                   "{} -> ",
                                   entries[position->index].definition->metadata().id.value);
                }
                cycle_path.append(target.value);
                for (auto position{cycle}; position != stack.end(); ++position) {
                    add_issue(states[position->index],
                              DiagnosticCode::UnlockCycle,
                              std::format("Unlock dependency cycle: {}", cycle_path));
                }
            } else if (states[next].visit == EntryState::Visit::Unvisited) {
                states[next].visit = EntryState::Visit::Active;
                stack.emplace_back(next, 0);
            }
        }
    }

    bool changed{true};
    while (changed) {
        changed = false;
        for (std::size_t index{}; index < count; ++index) {
            if (!entries[index].definition || !states[index].issues.empty()) {
                continue;
            }
            for (auto const& dependency : entries[index].definition->unlock_level_ids()) {
                auto const found{indices.find(dependency)};
                if (found == indices.end() || !states[found->second].issues.empty()) {
                    add_issue(states[index],
                              DiagnosticCode::UnavailableLevel,
                              std::format("Requires unavailable level '{}'", dependency.value));
                    changed = true;
                    break;
                }
            }
        }
    }
    invalidate(entries, states);
}
void validate_campaigns(std::vector<DefinitionEntry<CampaignDefinition>>& campaigns,
                        std::vector<DefinitionEntry<LevelDefinition>> const& levels) {
    std::unordered_set<LevelId> available;
    for (auto const& level : levels) {
        if (level.definition) {
            available.insert(level.definition->metadata().id);
        }
    }
    std::unordered_map<CampaignId, std::size_t> indices;
    std::vector<EntryState> states(campaigns.size());
    auto const count{campaigns.size()};
    for (std::size_t index{}; index < count; ++index) {
        if (!campaigns[index].definition) {
            continue;
        }
        auto const& campaign{*campaigns[index].definition};
        auto const [found, inserted]{indices.emplace(campaign.id, index)};
        if (!inserted) {
            auto const message{
                std::format("Campaign id '{}' is declared more than once", campaign.id.value)};
            add_issue(states[index], DiagnosticCode::DuplicateCampaignId, message);
            add_issue(states[found->second], DiagnosticCode::DuplicateCampaignId, message);
        }
        for (auto const& id : campaign.level_ids) {
            if (!available.contains(id)) {
                add_issue(states[index],
                          DiagnosticCode::UnavailableLevel,
                          std::format("Campaign references unavailable level '{}'", id.value));
            }
        }
    }
    invalidate(campaigns, states);
}
}

auto catalog_category(LevelDefinition const& definition) noexcept -> LevelCatalogCategory {
    return definition.player_entity_id().empty() ? LevelCatalogCategory::BattleViewer
                                                 : LevelCatalogCategory::Mission;
}

DefinitionCatalog::DefinitionCatalog(
    std::vector<DefinitionEntry<LevelDefinition>> level_entries,
    std::vector<DefinitionEntry<CampaignDefinition>> campaign_entries)
    : levels{std::move(level_entries)}
    , campaigns{std::move(campaign_entries)} {
    validate_levels(levels);
    validate_campaigns(campaigns, levels);
    std::stable_sort(campaigns.begin(), campaigns.end(), [](auto const& lhs, auto const& rhs) {
        if (!lhs.definition || !rhs.definition) {
            return lhs.definition.has_value() && !rhs.definition.has_value();
        }
        auto const& left{*lhs.definition};
        auto const& right{*rhs.definition};
        if (equals_ignore_case(left.title, right.title)) {
            return left.id.value < right.id.value;
        }
        return std::ranges::lexicographical_compare(
            left.title, right.title, std::ranges::less{}, ioj::tolower, ioj::tolower);
    });
}
}
