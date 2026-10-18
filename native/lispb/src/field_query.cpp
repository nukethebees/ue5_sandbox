#include <lispb/schema/field_query.h>

#include <codegen/schema/declaration_schema.h>
#include <codegen/schema/declarations.h>
#include <lispb/strings.h>

namespace lispb::schema {

FieldQuery::FieldQuery(codegen::Declarations const& store,
                       std::optional<codegen::PackedSegmentId> field)
    : store_{&store}
    , field_{field} {}
auto FieldQuery::exists() const -> bool {
    return field_.has_value();
}
auto FieldQuery::name() const -> std::optional<std::string_view> {
    if (!field_) {
        return {};
    }
    return store_->strings().view(store_->packed_fields.name[store_->packed_fields.index(*field_)]);
}
auto FieldQuery::bits() const -> std::optional<int> {
    if (!field_) {
        return {};
    }
    return store_->packed_fields.bits[store_->packed_fields.index(*field_)];
}
auto FieldQuery::type() const -> std::optional<std::string_view> {
    if (!field_) {
        return {};
    }
    auto const type{store_->packed_fields.type[store_->packed_fields.index(*field_)]};
    return store_->strings().view(store_->type_refs.name[store_->type_refs.index(type)]);
}

}
