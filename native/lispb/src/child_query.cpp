#include <lispb/schema/child_query.h>

#include <codegen/schema/declaration_schema.h>
#include <codegen/schema/declarations.h>
#include <lispb/strings.h>

namespace lispb::schema {

ChildQuery::ChildQuery(codegen::Declarations const& store,
                       std::optional<codegen::ChildOccurrenceId> child)
    : store_{&store}
    , child_{child} {}
auto ChildQuery::exists() const -> bool {
    return child_.has_value();
}
auto ChildQuery::name() const -> std::optional<std::string_view> {
    if (!child_) {
        return {};
    }
    return store_->strings().view(
        store_->child_occurrences.name[store_->child_occurrences.index(*child_)]);
}
auto ChildQuery::bits() const -> std::optional<std::uint32_t> {
    if (!child_) {
        return {};
    }
    return store_->child_occurrences.bits[store_->child_occurrences.index(*child_)];
}
auto ChildQuery::type() const -> std::optional<std::string_view> {
    if (!child_) {
        return {};
    }
    auto const type{store_->child_occurrences.type[store_->child_occurrences.index(*child_)]};
    return store_->strings().view(store_->type_refs.name[store_->type_refs.index(type)]);
}

}
