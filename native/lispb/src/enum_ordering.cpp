#include "enum_ordering.h"

#include <codegen/schema/declarations.h>
#include <lispb/strings.h>

#include <algorithm>
#include <format>
#include <span>
#include <stdexcept>
#include <vector>

namespace lispb::codegen {
void order_enum_values(Declarations& declarations, DeclarationHandle const enumeration) {
    auto& enums{declarations.enums};
    auto const& enumerators{declarations.enumerators};
    auto const source{declarations.items(enums.values[enumeration])};
    std::vector<EnumeratorId> const original{source.begin(), source.end()};
    std::vector<StringId> groups;
    for (auto const value : original) {
        auto const memberships{declarations.items(enumerators.groups[value])};
        std::vector<StringId> seen;
        for (auto const group : memberships) {
            if (std::ranges::contains(seen, group)) {
                throw std::invalid_argument{"Duplicate enumerator group"};
            }
            seen.push_back(group);
            if (!std::ranges::contains(groups, group)) {
                groups.push_back(group);
            }
        }
    }

    auto const mode{enums.ordering[enumeration]};
    if (mode == EnumOrdering::manual) {
        return;
    }

    auto const requested{declarations.items(enums.group_order[enumeration])};
    std::vector<StringId> priorities{requested.begin(), requested.end()};
    if (mode == EnumOrdering::priority) {
        std::vector<StringId> seen;
        for (auto const group : priorities) {
            if (!std::ranges::contains(groups, group)) {
                throw std::invalid_argument{std::format("Undefined enum ordering group '{}'",
                                                        declarations.strings().view(group))};
            }
            if (std::ranges::contains(seen, group)) {
                throw std::invalid_argument{"Duplicate enum ordering group"};
            }
            seen.push_back(group);
        }
        if (priorities.size() != groups.size()) {
            throw std::invalid_argument{"Enum ordering must list every group"};
        }
    } else {
        priorities = groups;
    }

    auto contains = [&](EnumeratorId const value, StringId const group) {
        return std::ranges::contains(declarations.items(enumerators.groups[value]), group);
    };
    auto arrange = [&](std::span<StringId const> const priority) {
        auto values{original};
        std::stable_sort(values.begin(), values.end(), [&](auto const left, auto const right) {
            auto const count{enums.count[enumeration]};
            if (count) {
                auto const left_count{enumerators.name[left] == *count};
                auto const right_count{enumerators.name[right] == *count};
                if (left_count != right_count) {
                    return right_count;
                }
            }
            for (auto const group : priority) {
                auto const left_member{contains(left, group)};
                auto const right_member{contains(right, group)};
                if (left_member != right_member) {
                    return left_member;
                }
            }
            return false;
        });
        return values;
    };
    auto fragmentation = [&](std::span<EnumeratorId const> const values) {
        std::size_t ranges{};
        for (auto const group : groups) {
            bool previous{};
            for (auto const value : values) {
                auto const member{contains(value, group)};
                ranges += member && !previous;
                previous = member;
            }
        }
        return ranges;
    };

    auto ordered{arrange(priorities)};
    if (mode == EnumOrdering::auto_) {
        auto score{fragmentation(ordered)};
        auto const group_count{priorities.size()};
        // Improve group priorities with deterministic swaps; retain stable member ties.
        bool improved{true};
        while (improved) {
            improved = false;
            for (std::size_t first{}; first < group_count; ++first) {
                for (auto second{first + 1}; second < group_count; ++second) {
                    std::swap(priorities[first], priorities[second]);
                    auto candidate{arrange(priorities)};
                    auto const candidate_score{fragmentation(candidate)};
                    if (candidate_score < score) {
                        ordered = std::move(candidate);
                        score = candidate_score;
                        improved = true;
                    } else {
                        std::swap(priorities[first], priorities[second]);
                    }
                }
            }
        }
        if (fragmentation(original) <= score) {
            ordered = original;
        }
    }

    enums.declared_values[enumeration] = enums.values[enumeration];
    enums.values[enumeration] = declarations.range(ordered);
}
}
