#include <lispb/schema/module_query.h>

#include <codegen/schema/declaration_schema.h>
#include <codegen/schema/declarations.h>
#include <lispb/strings.h>

namespace lispb::schema {

ModuleQuery::ModuleQuery(codegen::Manifest const& manifest, std::string_view name)
    : manifest_{&manifest} {
    auto const& store{manifest.declarations};
    for (auto const module : manifest.modules) {
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
ModuleQuery::ModuleQuery(codegen::Manifest const& manifest, codegen::ModuleHandle module)
    : manifest_{&manifest}
    , module_{module} {}
auto ModuleQuery::name() const -> std::string_view {
    if (!module_) {
        return {};
    }
    auto const& store{manifest_->declarations};
    auto const settings{store.module_settings(*module_)};
    return store.strings().view(
        store.module_settings_store.name[store.module_settings_store.index(settings)]);
}
auto ModuleQuery::declarations() const -> std::vector<DeclarationQuery> {
    std::vector<DeclarationQuery> result;
    if (!module_ || module_->kind() != codegen::ModuleKind::normal) {
        return result;
    }
    auto const& store{manifest_->declarations};
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
    return {manifest_->declarations, std::nullopt};
}
auto ModuleQuery::find(std::string_view name, codegen::DeclarationKind kind) const
    -> std::optional<codegen::DeclarationHandle> {
    if (!module_ || module_->kind() != codegen::ModuleKind::normal) {
        return {};
    }
    auto const& store{manifest_->declarations};
    for (auto const declaration :
         store.items(store.normal_modules.declarations[store.normal_modules.index(*module_)])) {
        if (declaration.kind() == kind &&
            store.strings().view(codegen::declaration_name(store, declaration)) == name) {
            return declaration;
        }
    }
    return {};
}
auto ModuleQuery::packed_value(std::string_view name) const -> PackedValueQuery {
    return {manifest_->declarations, find(name, codegen::DeclarationKind::packed_value)};
}
auto ModuleQuery::soa(std::string_view name) const -> SoaQuery {
    return {manifest_->declarations, find(name, codegen::DeclarationKind::soa)};
}

}
