#include <lispb/schema/module_query.h>

#include <codegen/schema/declaration_schema.h>
#include <codegen/schema/declarations.h>
#include <lispb/strings.h>

namespace lispb::schema {

ModuleQuery::ModuleQuery(codegen::Declarations const& declarations,
                         std::span<codegen::ModuleHandle const> modules,
                         std::string_view name)
    : declarations_{&declarations} {
    auto const& store{declarations};
    for (auto const module : modules) {
        auto const settings{store.module_settings(module)};
        if (store.strings().view(
                store.module_settings_store.name[store.module_settings_store.index(settings)]) ==
            name) {
            module_ = module;
            break;
        }
    }
}
auto ModuleQuery::exists() const -> bool {
    return module_.has_value();
}
ModuleQuery::ModuleQuery(codegen::Declarations const& declarations, codegen::ModuleHandle module)
    : declarations_{&declarations}
    , module_{module} {}
auto ModuleQuery::name() const -> std::string_view {
    if (!module_) {
        return {};
    }
    auto const& store{*declarations_};
    auto const settings{store.module_settings(*module_)};
    return store.strings().view(
        store.module_settings_store.name[store.module_settings_store.index(settings)]);
}
auto ModuleQuery::declarations() const -> std::vector<DeclarationQuery> {
    std::vector<DeclarationQuery> result;
    if (!module_ || module_->kind() != codegen::ModuleKind::normal) {
        return result;
    }
    auto const& store{*declarations_};
    for (auto const declaration :
         store.items(store.normal_modules.declarations[store.normal_modules.index(*module_)])) {
        result.emplace_back(store, declaration);
    }
    return result;
}
auto ModuleQuery::declaration(std::string_view name) const -> DeclarationQuery {
    for (auto const& declaration : declarations()) {
        if (declaration.name() == name) {
            return declaration;
        }
    }
    return {*declarations_, std::nullopt};
}
auto ModuleQuery::find(std::string_view name, codegen::DeclarationKind kind) const
    -> std::optional<codegen::DeclarationHandle> {
    if (!module_ || module_->kind() != codegen::ModuleKind::normal) {
        return {};
    }
    auto const& store{*declarations_};
    for (auto const declaration :
         store.items(store.normal_modules.declarations[store.normal_modules.index(*module_)])) {
        if (declaration.kind() == kind &&
            store.strings().view(codegen::declaration_name(store, declaration)) == name) {
            return declaration;
        }
    }
    return {};
}
auto ModuleQuery::record(std::string_view name) const -> RecordQuery {
    return {*declarations_, find(name, codegen::DeclarationKind::record)};
}
auto ModuleQuery::soa(std::string_view name) const -> SoaQuery {
    return {*declarations_, find(name, codegen::DeclarationKind::soa)};
}

}
