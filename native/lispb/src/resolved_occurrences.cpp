#include <lispb/schema/resolved_occurrences.h>

#include <cassert>

namespace lispb::schema {

auto ResolvedOccurrences::append() -> ResolvedOccurrenceId {
    auto const id{ResolvedOccurrenceId{static_cast<std::uint32_t>(type.size() + 1)}};
    type.push_back({});
    cpp_type.push_back({});
    physical_form.push_back(codegen::PhysicalTypeForm::unsupported);
    object_spelling.push_back({});
    names_semantic_type.push_back({});
    cv_qualified.push_back({});
    return id;
}
auto ResolvedOccurrences::index(ResolvedOccurrenceId const id) const -> std::uint32_t {
    assert(lispb::schema::is_valid(id) && id.value <= type.size());
    return id.value - 1;
}
auto ResolvedOccurrences::contains_value(ResolvedOccurrenceId const id) const -> bool {
    auto const row{index(id)};
    return physical_form[row] == codegen::PhysicalTypeForm::value && names_semantic_type[row];
}

} // namespace lispb::schema
