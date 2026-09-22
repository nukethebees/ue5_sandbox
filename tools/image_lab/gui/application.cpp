#include "application.hpp"
#include "platform/sdl_headers.hpp"

#include <sandbox/image/image_generation.h>
#include <sandbox/image_lab/workflow.hpp>

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlgpu3.h>
#include <misc/cpp/imgui_stdlib.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace sandbox::image_lab::gui {
namespace detail {

constexpr std::int32_t maximum_preview_dimension{512};

enum class FramePacingMode { interactive, idle, background, suspended };

struct FramePacingState {
    bool focused{true};
    bool minimized{};
    bool explicit_refresh{};
    bool dragging{};
    std::chrono::steady_clock::time_point last_interaction{};
};

[[nodiscard]] auto frame_pacing_mode(FramePacingState const& state,
                                     std::chrono::steady_clock::time_point const now)
    -> FramePacingMode {
    if (state.minimized) {
        return FramePacingMode::suspended;
    }
    if (!state.focused) {
        return FramePacingMode::background;
    }
    if (state.explicit_refresh || state.dragging ||
        now - state.last_interaction <= std::chrono::milliseconds{750}) {
        return FramePacingMode::interactive;
    }
    return FramePacingMode::idle;
}

[[nodiscard]] auto frame_wait_timeout(FramePacingMode const mode) -> std::chrono::milliseconds {
    switch (mode) {
        case FramePacingMode::interactive:
            return std::chrono::milliseconds{0};
        case FramePacingMode::idle:
            return std::chrono::milliseconds{83};
        case FramePacingMode::background:
            return std::chrono::milliseconds{500};
        case FramePacingMode::suspended:
            return std::chrono::milliseconds{-1};
    }
    return std::chrono::milliseconds{0};
}

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
    auto process_event(SDL_Event const& event) -> void;
    auto wait_for_events() -> void;
    auto render_frame() -> bool;
    [[nodiscard]] static auto is_interaction_event(Uint32 type) -> bool;
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
    SDL_GPUDevice* gpu_device_{};
    SDL_GPUTexture* preview_texture_{};
    SDL_GPUTransferBuffer* preview_transfer_buffer_{};
    std::size_t preview_transfer_buffer_size_{};
    image::GeneratedImage preview_{};
    std::string status_;
    FramePacingState pacing_state_{
        .focused = true,
        .minimized = false,
        .explicit_refresh = true,
        .dragging = false,
        .last_interaction = std::chrono::steady_clock::now(),
    };
};

