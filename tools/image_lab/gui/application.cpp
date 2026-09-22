#include "application.hpp"
#include "platform/sdl_headers.hpp"

#include <sandbox/image/image_generation.h>
#include <sandbox/image_lab/workflow.hpp>

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>
#include <misc/cpp/imgui_stdlib.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace sandbox::image_lab::gui {
namespace detail {

constexpr std::int32_t maximum_preview_dimension{512};

[[nodiscard]] auto generator_name(image::GeneratorType const generator) -> char const* {
    switch (generator) {
        case image::GeneratorType::RadialGradient:
            return "Soft Radial Gradient";
        case image::GeneratorType::RingMask:
            return "Ring Mask";
        case image::GeneratorType::ShockwaveFlipbook:
            return "Shockwave Flipbook";
        case image::GeneratorType::Starfield:
            return "Starfield";
        case image::GeneratorType::Noise:
            return "Coherent Noise";
        case image::GeneratorType::DomainWarpedNoise:
            return "Domain-Warped Noise";
        case image::GeneratorType::CurlNoiseFlow:
            return "Curl-Noise Flow";
        case image::GeneratorType::CellularNoise:
            return "Cellular Noise";
        case image::GeneratorType::HexGrid:
            return "Hex Grid Mask";
    }
    return "Unknown";
}

[[nodiscard]] auto preview_channel_name(image::PreviewChannel const channel) -> char const* {
    switch (channel) {
        case image::PreviewChannel::Color:
            return "Color";
        case image::PreviewChannel::RGB:
            return "RGB";
        case image::PreviewChannel::Red:
            return "Red";
        case image::PreviewChannel::Green:
            return "Green";
        case image::PreviewChannel::Blue:
            return "Blue";
        case image::PreviewChannel::Alpha:
            return "Alpha";
    }
    return "Color";
}

[[nodiscard]] auto output_name(image::ImagePostProcessParameters::Output const output)
    -> char const* {
    switch (output) {
        case image::ImagePostProcessParameters::Output::Scalar:
            return "Scalar / Color";
        case image::ImagePostProcessParameters::Output::NormalMap:
            return "Tangent-Space Normal Map";
        case image::ImagePostProcessParameters::Output::SignedDistance:
            return "Signed Distance Field";
    }
    return "Scalar / Color";
}

auto draw_dimensions(std::int32_t& width, std::int32_t& height) -> bool {
    auto changed{ImGui::InputInt("Width", &width)};
    changed |= ImGui::InputInt("Height", &height);
    return changed;
}

auto draw_seed(char const* const label, std::uint32_t& seed) -> bool {
    return ImGui::InputScalar(label, ImGuiDataType_U32, &seed);
}

class Application {
  public:
    explicit Application(std::filesystem::path output_directory)
        : output_directory_{std::move(output_directory)} {}
    ~Application();

    auto initialize() -> bool;
    auto run() -> int;
  private:
    auto process_events() -> void;
    auto render_frame() -> bool;
    auto draw_interface() -> bool;
    auto draw_preset_selector() -> bool;
    auto draw_request_editor() -> bool;
    auto draw_generator_editor() -> bool;
    auto draw_post_process_editor() -> bool;
    auto draw_preview_controls() -> bool;
    auto draw_preview() const -> void;
    auto refresh_preview() -> void;
    auto update_preview_texture(image::GeneratedImage const& preview) -> bool;
    auto reset_generator(image::GeneratorType generator) -> void;
    auto open_output_directory() -> void;

