#include <codegen/generator.h>
#include <codegen/manifest.h>

#include <iostream>
#include <stdexcept>

auto main(int argc, char** argv) -> int {
    try {
        if (argc != 3) {
            throw std::invalid_argument{"Expected source root and output directory"};
        }
        std::filesystem::path const root{argv[1]};
        std::filesystem::path const modules[]{root /
                                              "lispb/schema/single_allocation_experiment.lispb"};
        auto manifest{codegen::load_sources(root / "lispb/schema/types.lispb", modules)};
        codegen::SoaModuleSchema native;
        for (auto const& module : manifest.modules) {
            auto const* soa{std::get_if<codegen::SoaModuleSchema>(&module)};
            if (soa && soa->settings.name == "single_allocation_experiment") {
                native = *soa;
            }
        }
        if (native.structs.empty()) {
            throw std::runtime_error{"Missing canonical experimental schema"};
        }
        native.experimental_stdlib = true;
        native.experimental_array_allocators.clear();
        native.settings = {.name = "native_soa",
                           .header = "native_soa_types.h",
                           .source = {},
                           .header_include = {},
                           .namespace_name = "ml::native_experiment",
                           .include_order = {},
                           .prelude_lines = {}};
        for (auto& schema : native.structs) {
            schema.single_allocation_variants.clear();
            schema.export_specifier.reset();
            for (auto& member : schema.members) {
                auto& name{member.type.name};
                for (auto const* integer :
                     {"int8", "uint8", "int16", "uint16", "int32", "uint32", "int64", "uint64"}) {
                    if (name == integer) {
                        name = "std::" + name + "_t";
                        break;
                    }
                }
            }
        }
        for (auto& [key, type] : manifest.types) {
            if (key.starts_with("soa_experiment_")) {
                type.dependencies = {{type.spelling, "native_soa/leaf_types.h", {}}};
            }
        }
        manifest.modules = {std::move(native)};
        auto const files{codegen::render_modules(codegen::lower_modules(manifest))};
        return codegen::generate_files(files, root, argv[2], false);
    } catch (std::exception const& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
