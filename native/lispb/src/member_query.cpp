#include <lispb/schema/member_query.h>

#include <codegen/schema/declaration_schema.h>
#include <codegen/schema/declarations.h>
#include <lispb/strings.h>

namespace lispb::schema {

MemberQuery::MemberQuery(codegen::Declarations const& store,
                         std::optional<codegen::SoaMemberId> member)
    : store_{&store}
    , member_{member} {}
auto MemberQuery::exists() const -> bool {
    return member_.has_value();
}
auto MemberQuery::name() const -> std::optional<std::string_view> {
    if (!member_) {
        return {};
    }
    return store_->strings().view(store_->soa_members.name[store_->soa_members.index(*member_)]);
}
auto MemberQuery::type() const -> std::optional<std::string_view> {
    if (!member_) {
        return {};
    }
    auto const row{store_->soa_members.index(*member_)};
    if (store_->soa_members.kind[row] == codegen::SoaMemberKind::nested) {
        return {};
    }
    auto const type{store_->soa_members.type[row]};
    return store_->strings().view(store_->type_refs.name[store_->type_refs.index(type)]);
}

}