    image::GenerationRequest request_{
        image::make_default_request(image::GeneratorType::RadialGradient)};
    std::vector<image::GenerationRequest> presets_{image::default_generation_requests()};
    std::filesystem::path output_directory_;
    image::PreviewChannel preview_channel_{image::PreviewChannel::Color};
    std::int32_t selected_preset_index_{};
    bool tiled_preview_{};
    bool done_{};
    bool sdl_initialized_{};
    bool imgui_context_initialized_{};
    bool imgui_platform_initialized_{};
    bool imgui_renderer_initialized_{};
    SDL_Window* window_{};
    SDL_Renderer* renderer_{};
    SDL_Texture* preview_texture_{};
    image::GeneratedImage preview_{};
    std::string status_;
};

Application::~Application() {
    if (preview_texture_ != nullptr) {
        SDL_DestroyTexture(preview_texture_);
    }
    if (imgui_renderer_initialized_) {
        ImGui_ImplSDLRenderer3_Shutdown();
    }
    if (imgui_platform_initialized_) {
        ImGui_ImplSDL3_Shutdown();
    }
    if (imgui_context_initialized_) {
        ImGui::DestroyContext();
    }
    if (renderer_ != nullptr) {
        SDL_DestroyRenderer(renderer_);
    }
    if (window_ != nullptr) {
        SDL_DestroyWindow(window_);
    }
    if (sdl_initialized_) {
        SDL_Quit();
    }
}

auto Application::initialize() -> bool {
    SDL_SetMainReady();
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return false;
    }
    sdl_initialized_ = true;

