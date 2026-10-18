#include <lispb/schema/declaration_query.h>

#include <codegen/schema/declaration_schema.h>
#include <codegen/schema/declarations.h>
#include <lispb/schema/source_renderer.h>
#include <lispb/strings.h>

namespace lispb::schema {
DeclarationQuery::DeclarationQuery(codegen::Declarations const& declarations,
                                   std::optional<codegen::DeclarationHandle> declaration)
    : declarations_{&declarations}
    , declaration_{declaration} {}
auto DeclarationQuery::exists() const -> bool {
    return declaration_.has_value();
}
auto DeclarationQuery::name() const -> std::string_view {
    return declaration_ ? declarations_->strings().view(
                              codegen::declaration_name(*declarations_, *declaration_))
                        : std::string_view{};
}
auto DeclarationQuery::kind() const -> std::optional<codegen::DeclarationKind> {
    return declaration_ ? std::optional{declaration_->kind()} : std::nullopt;
}
auto DeclarationQuery::source() const -> std::optional<std::string> {
    return declaration_ ? std::optional{render_declaration_source(*declarations_, *declaration_)}
                        : std::nullopt;
}
}
