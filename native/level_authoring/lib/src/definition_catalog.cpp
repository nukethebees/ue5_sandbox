#include <ioj/levels/authoring/definition_catalog.h>

#include <ioj/ascii.h>
#include <ioj/levels/catalog_validation.h>

#include <algorithm>

namespace ioj::levels::authoring {
namespace {
template <typename Definition>
auto valid_entries(std::vector<DefinitionEntry<Definition>> const& entries)
    -> std::vector<DecodedCatalogEntry<Definition>> {
    std::vector<DecodedCatalogEntry<Definition>> result;
    auto const count{entries.size()};
    result.reserve(count);
    for (std::size_t index{}; index < count; ++index) {
        auto const& entry{entries[index]};
        if (entry.definition) {
            result.push_back({index, entry.source_path, *entry.definition});
        }
    }
    return result;
}

template <typename Definition>
void apply_issues(std::vector<DefinitionEntry<Definition>>& entries,
                  std::vector<CatalogValidationIssue> const& issues,
                  Diagnostics& diagnostics) {
    for (auto const& issue : issues) {
        auto& result{entries[issue.entry_index].definition};
        if (result) {
            result = std::unexpected{Diagnostics{}};
        }
        result.error().push_back(issue.diagnostic);
        diagnostics.push_back(issue.diagnostic);
    }
}
}

auto catalog_category(LevelDefinition const& definition) noexcept -> LevelCatalogCategory {
    return definition.player_entity_id().empty() ? LevelCatalogCategory::BattleViewer
                                                 : LevelCatalogCategory::Mission;
}

auto resolve_catalog(DefinitionCatalog& catalog) -> Diagnostics {
    Diagnostics diagnostics;
    apply_issues(
        catalog.levels, validate_level_catalog(valid_entries(catalog.levels)), diagnostics);
    apply_issues(
        catalog.campaigns,
        validate_campaign_catalog(valid_entries(catalog.campaigns), valid_entries(catalog.levels)),
        diagnostics);

    std::stable_sort(
        catalog.campaigns.begin(), catalog.campaigns.end(), [](auto const& lhs, auto const& rhs) {
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
    return diagnostics;
}
}
