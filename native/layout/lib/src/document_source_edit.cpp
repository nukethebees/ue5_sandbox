#include <ioj/layout/document_state.hpp>
#include <ioj/layout/editor_session.hpp>

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
    auto const name_id{storage().strings.find(name)};
    auto const found{std::ranges::find_if(source_order, [&](auto file) {
        return storage().strings.find(storage().sources.path(file)) == name_id;
    })};
    if (operation == "add") {
        if (found != source_order.end()) {
            throw std::invalid_argument{"Source is already registered: " + name};
        }
        auto const file{storage().sources.add(name, text)};
        auto const parsed{lispb::codegen::parse_module_source(storage(), file)};
        for (auto const module : parsed.modules) {
            source_files.emplace(module, file);
            candidate.modules.push_back(module);
            if (module.kind() == lispb::codegen::ModuleKind::normal) {
                auto const& store{storage().declarations};
                for (auto const declaration : store.items(
                         store.normal_modules.declarations[store.normal_modules.index(module)])) {
                    register_declaration(declaration);
                }
            }
        }
        source_order.push_back(file);
        source_ids.emplace(file, SourceId{current_instruction_id, ++source_creation_slot});
        comments.capture(storage(), parsed.modules, file);
        return;
    }

    if (found == source_order.end()) {
        throw std::invalid_argument{"Unknown source: " + name};
    }
    auto const previous{*found};
    if (operation == "replace") {
        auto& store{storage().declarations};
        std::map<std::pair<lispb::StringId, lispb::StringId>, lispb::schema::DeclarationId> old_ids;
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
            auto const module_name{
                store.module_settings_store.name[store.module_settings_store.index(settings)]};
            for (auto const declaration : store.items(
                     store.normal_modules.declarations[store.normal_modules.index(module)])) {
                old_ids.emplace(
                    std::pair{module_name, lispb::codegen::declaration_name(store, declaration)},
                    declaration_ids.at(declaration));
            }
        }
        auto const file{storage().sources.add(name, text)};
        auto const parsed{lispb::codegen::parse_module_source(storage(), file)};
        for (auto const module : parsed.modules) {
            source_files.emplace(module, file);
            if (module.kind() != lispb::codegen::ModuleKind::normal) {
                continue;
            }
            auto const settings{store.module_settings(module)};
            auto const module_name{
                store.module_settings_store.name[store.module_settings_store.index(settings)]};
            for (auto const declaration : store.items(
                     store.normal_modules.declarations[store.normal_modules.index(module)])) {
                auto const key{
                    std::pair{module_name, lispb::codegen::declaration_name(store, declaration)}};
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
            analysis_settings.erase_declaration(id);
        }
        comments.capture(storage(), parsed.modules, file);
        source_ids.emplace(file, source_ids.at(previous));
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
    auto const destination_id{storage().strings.find(destination)};
    if (name == destination) {
        return;
    }
    if (std::ranges::any_of(source_order, [&](auto file) {
            return storage().strings.find(storage().sources.path(file)) == destination_id;
        })) {
        throw std::invalid_argument{"Source is already registered: " + destination};
    }
    auto const file{storage().sources.add(destination, storage().sources.text(previous))};
    source_ids.emplace(file, source_ids.at(previous));
    auto const render_origin{render_origins.find(previous)};
    render_origins.emplace(
        file, render_origin == render_origins.end() ? previous : render_origin->second);
    *found = file;
    for (auto const module : candidate.modules) {
        if (source_files.at(module) != previous) {
            continue;
        }
        source_files[module] = file;
    }
}
}
