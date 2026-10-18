#include "document_state.hpp"

#include <codegen/schema/declaration_schema.h>
#include <codegen/source_loader.h>

#include <algorithm>
#include <cassert>
#include <stdexcept>

namespace ioj::layout {
namespace source_edit {
auto normalized(std::string_view path) -> std::string {
    auto const value{std::filesystem::path{path}.lexically_normal()};
    if (value.empty() || value == "." || value.has_root_path() ||
        std::ranges::find(value, std::filesystem::path{".."}) != value.end()) {
        throw std::invalid_argument{"Source path must be relative and stay within the project"};
    }
    return value.generic_string();
}
}

void DocumentState::edit_source(std::string_view operation,
                                std::string_view path,
                                std::string_view text,
                                lispb::codegen::Manifest& candidate) {
    auto const name{source_edit::normalized(path)};
    auto const found{std::ranges::find_if(
        source_order, [&](auto file) { return storage.sources.path(file) == name; })};
    if (operation == "add") {
        if (found != source_order.end()) {
            throw std::invalid_argument{"Source is already registered: " + name};
        }
        auto const file{storage.sources.add(name, text)};
        auto const parsed{lispb::codegen::parse_module_source(storage, file)};
        for (auto const module : parsed.modules) {
            source_files.emplace(module, file);
            candidate.modules.push_back(module);
            if (module.kind() == lispb::codegen::ModuleKind::normal) {
                auto const& store{storage.declarations};
                for (auto const declaration : store.items(
                         store.normal_modules.declarations[store.normal_modules.index(module)])) {
                    register_declaration(declaration);
                }
            }
        }
        source_order.push_back(file);
        io.source_origins.emplace(file, file);
        comments.capture(storage, parsed.modules);
        return;
    }

    if (found == source_order.end()) {
        throw std::invalid_argument{"Unknown source: " + name};
    }
    auto const previous{*found};
    if (operation == "replace") {
        auto& store{storage.declarations};
        std::map<std::pair<std::string, std::string>, lispb::schema::DeclarationId> old_ids;
        auto first{candidate.modules.size()};
        auto const module_count{candidate.modules.size()};
        for (std::size_t index{}; index < module_count; ++index) {
            auto const module{candidate.modules[index]};
            if (source_files.at(module) != previous) {
                continue;
            }
            first = std::min(first, index);
            if (module.kind() != lispb::codegen::ModuleKind::normal) {
                continue;
            }
            auto const settings{store.module_settings(module)};
            auto const module_name{std::string{storage.strings.view(
                store.module_settings_store.name[store.module_settings_store.index(settings)])}};
            for (auto const declaration : store.items(
                     store.normal_modules.declarations[store.normal_modules.index(module)])) {
                old_ids.emplace(
                    std::pair{module_name,
                              std::string{storage.strings.view(
                                  lispb::codegen::declaration_name(store, declaration))}},
                    declaration_ids.at(declaration));
            }
        }
        auto const file{storage.sources.add(name, text)};
        auto const parsed{lispb::codegen::parse_module_source(storage, file)};
        for (auto const module : parsed.modules) {
            source_files.emplace(module, file);
            if (module.kind() != lispb::codegen::ModuleKind::normal) {
                continue;
            }
            auto const settings{store.module_settings(module)};
            auto const module_name{std::string{storage.strings.view(
                store.module_settings_store.name[store.module_settings_store.index(settings)])}};
            for (auto const declaration : store.items(
                     store.normal_modules.declarations[store.normal_modules.index(module)])) {
                auto const key{
                    std::pair{module_name,
                              std::string{storage.strings.view(
                                  lispb::codegen::declaration_name(store, declaration))}}};
                auto const prior{old_ids.find(key)};
                if (prior == old_ids.end()) {
                    register_declaration(declaration);
                } else {
                    declaration_ids.emplace(declaration, prior->second);
                    old_ids.erase(prior);
                }
            }
        }
        for (auto const& [unused_name, id] : old_ids) {
            variants.erase_declaration(id);
        }
        comments.capture(storage, parsed.modules, file);
        io.source_origins.emplace(file, io.source_origins.at(previous));
        *found = file;
        std::erase_if(candidate.modules,
                      [&](auto module) { return source_files.at(module) == previous; });
        candidate.modules.insert(candidate.modules.begin() + static_cast<std::ptrdiff_t>(first),
                                 parsed.modules.begin(),
                                 parsed.modules.end());
        return;
    }
    if (operation == "remove") {
        std::erase_if(candidate.modules,
                      [&](auto module) { return source_files.at(module) == previous; });
        source_order.erase(found);
        return;
    }

    assert(operation == "rename");
    auto const destination{source_edit::normalized(text)};
    if (name == destination) {
        return;
    }
    if (std::ranges::any_of(source_order,
                            [&](auto file) { return storage.sources.path(file) == destination; })) {
        throw std::invalid_argument{"Source is already registered: " + destination};
    }
    auto const file{storage.sources.add(destination, storage.sources.text(previous))};
    io.source_origins.emplace(file, io.source_origins.at(previous));
    auto const render_origin{io.render_origins.find(previous)};
    io.render_origins.emplace(
        file, render_origin == io.render_origins.end() ? previous : render_origin->second);
    *found = file;
    auto& store{storage.declarations};
    for (auto& module : candidate.modules) {
        if (source_files.at(module) != previous) {
            continue;
        }
        auto const original{module};
        switch (module.kind()) {
            case lispb::codegen::ModuleKind::normal:
                module = store.normal_modules.clone(module);
                break;
            case lispb::codegen::ModuleKind::settings:
                module = store.settings_modules.clone(module);
                break;
            case lispb::codegen::ModuleKind::umbrella:
                module = store.umbrella_modules.clone(module);
                break;
        }
        inherit_module(original, module);
        source_files[module] = file;
    }
}
}
