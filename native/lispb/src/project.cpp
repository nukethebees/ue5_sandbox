#include <lispb/project.h>

#include <codegen/sexpr/fields.h>

#include <fstream>
#include <set>
#include <span>
#include <stdexcept>
#include <string_view>

namespace lispb {
namespace {

using codegen::sexpr::Fields;
using codegen::sexpr::Form;
using codegen::sexpr::SourceError;
using codegen::sexpr::SourceSpan;

[[noreturn]] void fail(SourceSpan const& span, std::string const& message) {
    throw SourceError{span.path, span, message};
}

auto read_file(std::filesystem::path const& path) -> std::string {
    std::ifstream input{path, std::ios::binary};
    if (!input) {
        throw std::runtime_error{"Cannot read Lispb project: " + path.string()};
    }
    return std::string{std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
}

auto text(Form const& form, std::string_view const purpose) -> std::string {
    return codegen::sexpr::text(form, purpose, codegen::sexpr::throw_source_error);
}

auto paths(Form const& form, std::string_view const purpose) -> std::vector<std::filesystem::path> {
    auto const values{codegen::sexpr::text_list(form, purpose, codegen::sexpr::throw_source_error)};
    std::vector<std::filesystem::path> result;
    result.reserve(values.size());
    for (auto const& value : values) {
        result.emplace_back(value);
    }
    return result;
}

auto names(Form const& form, std::string_view const purpose) -> std::vector<std::string> {
    return codegen::sexpr::text_list(form, purpose, codegen::sexpr::throw_source_error);
}

auto rooted_path(Form const& form, std::string_view const purpose) -> RootedPath {
    if (!form.is_list() || form.children.size() != 2 ||
        (form.head() != "project-path" && form.head() != "build-path")) {
        fail(form.token.span,
             std::string{purpose} + " must be (project-path ...) or (build-path ...)");
    }
    auto const value{std::filesystem::path{text(form.children[1], purpose)}};
    if (value.is_absolute() || value.has_root_path()) {
        fail(form.children[1].token.span, std::string{purpose} + " must be relative");
    }
    return {form.head() == "project-path" ? PathBase::project : PathBase::build, value};
}

auto parse_profile(Form const& form) -> KernelProfile {
    auto const value{text(form, "kernel profile")};
    if (value == "unreal") {
        return KernelProfile::unreal;
    }
    if (value == "standard") {
        return KernelProfile::standard;
    }
    if (value == "unreal-avx2-lab") {
        return KernelProfile::unreal_avx2_lab;
    }
    if (value == "native-x86-simd-lab") {
        return KernelProfile::native_x86_simd_lab;
    }
    fail(form.token.span, "unknown kernel profile '" + value + "'");
}

void insert_target(Project& project,
                   std::string const& name,
                   Target target,
                   SourceSpan const& span) {
    if (project.groups.contains(name) || !project.targets.emplace(name, std::move(target)).second) {
        fail(span, "duplicate Lispb target or group '" + name + "'");
    }
}

void expand(Project const& project,
            std::string const& name,
            std::set<std::string>& active,
            std::set<std::string>& emitted,
            std::vector<std::string>& result) {
    if (project.targets.contains(name)) {
        if (emitted.insert(name).second) {
            result.push_back(name);
        }
        return;
    }
    auto const group{project.groups.find(name)};
    if (group == project.groups.end()) {
        throw std::invalid_argument{"Unknown Lispb target or group: " + name};
    }
    if (!active.insert(name).second) {
        throw std::invalid_argument{"Lispb target group cycle at: " + name};
    }
    for (auto const& member : group->second) {
        expand(project, member, active, emitted, result);
    }
    active.erase(name);
}

} // namespace

auto load_project(std::filesystem::path const& path) -> Project {
    auto const project_path{std::filesystem::absolute(path).lexically_normal()};
    auto const forms{codegen::sexpr::read_forms(project_path.string(), read_file(project_path))};
    if (forms.size() != 1 || forms.front().head() != "lispb-project") {
        auto const span{forms.empty() ? SourceSpan{.path = project_path.string()}
                                      : forms.front().token.span};
        fail(span, "project must contain exactly one 'lispb-project' form");
    }

    Fields const project_fields{forms.front(), "lispb-project", 0};
    project_fields.validate({"language-version", "project-root"},
                            {"cpp-schema", "slate", "kernel", "material", "group"});
    if (text(project_fields.required("language-version"), "language version") != "1") {
        fail(project_fields.required("language-version").token.span,
             "unsupported Lispb language version; expected 1");
    }

    Project project{.root = (project_path.parent_path() /
                             text(project_fields.required("project-root"), "project root"))
                                .lexically_normal()};
    for (auto const* declaration : project_fields.declarations()) {
        auto const head{declaration->head()};
        if (head == "cpp-schema") {
            Fields const fields{*declaration, head, 1};
            fields.validate({"types", "sources", "output-root"});
            auto const name{text(fields.positional(0), "target name")};
            insert_target(project,
                          name,
                          CppSchemaTarget{.name = name,
                                          .types = text(fields.required("types"), "types path"),
                                          .sources = paths(fields.required("sources"), "sources"),
                                          .output_root = rooted_path(fields.required("output-root"),
                                                                     "output root")},
                          declaration->token.span);
        } else if (head == "slate") {
            Fields const fields{*declaration, head, 1};
            fields.validate({"sources", "include-directories", "output-root"});
            auto const name{text(fields.positional(0), "target name")};
            auto const* includes{fields.optional("include-directories")};
            insert_target(
                project,
                name,
                SlateTarget{.name = name,
                            .sources = paths(fields.required("sources"), "sources"),
                            .include_directories = includes == nullptr
                                                     ? std::vector<std::filesystem::path>{}
                                                     : paths(*includes, "include directories"),
                            .output_root =
                                rooted_path(fields.required("output-root"), "output root")},
                declaration->token.span);
        } else if (head == "kernel") {
            Fields const fields{*declaration, head, 1};
            fields.validate({"sources"}, {"emission"});
            auto const sources{paths(fields.required("sources"), "sources")};
            for (auto const* emission : fields.declarations()) {
                Fields const emission_fields{*emission, "emission", 1};
                emission_fields.validate({"profile", "output-root"});
                auto const name{text(emission_fields.positional(0), "target name")};
                insert_target(
                    project,
                    name,
                    KernelTarget{.name = name,
                                 .sources = sources,
                                 .profile = parse_profile(emission_fields.required("profile")),
                                 .output_root = rooted_path(emission_fields.required("output-root"),
                                                            "output root")},
                    emission->token.span);
            }
        } else if (head == "material") {
            Fields const fields{*declaration, head, 1};
            fields.validate({"source", "artifact"});
            auto const name{text(fields.positional(0), "target name")};
            insert_target(project,
                          name,
                          MaterialTarget{.name = name,
                                         .source = text(fields.required("source"), "source path"),
                                         .artifact = rooted_path(fields.required("artifact"),
                                                                 "artifact path")},
                          declaration->token.span);
        } else if (head == "group") {
            Fields const fields{*declaration, head, 1};
            fields.validate({"targets"});
            auto const name{text(fields.positional(0), "group name")};
            if (project.targets.contains(name) ||
                !project.groups.emplace(name, names(fields.required("targets"), "targets"))
                     .second) {
                fail(declaration->token.span, "duplicate Lispb target or group '" + name + "'");
            }
        }
    }

    for (auto const& [name, unused] : project.groups) {
        static_cast<void>(unused);
        static_cast<void>(expand_target(project, name));
    }
    return project;
}

auto resolve(RootedPath const& path,
             std::filesystem::path const& project_root,
             std::filesystem::path const& build_root) -> std::filesystem::path {
    return ((path.base == PathBase::project ? project_root : build_root) / path.path)
        .lexically_normal();
}

auto expand_target(Project const& project, std::string const& name) -> std::vector<std::string> {
    std::set<std::string> active;
    std::set<std::string> emitted;
    std::vector<std::string> result;
    expand(project, name, active, emitted, result);
    return result;
}

} // namespace lispb
