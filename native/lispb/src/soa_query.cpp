#include <lispb/schema/soa_query.h>

#include <codegen/schema/declaration_schema.h>
#include <codegen/schema/declarations.h>
#include <lispb/strings.h>

namespace lispb::schema {

SoaQuery::SoaQuery(codegen::Declarations const& store,
                   std::optional<codegen::DeclarationHandle> declaration)
    : store_{&store}
    , declaration_{declaration} {}
auto SoaQuery::exists() const -> bool {
    return declaration_.has_value();
}
auto SoaQuery::members() const -> std::vector<MemberQuery> {
    std::vector<MemberQuery> result;
    if (!declaration_) {
        return result;
    }
    for (auto const member :
         store_->items(store_->soas.members[store_->soas.index(*declaration_)])) {
        result.emplace_back(*store_, member);
    }
    return result;
}
auto SoaQuery::member(std::string_view name) const -> MemberQuery {
    for (auto const member : members()) {
        if (member.name() == name) {
            return member;
        }
    }
    return {*store_, {}};
}

}
