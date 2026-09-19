#include "application.hpp"

#include "planner_ui.hpp"
#include "sdl_headers.hpp"

#include <ioj/layout/frame_pacer.hpp>

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlgpu3.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <utility>

namespace ioj::layout_planner {
namespace {

using namespace layout;

class Application {
  public:
    explicit Application(CatalogLoadResult loaded)
        : ui_{std::move(loaded)} {}
    ~Application();

    auto initialize() -> bool;
    auto run() -> int;
  private:
    void process_event(SDL_Event const& event);
    void wait_for_events();
    auto render_frame() -> bool;
    static auto is_interaction_event(Uint32 type) -> bool;

    PlannerUi ui_;
    SDL_Window* window_{};
    SDL_GPUDevice* gpu_device_{};
    bool sdl_initialized_{};
    bool imgui_context_initialized_{};
    bool imgui_platform_initialized_{};
    bool imgui_renderer_initialized_{};
    bool done_{};
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

    auto const scale{SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay())};
    auto const flags{SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY};
    window_ = SDL_CreateWindow("Memory Layout Planner",
                               static_cast<int>(1440.0F * scale),
                               static_cast<int>(900.0F * scale),
                               flags);
    if (window_ == nullptr) {
        std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        return false;
    }
    SDL_SetWindowPosition(window_, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);

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

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    imgui_context_initialized_ = true;
    auto& io{ImGui::GetIO()};
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigDpiScaleFonts = true;
    io.IniFilename = nullptr;
    ImGui::StyleColorsDark();
    auto& style{ImGui::GetStyle()};
    style.ScaleAllSizes(scale);
    style.FontScaleDpi = scale;

    if (!ImGui_ImplSDL3_InitForSDLGPU(window_)) {
        std::fprintf(stderr, "ImGui SDL3 platform initialization failed.\n");
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
        std::fprintf(stderr, "ImGui SDL_GPU renderer initialization failed.\n");
        return false;
    }
    imgui_renderer_initialized_ = true;
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

void Application::process_event(SDL_Event const& event) {
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

void Application::wait_for_events() {
    auto const mode{FramePacer::mode(pacing_state_, std::chrono::steady_clock::now())};
    SDL_Event event{};
    if (mode == FramePacingMode::suspended) {
        if (SDL_WaitEvent(&event)) {
            process_event(event);
        }
    } else {
        auto const timeout{FramePacer::wait_timeout(mode)};
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

    auto const changed{ui_.draw()};
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

} // namespace

auto run_application(CatalogLoadResult loaded) -> int {
    Application application{std::move(loaded)};
    if (!application.initialize()) {
        return 1;
    }
    return application.run();
}

} // namespace ioj::layout_planner
