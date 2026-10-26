#include <lispb/schema/record_query.h>

#include <codegen/schema/declaration_schema.h>
#include <codegen/schema/declarations.h>
#include <lispb/strings.h>

namespace lispb::schema {

RecordQuery::RecordQuery(codegen::Declarations const& store,
                         std::optional<codegen::DeclarationHandle> declaration)
    : store_{&store}
    , declaration_{declaration} {}
auto RecordQuery::exists() const -> bool {
    return declaration_.has_value();
}
auto RecordQuery::storage_type() const -> std::optional<std::string_view> {
    if (!declaration_) {
        return {};
    }
    auto const& projection{store_->records.cpp_projection[store_->records.index(*declaration_)]};
    if (!projection || !projection->storage) {
        return {};
    }
    auto const type{*projection->storage};
    return store_->strings().view(store_->type_refs.name[store_->type_refs.index(type)]);
}
auto RecordQuery::members() const -> std::vector<ChildQuery> {
    std::vector<ChildQuery> result;
    if (!declaration_) {
        return result;
    }
    for (auto const segment :
         store_->items(store_->records.members[store_->records.index(*declaration_)])) {
        if (store_->child_occurrences.kind[store_->child_occurrences.index(segment)] ==
            codegen::ChildOccurrenceKind::value) {
            result.emplace_back(*store_, segment);
        }
    }
    return result;
}
auto RecordQuery::member(std::string_view name) const -> ChildQuery {
    for (auto const field : members()) {
        if (field.name() == name) {
            return field;
        }
    }
    return {*store_, {}};
}

}
