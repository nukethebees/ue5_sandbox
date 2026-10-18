#include <lispb/schema/packed_value_query.h>

#include <codegen/schema/declaration_schema.h>
#include <codegen/schema/declarations.h>
#include <lispb/strings.h>

namespace lispb::schema {

PackedValueQuery::PackedValueQuery(codegen::Declarations const& store,
                                   std::optional<codegen::DeclarationHandle> declaration)
    : store_{&store}
    , declaration_{declaration} {}
auto PackedValueQuery::exists() const -> bool {
    return declaration_.has_value();
}
auto PackedValueQuery::storage_type() const -> std::optional<std::string_view> {
    if (!declaration_) {
        return {};
    }
    auto const type{store_->packed_values.storage_type[store_->packed_values.index(*declaration_)]};
    return store_->strings().view(store_->type_refs.name[store_->type_refs.index(type)]);
}
auto PackedValueQuery::fields() const -> std::vector<FieldQuery> {
    std::vector<FieldQuery> result;
    if (!declaration_) {
        return result;
    }
    for (auto const segment : store_->items(
             store_->packed_values.segments[store_->packed_values.index(*declaration_)])) {
        if (segment.kind() == codegen::PackedSegmentKind::field) {
            result.emplace_back(*store_, segment);
        }
    }
    return result;
}
auto PackedValueQuery::field(std::string_view name) const -> FieldQuery {
    for (auto const field : fields()) {
        if (field.name() == name) {
            return field;
        }
    }
    return {*store_, {}};
}

}
