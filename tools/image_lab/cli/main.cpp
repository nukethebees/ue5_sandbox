#include <sandbox/image_lab/workflow.hpp>

#include <CLI/CLI.hpp>

#include <filesystem>
#include <iostream>
#include <string>

namespace {

auto print_error(std::string const& error) -> int {
    std::cerr << "Image Lab: " << error << '\n';
    return 1;
}

auto run_list() -> int {
    for (auto const& name : sandbox::image_lab::default_preset_names()) {
        std::cout << name << '\n';
    }
    return 0;
}

auto run_describe(std::string const& preset) -> int {
    auto const description{sandbox::image_lab::describe_default_preset(preset)};
    if (!description) {
        return print_error(description.error());
    }
    std::cout << *description << '\n';
    return 0;
}

auto run_generate(std::string const& preset, std::filesystem::path const& output_directory) -> int {
    auto const request{sandbox::image_lab::find_default_request(preset)};
    if (!request) {
        return print_error("Unknown Image Lab preset: " + preset);
    }

    auto const output_path{sandbox::image_lab::generate_to_png(*request, output_directory)};
    if (!output_path) {
        return print_error(output_path.error());
    }
    std::cout << output_path->string() << '\n';
    return 0;
}

auto run_generate_all(std::filesystem::path const& output_directory) -> int {
    auto const output_paths{sandbox::image_lab::generate_all_defaults_to_png(output_directory)};
    if (!output_paths) {
        return print_error(output_paths.error());
    }
    for (auto const& path : *output_paths) {
        std::cout << path.string() << '\n';
    }
    return 0;
}

} // namespace

auto main(int const argument_count, char** arguments) -> int {
    CLI::App app{"Generate standalone Image Lab PNGs"};
    app.require_subcommand(1);

    auto* const list{app.add_subcommand("list", "List default preset names")};

    std::string describe_preset;
    auto* const describe{app.add_subcommand("describe", "Describe a default preset")};
    describe->add_option("preset", describe_preset, "Preset output name")->required();

    std::string generate_preset;
    std::filesystem::path generate_output_directory;
    auto* const generate{app.add_subcommand("generate", "Generate one default preset")};
    generate->add_option("preset", generate_preset, "Preset output name")->required();
    generate->add_option("--output", generate_output_directory, "PNG output directory")->required();

    std::filesystem::path generate_all_output_directory;
    auto* const generate_all{app.add_subcommand("generate-all", "Generate every default preset")};
    generate_all->add_option("--output", generate_all_output_directory, "PNG output directory")
        ->required();

    CLI11_PARSE(app, argument_count, arguments);
    if (*list) {
        return run_list();
    }
    if (*describe) {
        return run_describe(describe_preset);
    }
    if (*generate) {
        return run_generate(generate_preset, generate_output_directory);
    }
    return run_generate_all(generate_all_output_directory);
}
