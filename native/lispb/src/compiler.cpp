#include <lispb/compiler.h>

#include "lispb/compile_mode.h"
#include "lispb/rooted_path.h"
#include <codegen/generator.h>
#include <codegen/schema/type_registry.h>
#include <codegen/sexpr/indices.h>
#include <kernel_codegen/compiler.h>
#include <lispb/material_compiler.h>
#include <lispb/project.h>
#include <lispb/project_loader.h>
#include <lispb/schema/type_graph.h>
#include <lispb/target_id.h>
#include <lispb/target_kind.h>
#include <lispb/temporary_string_scope.h>
#include <material_gen/MaterialIR.h>
#include <slate_codegen/compiler.h>
#include <slate_codegen/source_options.h>

#include <cassert>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

namespace lispb {

Compiler::Compiler()
    : manifest_{
          1, lispb::codegen::TypeRegistry{storage_.registered_types}, storage_.declarations, {}}
    , types_{storage_.declarations, storage_.cpp_types}
    , cpp_{storage_.strings, storage_.cpp_types}
    , kernel_{storage_.strings}
    , material_{storage_.strings} {}

void Compiler::reset_compilation() {
    output_ = {};
    material_ = material_synth::MaterialIR{storage_.strings};
    slate_ = {};
    kernel_.clear();
    cpp_.clear();
    types_ = schema::TypeGraph{storage_.declarations, storage_.cpp_types};
    manifest_.modules.clear();
    manifest_.types.clear();
    storage_.registered_types.clear();
    storage_.declarations.clear();
    storage_.cpp_types.clear();
    storage_.syntax.clear();
    storage_.sources.clear_expansions();
    storage_.strings.reset_temporary();
    storage_.sources.refresh();
}

auto Compiler::compile(Project const& project,
                       TargetId const target,
                       std::filesystem::path const& build_root,
                       CompileMode const mode) -> CompiledTarget& {
    reset_compilation();
    TemporaryStringScope temporary_strings{storage_.strings};
    auto const emit{mode == CompileMode::generate};
    auto& compilation{output_.compilation};
    auto const output{resolve(project.output_path(target), project.root, build_root)};
    switch (target.kind()) {
        case TargetKind::cpp_schema: {
            auto sources{project.sources(target)};
            for (auto& source : sources) {
                source = project.root / source;
            }
            lispb::codegen::compile_schema(storage_,
                                           cpp_,
                                           manifest_,
                                           types_,
                                           compilation,
                                           project.root / project.types(target),
                                           sources,
                                           emit);
            output_.publication = {.path_base = project.root, .output_root = output};
            break;
        }
        case TargetKind::slate: {
            slate_codegen::compile_sources(
                storage_.syntax,
                slate_,
                cpp_,
                compilation,
                {.source_root = project.root,
                 .inputs = project.sources(target),
                 .include_directories = project.include_directories(target)},
                emit);
            output_.publication = {.path_base = output, .output_root = output};
            break;
        }
        case TargetKind::kernel: {
            auto const paths{project.sources(target)};
            std::vector<lispb::codegen::sexpr::SourceFileId> sources;
            sources.reserve(paths.size());
            for (auto const& source : paths) {
                auto const path{project.root / source};
                sources.push_back(storage_.sources.load(path));
                compilation.dependencies.push_back(path);
            }
            kernel_codegen::compile(storage_.syntax,
                                    kernel_,
                                    cpp_,
                                    compilation,
                                    sources,
                                    project.profile(target),
                                    emit);
            output_.publication = {.path_base = output, .output_root = output};
            break;
        }
        case TargetKind::material: {
            auto const paths{project.sources(target)};
            assert(paths.size() == 1);
            auto const source{project.root / paths.front()};
            auto const file{storage_.sources.load(source, paths.front().generic_string())};
            compile_material(storage_.syntax,
                             file,
                             material_,
                             compilation,
                             source,
                             project.root,
                             output.filename(),
                             emit);
            output_.publication = {.path_base = output.parent_path(),
                                   .output_root = output.parent_path(),
                                   .track_outputs = false};
            break;
        }
        case TargetKind::group:
        case TargetKind::none:
            assert(false && "Compile an expanded concrete target");
            break;
    }
    return output_;
}

auto Compiler::expand(Project const& project, TargetId const target) -> std::string {
    reset_compilation();
    TemporaryStringScope temporary_strings{storage_.strings};
    if (target.kind() != TargetKind::slate) {
        throw std::invalid_argument{"expand is only supported for Slate targets"};
    }
    return slate_codegen::expand_sources(
        storage_.syntax,
        {.source_root = project.root,
         .inputs = project.sources(target),
         .include_directories = project.include_directories(target)});
}

} // namespace lispb
