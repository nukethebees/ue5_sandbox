#include <ioj/levels/catalog_validation.h>

#include <ioj/files.h>

#include <algorithm>
#include <cassert>
#include <format>
#include <unordered_map>
#include <unordered_set>

namespace ioj::levels {
namespace {
struct EntryState {
    Diagnostics issues{};
    enum class Visit { Unvisited, Active, Complete } visit{Visit::Unvisited};
};
void add_issue(EntryState& state, DiagnosticCode const code, std::string message) {
    if (std::ranges::none_of(state.issues,
                             [code](Diagnostic const& issue) { return issue.code == code; })) {
        state.issues.push_back({code, "catalog", std::move(message)});
    }
}
template <typename Definition>
auto collect(std::span<DecodedCatalogEntry<Definition> const> const entries,
             std::vector<EntryState> states) -> std::vector<CatalogValidationIssue> {
    std::vector<CatalogValidationIssue> result;
    std::unordered_set<CatalogEntryIndex> source_indices;
    auto const count{entries.size()};
    for (std::size_t index{}; index < count; ++index) {
        [[maybe_unused]] auto const inserted{
            source_indices.insert(entries[index].source_index).second};
        assert(inserted);
        for (auto& diagnostic : states[index].issues) {
            diagnostic.source_path = entries[index].source_path;
            result.push_back({entries[index].source_index, std::move(diagnostic)});
        }
    }
    return result;
}
}
auto validate_level_catalog(std::span<LevelCatalogEntry const> const entries)
    -> std::vector<CatalogValidationIssue> {
    std::vector<EntryState> states(entries.size());
    std::unordered_map<LevelId, std::size_t> indices;
    auto const count{entries.size()};
    for (std::size_t index{}; index < count; ++index) {
        auto const& id{entries[index].definition.metadata.id};
        auto const [found, inserted]{indices.emplace(id, index)};
        if (!inserted) {
            auto const message{std::format("Level id '{}' is declared in '{}' and '{}'",
                                           id.value,
                                           ioj::path_to_utf8(entries[found->second].source_path),
                                           ioj::path_to_utf8(entries[index].source_path))};
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
        if (states[start].visit != EntryState::Visit::Unvisited || !states[start].issues.empty()) {
            continue;
        }
        stack.push_back({start, 0});
        states[start].visit = EntryState::Visit::Active;
        while (!stack.empty()) {
            auto& frame{stack.back()};
            auto const& dependencies{entries[frame.index].definition.unlock_level_ids};
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
                                   entries[position->index].definition.metadata.id.value);
                }
                cycle_path.append(target.value);
                for (auto position{cycle}; position != stack.end(); ++position) {
                    add_issue(states[position->index],
                              DiagnosticCode::UnlockCycle,
                              std::format("Unlock dependency cycle: {}", cycle_path));
                }
            } else if (states[next].visit == EntryState::Visit::Unvisited) {
                states[next].visit = EntryState::Visit::Active;
                stack.push_back({next, 0});
            }
        }
    }

    bool changed{true};
    while (changed) {
        changed = false;
        for (std::size_t index{}; index < count; ++index) {
            if (!states[index].issues.empty()) {
                continue;
            }
            for (auto const& dependency : entries[index].definition.unlock_level_ids) {
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
    return collect(entries, std::move(states));
}
auto validate_campaign_catalog(std::span<CampaignCatalogEntry const> const campaigns,
                               std::span<LevelCatalogEntry const> const levels)
    -> std::vector<CatalogValidationIssue> {
    std::unordered_set<LevelId> available;
    for (auto const& level : levels) {
        available.insert(level.definition.metadata.id);
    }
    std::unordered_map<CampaignId, std::size_t> indices;
    std::vector<EntryState> states(campaigns.size());
    auto const count{campaigns.size()};
    for (std::size_t index{}; index < count; ++index) {
        auto const& campaign{campaigns[index].definition};
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
    return collect(campaigns, std::move(states));
}
}