    window_ = SDL_CreateWindow(
        "Image Lab", 1440, 900, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (window_ == nullptr) {
        std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        return false;
    }
    renderer_ = SDL_CreateRenderer(window_, nullptr);
    if (renderer_ == nullptr) {
        std::fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        return false;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    imgui_context_initialized_ = true;
    auto& io{ImGui::GetIO()};
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();
    if (!ImGui_ImplSDL3_InitForSDLRenderer(window_, renderer_)) {
        std::fprintf(stderr, "ImGui SDL3 initialization failed.\n");
        return false;
    }
    imgui_platform_initialized_ = true;
    if (!ImGui_ImplSDLRenderer3_Init(renderer_)) {
        std::fprintf(stderr, "ImGui SDL renderer initialization failed.\n");
        return false;
    }
    imgui_renderer_initialized_ = true;

    refresh_preview();
    return true;
}

auto Application::run() -> int {
    while (!done_) {
        process_events();
        if (!render_frame()) {
            return 1;
        }
    }
    return 0;
}

auto Application::process_events() -> void {
    SDL_Event event{};
    while (SDL_PollEvent(&event)) {
        ImGui_ImplSDL3_ProcessEvent(&event);
        if (event.type == SDL_EVENT_QUIT || (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
                                             event.window.windowID == SDL_GetWindowID(window_))) {
            done_ = true;
        }
    }
}

auto Application::render_frame() -> bool {
    ImGui_ImplSDLRenderer3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
    draw_interface();
    ImGui::Render();

    if (!SDL_SetRenderDrawColor(renderer_, 14u, 17u, 22u, 255u) || !SDL_RenderClear(renderer_)) {
        std::fprintf(stderr, "SDL render clear failed: %s\n", SDL_GetError());
        return false;
    }
    ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer_);
    if (!SDL_RenderPresent(renderer_)) {
        std::fprintf(stderr, "SDL_RenderPresent failed: %s\n", SDL_GetError());
        return false;
    }
    return true;
}

auto Application::draw_interface() -> bool {
    auto changed{false};
    ImGui::SetNextWindowPos({0.0F, 0.0F});
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
    constexpr auto window_flags{ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
                                ImGuiWindowFlags_NoMove};
    ImGui::Begin("Image Lab", nullptr, window_flags);
    if (ImGui::BeginTable(
            "ImageLabLayout", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV)) {
        ImGui::TableSetupColumn("Controls", ImGuiTableColumnFlags_WidthStretch, 0.45F);
        ImGui::TableSetupColumn("Preview", ImGuiTableColumnFlags_WidthStretch, 0.55F);
        ImGui::TableNextColumn();
        changed |= draw_preset_selector();
        auto const request_changed{draw_request_editor()};
        if (request_changed) {
            selected_preset_index_ = -1;
            changed = true;
        }
        if (ImGui::Button("Generate Selected")) {
            auto const result{generate_to_png(request_, output_directory_)};
            status_ = result ? "Wrote " + result->string() : result.error();
        }
        ImGui::SameLine();
        if (ImGui::Button("Generate All Defaults")) {
            auto const result{generate_all_defaults_to_png(output_directory_)};
            status_ = result ? "Wrote " + std::to_string(result->size()) + " PNGs to " +
                                   output_directory_.string()
                             : result.error();
        }
        ImGui::SameLine();
        if (ImGui::Button("Open Output Folder")) {
            open_output_directory();
        }
        ImGui::TextWrapped("Output: %s", output_directory_.string().c_str());
        if (!status_.empty()) {
            ImGui::Separator();
            ImGui::TextWrapped("%s", status_.c_str());
        }

        ImGui::TableNextColumn();
        auto const preview_changed{draw_preview_controls()};
        changed |= preview_changed;
        draw_preview();
        ImGui::EndTable();
    }
    ImGui::End();

    if (changed) {
        refresh_preview();
    }
    return changed;
}

auto Application::draw_preset_selector() -> bool {
    auto changed{false};
    auto const label{selected_preset_index_ >= 0 ? presets_[selected_preset_index_].output_name
                                                 : "Custom"};
    if (ImGui::BeginCombo("Preset", label.c_str())) {
        auto const preset_count{static_cast<std::int32_t>(presets_.size())};
        for (std::int32_t index{}; index < preset_count; ++index) {
            auto const selected{index == selected_preset_index_};
            if (ImGui::Selectable(presets_[index].output_name.c_str(), selected)) {
                request_ = presets_[index];
                selected_preset_index_ = index;
                changed = true;
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
    return changed;
}

auto Application::draw_request_editor() -> bool {
    auto changed{ImGui::InputText("Output Name", &request_.output_name)};
    changed |= draw_generator_editor();
    changed |= draw_post_process_editor();
    return changed;
}

auto Application::draw_generator_editor() -> bool {
    auto changed{false};
    if (ImGui::BeginCombo("Generator", generator_name(request_.generator))) {
        constexpr std::array generators{image::GeneratorType::RadialGradient,
                                        image::GeneratorType::RingMask,
                                        image::GeneratorType::ShockwaveFlipbook,
                                        image::GeneratorType::Starfield,
                                        image::GeneratorType::Noise,
                                        image::GeneratorType::DomainWarpedNoise,
                                        image::GeneratorType::CurlNoiseFlow,
                                        image::GeneratorType::CellularNoise,
                                        image::GeneratorType::HexGrid};
        for (auto const generator : generators) {
            auto const selected{generator == request_.generator};
            if (ImGui::Selectable(generator_name(generator), selected)) {
                reset_generator(generator);
                changed = true;
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    ImGui::SeparatorText("Generator Parameters");
    switch (request_.generator) {
        case image::GeneratorType::RadialGradient:
            changed |=
                draw_dimensions(request_.radial_gradient.width, request_.radial_gradient.height);
            changed |=
                ImGui::DragFloat("Inner Radius", &request_.radial_gradient.inner_radius, 0.005F);
            changed |=
                ImGui::DragFloat("Outer Radius", &request_.radial_gradient.outer_radius, 0.005F);
            break;
        case image::GeneratorType::RingMask:
            changed |= draw_dimensions(request_.ring_mask.width, request_.ring_mask.height);
            changed |= ImGui::DragFloat("Radius", &request_.ring_mask.radius, 0.005F);
            changed |= ImGui::DragFloat("Thickness", &request_.ring_mask.thickness, 0.005F);
            changed |= ImGui::DragFloat("Falloff", &request_.ring_mask.falloff, 0.005F);
            break;
        case image::GeneratorType::ShockwaveFlipbook:
            changed |= draw_dimensions(request_.shockwave_flipbook.width,
                                       request_.shockwave_flipbook.height);
            changed |= ImGui::InputInt("Columns", &request_.shockwave_flipbook.columns);
            changed |= ImGui::InputInt("Rows", &request_.shockwave_flipbook.rows);
            changed |= ImGui::InputInt("Frame Count", &request_.shockwave_flipbook.frame_count);
            changed |=
                ImGui::DragFloat("Start Radius", &request_.shockwave_flipbook.start_radius, 0.005F);
            changed |=
                ImGui::DragFloat("End Radius", &request_.shockwave_flipbook.end_radius, 0.005F);
            changed |=
                ImGui::DragFloat("Thickness", &request_.shockwave_flipbook.thickness, 0.005F);
            changed |= ImGui::DragFloat("Falloff", &request_.shockwave_flipbook.falloff, 0.005F);
            changed |= ImGui::DragFloat(
                "Start Intensity", &request_.shockwave_flipbook.start_intensity, 0.01F);
            changed |= ImGui::DragFloat(
                "End Intensity", &request_.shockwave_flipbook.end_intensity, 0.01F);
            break;
        case image::GeneratorType::Starfield:
            changed |= draw_dimensions(request_.starfield.width, request_.starfield.height);
            changed |= draw_seed("Seed", request_.starfield.seed);
            changed |= ImGui::InputInt("Star Count", &request_.starfield.star_count);
            changed |= ImGui::DragFloat(
                "Minimum Brightness", &request_.starfield.minimum_brightness, 0.01F);
            changed |=
                ImGui::DragFloat("Minimum Radius", &request_.starfield.minimum_radius, 0.01F);
            changed |=
                ImGui::DragFloat("Maximum Radius", &request_.starfield.maximum_radius, 0.01F);
            changed |= ImGui::Checkbox("Transparent Background",
                                       &request_.starfield.transparent_background);
            break;
        case image::GeneratorType::Noise:
            changed |= draw_dimensions(request_.noise.width, request_.noise.height);
            changed |= draw_seed("Seed", request_.noise.seed);
            changed |= ImGui::DragFloat("Base Scale", &request_.noise.base_scale, 0.1F);
            changed |= ImGui::InputInt("Octave Count", &request_.noise.octave_count);
            changed |= ImGui::DragFloat("Persistence", &request_.noise.persistence, 0.01F);
            changed |= ImGui::Checkbox("Tileable", &request_.noise.tileable);
            break;
        case image::GeneratorType::DomainWarpedNoise:
            changed |= draw_dimensions(request_.domain_warped_noise.width,
                                       request_.domain_warped_noise.height);
            changed |= draw_seed("Base Seed", request_.domain_warped_noise.base_seed);
            changed |= draw_seed("Warp Seed", request_.domain_warped_noise.warp_seed);
            changed |=
                ImGui::DragFloat("Base Scale", &request_.domain_warped_noise.base_scale, 0.1F);
            changed |=
                ImGui::DragFloat("Warp Scale", &request_.domain_warped_noise.warp_scale, 0.1F);
            changed |= ImGui::DragFloat(
                "Warp Strength", &request_.domain_warped_noise.warp_strength, 0.1F);
            changed |= ImGui::InputInt("Base Octave Count",
                                       &request_.domain_warped_noise.base_octave_count);
            changed |= ImGui::InputInt("Warp Octave Count",
                                       &request_.domain_warped_noise.warp_octave_count);
            changed |=
                ImGui::DragFloat("Persistence", &request_.domain_warped_noise.persistence, 0.01F);
            changed |= ImGui::Checkbox("Tileable", &request_.domain_warped_noise.tileable);
            break;
        case image::GeneratorType::CurlNoiseFlow:
            changed |=
                draw_dimensions(request_.curl_noise_flow.width, request_.curl_noise_flow.height);
            changed |= draw_seed("Seed", request_.curl_noise_flow.seed);
            changed |= ImGui::DragFloat("Base Scale", &request_.curl_noise_flow.base_scale, 0.1F);
            changed |= ImGui::InputInt("Octave Count", &request_.curl_noise_flow.octave_count);
            changed |=
                ImGui::DragFloat("Persistence", &request_.curl_noise_flow.persistence, 0.01F);
            changed |= ImGui::DragFloat(
                "Derivative Step", &request_.curl_noise_flow.derivative_step, 0.01F);
            changed |= ImGui::DragFloat("Strength", &request_.curl_noise_flow.strength, 0.01F);
            changed |= ImGui::Checkbox("Tileable", &request_.curl_noise_flow.tileable);
            break;
        case image::GeneratorType::CellularNoise:
            changed |=
                draw_dimensions(request_.cellular_noise.width, request_.cellular_noise.height);
            changed |= draw_seed("Seed", request_.cellular_noise.seed);
            changed |= ImGui::DragFloat("Cell Size", &request_.cellular_noise.cell_size, 0.1F);
            changed |= ImGui::DragFloat("Jitter", &request_.cellular_noise.jitter, 0.01F);
            if (ImGui::BeginCombo("Mode",
                                  request_.cellular_noise.mode == image::CellularMode::Distance
                                      ? "Distance"
                                      : "Borders")) {
                for (auto const mode :
                     {image::CellularMode::Distance, image::CellularMode::Borders}) {
                    auto const selected{mode == request_.cellular_noise.mode};
                    auto const* const label{mode == image::CellularMode::Distance ? "Distance"
                                                                                  : "Borders"};
                    if (ImGui::Selectable(label, selected)) {
                        request_.cellular_noise.mode = mode;
                        changed = true;
                    }
                    if (selected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }
            changed |= ImGui::DragFloat("Edge Width", &request_.cellular_noise.edge_width, 0.01F);
            changed |= ImGui::DragFloat("Falloff", &request_.cellular_noise.falloff, 0.01F);
            changed |= ImGui::Checkbox("Tileable", &request_.cellular_noise.tileable);
            break;
        case image::GeneratorType::HexGrid:
            changed |= draw_dimensions(request_.hex_grid.width, request_.hex_grid.height);
            changed |= ImGui::DragFloat("Cell Radius", &request_.hex_grid.cell_radius, 0.1F);
            changed |= ImGui::DragFloat("Line Thickness", &request_.hex_grid.line_thickness, 0.01F);
            changed |= ImGui::DragFloat("Falloff", &request_.hex_grid.falloff, 0.01F);
            break;
    }
    return changed;
}

auto Application::draw_post_process_editor() -> bool {
    if (request_.generator == image::GeneratorType::CurlNoiseFlow) {
        return false;
    }

    auto& parameters{request_.post_process};
    auto changed{false};
    ImGui::SeparatorText("Output Shaping");
    changed |= ImGui::Checkbox("Invert", &parameters.invert);
    changed |= ImGui::DragFloat("Contrast", &parameters.contrast, 0.01F, 0.0F);
    changed |= ImGui::Checkbox("Threshold Enabled", &parameters.threshold_enabled);
    if (parameters.threshold_enabled) {
        changed |= ImGui::DragFloat("Threshold", &parameters.threshold, 0.01F, 0.0F, 1.0F);
        changed |= ImGui::DragFloat(
            "Threshold Softness", &parameters.threshold_softness, 0.01F, 0.0F, 1.0F);
    }
    if (ImGui::BeginCombo("Output", output_name(parameters.output))) {
        constexpr std::array outputs{image::ImagePostProcessParameters::Output::Scalar,
                                     image::ImagePostProcessParameters::Output::NormalMap,
                                     image::ImagePostProcessParameters::Output::SignedDistance};
        for (auto const output : outputs) {
            auto const selected{output == parameters.output};
            if (ImGui::Selectable(output_name(output), selected)) {
                parameters.output = output;
                changed = true;
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
    if (parameters.output == image::ImagePostProcessParameters::Output::NormalMap) {
        changed |= ImGui::DragFloat("Normal Strength", &parameters.normal_strength, 0.1F, 0.0F);
        changed |= ImGui::Checkbox("Normal Wrap", &parameters.normal_wrap);
    }
    if (parameters.output == image::ImagePostProcessParameters::Output::SignedDistance) {
        changed |= ImGui::DragFloat(
            "Distance Threshold", &parameters.distance_threshold, 0.01F, 0.0F, 1.0F);
        changed |=
            ImGui::DragFloat("Distance Range", &parameters.distance_range, 0.1F, 1.0F, 4096.0F);
        changed |= ImGui::Checkbox("Distance Wrap", &parameters.distance_wrap);
    }
    return changed;
}

auto Application::draw_preview_controls() -> bool {
    auto changed{false};
    if (ImGui::BeginCombo("Preview Channel", preview_channel_name(preview_channel_))) {
        constexpr std::array channels{image::PreviewChannel::Color,
                                      image::PreviewChannel::RGB,
                                      image::PreviewChannel::Red,
                                      image::PreviewChannel::Green,
                                      image::PreviewChannel::Blue,
                                      image::PreviewChannel::Alpha};
        for (auto const channel : channels) {
            auto const selected{channel == preview_channel_};
            if (ImGui::Selectable(preview_channel_name(channel), selected)) {
                preview_channel_ = channel;
                changed = true;
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
    changed |= ImGui::Checkbox("Tiled Preview", &tiled_preview_);
    return changed;
}

auto Application::draw_preview() const -> void {
    if (preview_texture_ == nullptr) {
        ImGui::TextWrapped("No preview: %s", status_.c_str());
        return;
    }

    auto const available{ImGui::GetContentRegionAvail()};
    auto const scale{std::min(available.x / static_cast<float>(preview_.width),
                              available.y / static_cast<float>(preview_.height))};
    auto const width{std::max(1.0F, static_cast<float>(preview_.width) * scale)};
    auto const height{std::max(1.0F, static_cast<float>(preview_.height) * scale)};
    ImGui::Image(
        ImTextureRef{static_cast<ImTextureID>(reinterpret_cast<std::uintptr_t>(preview_texture_))},
        {width, height});
}

auto Application::refresh_preview() -> void {
    auto const preview_request{
        image::scale_request_for_preview(request_, maximum_preview_dimension, tiled_preview_)};
    auto const generated{image::generate_image(preview_request)};
    if (!generated.is_valid()) {
        status_ = generated.error;
        if (preview_texture_ != nullptr) {
            SDL_DestroyTexture(preview_texture_);
            preview_texture_ = nullptr;
        }
        return;
    }
    auto const display{image::make_preview_image(generated, preview_channel_, tiled_preview_)};
    if (!update_preview_texture(display)) {
        status_ = std::string{"Could not upload preview texture: "} + SDL_GetError();
        return;
    }
    preview_ = display;
    status_ = "Output: " + std::to_string(generated.width) + " x " +
              std::to_string(generated.height) + " | Preview: " + std::to_string(display.width) +
              " x " + std::to_string(display.height);
}

auto Application::update_preview_texture(image::GeneratedImage const& preview) -> bool {
    if (preview_texture_ != nullptr) {
        SDL_DestroyTexture(preview_texture_);
        preview_texture_ = nullptr;
    }
    preview_texture_ = SDL_CreateTexture(
        renderer_, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, preview.width, preview.height);
    if (preview_texture_ == nullptr) {
        return false;
    }
    if (!SDL_SetTextureBlendMode(preview_texture_, SDL_BLENDMODE_BLEND) ||
        !SDL_SetTextureScaleMode(preview_texture_, SDL_SCALEMODE_NEAREST) ||
        !SDL_UpdateTexture(preview_texture_,
                           nullptr,
                           preview.pixels.data(),
                           preview.width * static_cast<std::int32_t>(sizeof(image::Pixel)))) {
        SDL_DestroyTexture(preview_texture_);
        preview_texture_ = nullptr;
        return false;
    }
    return true;
}

auto Application::reset_generator(image::GeneratorType const generator) -> void {
    auto const post_process{request_.post_process};
    request_ = image::make_default_request(generator);
    request_.post_process = post_process;
    if (generator == image::GeneratorType::CurlNoiseFlow) {
        request_.post_process.output = image::ImagePostProcessParameters::Output::Scalar;
    }
}

auto Application::open_output_directory() -> void {
    std::error_code error;
    std::filesystem::create_directories(output_directory_, error);
    if (error) {
        status_ = "Could not create output directory: " + output_directory_.string();
        return;
    }
    auto const url{"file:///" + std::filesystem::absolute(output_directory_).generic_string()};
    if (!SDL_OpenURL(url.c_str())) {
        status_ = std::string{"Could not open output directory: "} + SDL_GetError();
    }
}

} // namespace detail

auto run_application(std::filesystem::path output_directory) -> int {
    detail::Application application{std::move(output_directory)};
    if (!application.initialize()) {
        return 1;
    }
    return application.run();
}

} // namespace sandbox::image_lab::gui
