#include <ioj/layout/analysis_settings.hpp>

namespace ioj::layout {
void AnalysisSettings::erase_declaration(lispb::schema::DeclarationId declaration) {
    access_sets.erase(declaration);
    varint_distributions.erase(declaration);
    union_distributions.erase(declaration);
    tagged_union_distributions.erase(declaration);
}
void AnalysisSettings::rename_member(lispb::schema::DeclarationId declaration, std::string_view before, std::string_view after) {
    auto const found{access_sets.find(declaration)};
    if (found == access_sets.end()) { return; }
    auto const member{found->second.find(before)};
    if (member == found->second.end()) { return; }
    auto const operation{member->second};
    found->second.erase(member);
    found->second.emplace(after, operation);
}
void AnalysisSettings::erase_member(lispb::schema::DeclarationId declaration, std::string_view name) {
    auto const found{access_sets.find(declaration)};
    if (found != access_sets.end()) { found->second.erase(std::string{name}); }
}
}
