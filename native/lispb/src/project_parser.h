#pragma once

#include <codegen/sexpr/handle_range.h>
#include <codegen/sexpr/parser.h>
#include <lispb/kernel_profile.h>
#include <lispb/target_id.h>

#include <filesystem>
#include <string_view>
#include <vector>

namespace lispb {
class Project;

class ProjectParser {
  public:
    explicit ProjectParser(codegen::sexpr::Syntax& syntax);
    auto parse(codegen::sexpr::SourceFileId file,
               codegen::sexpr::HandleRange forms,
               std::filesystem::path const& project_path) -> Project;
  private:
    auto paths(Project& project, codegen::sexpr::FormId form, std::string_view purpose)
        -> codegen::sexpr::HandleRange;
    auto names(codegen::sexpr::FormId form, std::string_view purpose) -> std::vector<IdentifierId>;
    void output_path(Project& project,
                     TargetId target,
                     codegen::sexpr::FormId form,
                     std::string_view purpose);
    auto parse_profile(codegen::sexpr::FormId form) -> KernelProfile;
    codegen::sexpr::Syntax& syntax_;
    codegen::sexpr::Parser parser_;
};

}