Application::~Application() {
    if (gpu_device_ != nullptr) {
        SDL_WaitForGPUIdle(gpu_device_);
    }
    if (preview_texture_ != nullptr) {
        SDL_ReleaseGPUTexture(gpu_device_, preview_texture_);
    }
    if (preview_transfer_buffer_ != nullptr) {
        SDL_ReleaseGPUTransferBuffer(gpu_device_, preview_transfer_buffer_);
    }
    if (imgui_renderer_initialized_) {
        ImGui_ImplSDLGPU3_Shutdown();
    }
    if (imgui_platform_initialized_) {
        ImGui_ImplSDL3_Shutdown();
    }
    if (imgui_context_initialized_) {
        ImGui::DestroyContext();
    }
    if (gpu_device_ != nullptr && window_ != nullptr) {
        SDL_ReleaseWindowFromGPUDevice(gpu_device_, window_);
    }
    if (gpu_device_ != nullptr) {
        SDL_DestroyGPUDevice(gpu_device_);
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

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    imgui_context_initialized_ = true;
    auto& io{ImGui::GetIO()};
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigDpiScaleFonts = true;
    ImGui::StyleColorsDark();
    auto const primary_display{SDL_GetPrimaryDisplay()};
    auto const scale{SDL_GetDisplayContentScale(primary_display)};
    auto& style{ImGui::GetStyle()};
    style.ScaleAllSizes(scale);
    style.FontScaleDpi = scale;
    io.Fonts->AddFontDefaultVector();

    auto const flags{SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY};
    window_ = SDL_CreateWindow(
        "Image Lab", static_cast<int>(1440.0F * scale), static_cast<int>(900.0F * scale), flags);
    if (window_ == nullptr) {
        std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        return false;
    }

    constexpr auto shader_formats{SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL |
                                  SDL_GPU_SHADERFORMAT_MSL | SDL_GPU_SHADERFORMAT_METALLIB};
    gpu_device_ = SDL_CreateGPUDevice(shader_formats, true, nullptr);
    if (gpu_device_ == nullptr) {
        std::fprintf(stderr, "SDL_CreateGPUDevice failed: %s\n", SDL_GetError());
        return false;
    }
    if (!SDL_ClaimWindowForGPUDevice(gpu_device_, window_)) {
        std::fprintf(stderr, "SDL_ClaimWindowForGPUDevice failed: %s\n", SDL_GetError());
        return false;
    }
    if (!SDL_SetGPUSwapchainParameters(
            gpu_device_, window_, SDL_GPU_SWAPCHAINCOMPOSITION_SDR, SDL_GPU_PRESENTMODE_VSYNC)) {
        std::fprintf(stderr, "SDL_SetGPUSwapchainParameters failed: %s\n", SDL_GetError());
        return false;
    }

    if (!ImGui_ImplSDL3_InitForSDLGPU(window_)) {
        std::fprintf(stderr, "ImGui SDL3 initialization failed.\n");
        return false;
    }
    imgui_platform_initialized_ = true;
    ImGui_ImplSDLGPU3_InitInfo renderer_info{};
    renderer_info.Device = gpu_device_;
    renderer_info.ColorTargetFormat = SDL_GetGPUSwapchainTextureFormat(gpu_device_, window_);
    renderer_info.MSAASamples = SDL_GPU_SAMPLECOUNT_1;
    renderer_info.SwapchainComposition = SDL_GPU_SWAPCHAINCOMPOSITION_SDR;
    renderer_info.PresentMode = SDL_GPU_PRESENTMODE_VSYNC;
    if (!ImGui_ImplSDLGPU3_Init(&renderer_info)) {
        std::fprintf(stderr, "ImGui SDL_GPU initialization failed.\n");
        return false;
    }
    imgui_renderer_initialized_ = true;

    refresh_preview();
    SDL_ShowWindow(window_);
    return true;
}

auto Application::run() -> int {
    while (!done_) {
        wait_for_events();
        if (done_) {
            break;
        }
        auto const flags{SDL_GetWindowFlags(window_)};
        pacing_state_.focused = (flags & SDL_WINDOW_INPUT_FOCUS) != 0;
        pacing_state_.minimized = (flags & SDL_WINDOW_MINIMIZED) != 0;
        if (pacing_state_.minimized) {
            continue;
        }
        if (!render_frame()) {
            return 1;
        }
    }
    return 0;
}

auto Application::process_event(SDL_Event const& event) -> void {
    ImGui_ImplSDL3_ProcessEvent(&event);
    if (event.type == SDL_EVENT_QUIT || (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
                                         event.window.windowID == SDL_GetWindowID(window_))) {
        done_ = true;
    }
    if (is_interaction_event(event.type)) {
        pacing_state_.last_interaction = std::chrono::steady_clock::now();
        pacing_state_.explicit_refresh = true;
    }
}

auto Application::wait_for_events() -> void {
    SDL_Event event{};
    auto const mode{frame_pacing_mode(pacing_state_, std::chrono::steady_clock::now())};
    if (mode == FramePacingMode::suspended) {
        if (SDL_WaitEvent(&event)) {
            process_event(event);
        }
    } else {
        auto const timeout{frame_wait_timeout(mode)};
        if (timeout.count() > 0 &&
            SDL_WaitEventTimeout(&event, static_cast<Sint32>(timeout.count()))) {
            process_event(event);
        }
    }
    while (SDL_PollEvent(&event)) {
        process_event(event);
    }
}

auto Application::render_frame() -> bool {
    ImGui_ImplSDLGPU3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
    auto const changed{draw_interface()};
    pacing_state_.dragging = ImGui::IsMouseDragging(ImGuiMouseButton_Left) ||
                             ImGui::IsMouseDragging(ImGuiMouseButton_Right) ||
                             ImGui::IsMouseDragging(ImGuiMouseButton_Middle);
    pacing_state_.explicit_refresh = changed;
    ImGui::Render();
    auto* draw_data{ImGui::GetDrawData()};
    auto* command_buffer{SDL_AcquireGPUCommandBuffer(gpu_device_)};
    if (command_buffer == nullptr) {
        std::fprintf(stderr, "SDL_AcquireGPUCommandBuffer failed: %s\n", SDL_GetError());
        return false;
    }
    SDL_GPUTexture* swapchain_texture{};
    if (!SDL_WaitAndAcquireGPUSwapchainTexture(
            command_buffer, window_, &swapchain_texture, nullptr, nullptr)) {
        std::fprintf(stderr, "SDL_WaitAndAcquireGPUSwapchainTexture failed: %s\n", SDL_GetError());
        SDL_CancelGPUCommandBuffer(command_buffer);
        return false;
    }
    if (swapchain_texture != nullptr && draw_data->DisplaySize.x > 0.0F &&
        draw_data->DisplaySize.y > 0.0F) {
        ImGui_ImplSDLGPU3_PrepareDrawData(draw_data, command_buffer);
        SDL_GPUColorTargetInfo target_info{};
        target_info.texture = swapchain_texture;
        target_info.clear_color = {0.055F, 0.065F, 0.08F, 1.0F};
        target_info.load_op = SDL_GPU_LOADOP_CLEAR;
        target_info.store_op = SDL_GPU_STOREOP_STORE;
        auto* render_pass{SDL_BeginGPURenderPass(command_buffer, &target_info, 1, nullptr)};
        if (render_pass == nullptr) {
            std::fprintf(stderr, "SDL_BeginGPURenderPass failed: %s\n", SDL_GetError());
            SDL_CancelGPUCommandBuffer(command_buffer);
            return false;
        }
        ImGui_ImplSDLGPU3_RenderDrawData(draw_data, command_buffer, render_pass);
        SDL_EndGPURenderPass(render_pass);
    }
    if (!SDL_SubmitGPUCommandBuffer(command_buffer)) {
        std::fprintf(stderr, "SDL_SubmitGPUCommandBuffer failed: %s\n", SDL_GetError());
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
            SDL_ReleaseGPUTexture(gpu_device_, preview_texture_);
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
        SDL_ReleaseGPUTexture(gpu_device_, preview_texture_);
        preview_texture_ = nullptr;
    }
    SDL_GPUTextureCreateInfo texture_info{};
    texture_info.type = SDL_GPU_TEXTURETYPE_2D;
    texture_info.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    texture_info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
    texture_info.width = static_cast<Uint32>(preview.width);
    texture_info.height = static_cast<Uint32>(preview.height);
    texture_info.layer_count_or_depth = 1u;
    texture_info.num_levels = 1u;
    texture_info.sample_count = SDL_GPU_SAMPLECOUNT_1;
    preview_texture_ = SDL_CreateGPUTexture(gpu_device_, &texture_info);
    if (preview_texture_ == nullptr) {
        return false;
    }

    auto const byte_count{preview.pixels.size() * sizeof(image::Pixel)};
    if (preview_transfer_buffer_size_ < byte_count) {
        if (preview_transfer_buffer_ != nullptr) {
            SDL_ReleaseGPUTransferBuffer(gpu_device_, preview_transfer_buffer_);
        }
        SDL_GPUTransferBufferCreateInfo transfer_info{};
        transfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        transfer_info.size = static_cast<Uint32>(byte_count);
        preview_transfer_buffer_ = SDL_CreateGPUTransferBuffer(gpu_device_, &transfer_info);
        if (preview_transfer_buffer_ == nullptr) {
            SDL_ReleaseGPUTexture(gpu_device_, preview_texture_);
            preview_texture_ = nullptr;
            return false;
        }
        preview_transfer_buffer_size_ = byte_count;
    }

    auto* const mapped{SDL_MapGPUTransferBuffer(gpu_device_, preview_transfer_buffer_, true)};
    if (mapped == nullptr) {
        SDL_ReleaseGPUTexture(gpu_device_, preview_texture_);
        preview_texture_ = nullptr;
        return false;
    }
    std::memcpy(mapped, preview.pixels.data(), byte_count);
    SDL_UnmapGPUTransferBuffer(gpu_device_, preview_transfer_buffer_);

    auto* const command_buffer{SDL_AcquireGPUCommandBuffer(gpu_device_)};
    if (command_buffer == nullptr) {
        SDL_ReleaseGPUTexture(gpu_device_, preview_texture_);
        preview_texture_ = nullptr;
        return false;
    }
    auto* const copy_pass{SDL_BeginGPUCopyPass(command_buffer)};
    SDL_GPUTextureTransferInfo transfer_info{};
    transfer_info.transfer_buffer = preview_transfer_buffer_;
    transfer_info.pixels_per_row = static_cast<Uint32>(preview.width);
    transfer_info.rows_per_layer = static_cast<Uint32>(preview.height);
    SDL_GPUTextureRegion destination{};
    destination.texture = preview_texture_;
    destination.w = static_cast<Uint32>(preview.width);
    destination.h = static_cast<Uint32>(preview.height);
    destination.d = 1u;
    SDL_UploadToGPUTexture(copy_pass, &transfer_info, &destination, false);
    SDL_EndGPUCopyPass(copy_pass);
    if (!SDL_SubmitGPUCommandBuffer(command_buffer)) {
        SDL_ReleaseGPUTexture(gpu_device_, preview_texture_);
        preview_texture_ = nullptr;
        return false;
    }
    return true;
}

auto Application::is_interaction_event(Uint32 const type) -> bool {
    switch (type) {
        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP:
        case SDL_EVENT_TEXT_EDITING:
        case SDL_EVENT_TEXT_INPUT:
        case SDL_EVENT_MOUSE_MOTION:
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP:
        case SDL_EVENT_MOUSE_WHEEL:
        case SDL_EVENT_FINGER_DOWN:
        case SDL_EVENT_FINGER_UP:
        case SDL_EVENT_FINGER_MOTION:
        case SDL_EVENT_WINDOW_RESIZED:
        case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
        case SDL_EVENT_WINDOW_MINIMIZED:
        case SDL_EVENT_WINDOW_RESTORED:
        case SDL_EVENT_WINDOW_FOCUS_GAINED:
        case SDL_EVENT_WINDOW_FOCUS_LOST:
            return true;
        default:
            return false;
    }
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
