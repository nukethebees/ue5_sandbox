#include "player/sdl_input.h"

#include <gtest/gtest.h>
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_video.h>

#include <optional>
#include <string>
#include <vector>

namespace ioj::host::tests {
struct Presentation {
    InputRoute route;
    std::vector<std::string> keys;
    std::vector<PointerAction> pointer;
    std::string text;
    std::optional<ViewerMotion> motion;
    int pauses{};
};
Presentation presentation;
}

namespace ioj::host {
auto input_route() -> InputRoute {
    return tests::presentation.route;
}
void read_legacy_key_bindings(void*, LegacyBindingSink) {}
void pause_or_exit_viewer() {
    ++tests::presentation.pauses;
}
void present_steering(std::optional<Vector2d>) {}
void present_viewer_motion(ViewerMotion const& motion, double) {
    tests::presentation.motion = motion;
}
void ui_key(std::string_view name, std::uint32_t, bool down, bool, Modifiers) {
    if (down) {
        tests::presentation.keys.emplace_back(name);
    }
}
void ui_axis(std::string_view, float, Modifiers) {}
void ui_pointer(PointerInput const& event, Modifiers) {
    tests::presentation.pointer.push_back(event.action);
}
void ui_text(std::string_view text, Modifiers) {
    tests::presentation.text.append(text);
}
auto editing_text() -> bool {
    return true;
}
}

namespace ioj::tests {
class SdlPlayerInput : public ::testing::Test {
  protected:
    void SetUp() override {
        SDL_SetMainReady();
        ASSERT_TRUE(SDL_Init(SDL_INIT_VIDEO)) << SDL_GetError();
        host::tests::presentation = {};
        window_ = SDL_CreateWindow("Input test", 100, 100, 0);
        ASSERT_NE(window_, nullptr) << SDL_GetError();
        ASSERT_EQ(SDL_GetKeyboardFocus(), window_);
        input_.emplace(*window_, settings_);
    }
    void TearDown() override {
        input_.reset();
        SDL_DestroyWindow(window_);
        SDL_Quit();
    }
    void frame() {
        input_->begin_frame();
        SDL_Event event{};
        while (SDL_PollEvent(&event)) {
            input_->handle(event);
        }
        input_->finish_frame(0.016);
    }
    void press(SDL_Keycode key, SDL_Scancode scancode) {
        SDL_Event event{};
        event.key.type = SDL_EVENT_KEY_DOWN;
        event.key.windowID = SDL_GetWindowID(window_);
        event.key.scancode = scancode;
        event.key.key = key;
        event.key.down = true;
        ASSERT_TRUE(SDL_PushEvent(&event));
    }
    ControlsSettings settings_{};
    SDL_Window* window_{};
    std::optional<SdlInput> input_;
};

TEST_F(SdlPlayerInput, RoutesQueuedInputAcrossMenuFlightViewerAndBenchmark) {
    auto& view{host::tests::presentation};
    press(SDLK_DOWN, SDL_SCANCODE_DOWN);
    SDL_Event text{};
    text.text.type = SDL_EVENT_TEXT_INPUT;
    text.text.windowID = SDL_GetWindowID(window_);
    text.text.text = "abc";
    ASSERT_TRUE(SDL_PushEvent(&text));
    frame();
    EXPECT_EQ(view.keys, (std::vector<std::string>{"Down"}));
    EXPECT_EQ(view.text, "abc");

    view.route = {input::Context::Gameplay, false};
    press(SDLK_W, SDL_SCANCODE_W);
    press(SDLK_LSHIFT, SDL_SCANCODE_LSHIFT);
    frame();
    EXPECT_EQ(input_->ship_frame().accelerator, 1.f);
    ASSERT_EQ(input_->ship_frame().events.size(), 1u);
    EXPECT_EQ(input_->ship_frame().events.front(), sim::player::PlayerInputKind::BoostPressed);

    view.route = {input::Context::BenchmarkViewer, true};
    press(SDLK_D, SDL_SCANCODE_D);
    frame();
    ASSERT_TRUE(view.motion);
    EXPECT_EQ(view.motion->right, 1.f);
    EXPECT_EQ(input_->ship_frame().accelerator, 0.f);
    ASSERT_EQ(input_->ship_frame().events.size(), 1u);
    EXPECT_EQ(input_->ship_frame().events.front(), sim::player::PlayerInputKind::BoostReleased);

    view.route = {input::Context::BenchmarkViewer, false};
    view.motion.reset();
    press(SDLK_ESCAPE, SDL_SCANCODE_ESCAPE);
    frame();
    EXPECT_EQ(view.pauses, 1);
    EXPECT_FALSE(view.motion);
    EXPECT_EQ(view.keys.size(), 1u);
}

TEST_F(SdlPlayerInput, PointerReleaseFollowsMenuPressAcrossContextChange) {
    auto& view{host::tests::presentation};
    SDL_Event event{};
    event.button.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
    event.button.windowID = SDL_GetWindowID(window_);
    event.button.button = SDL_BUTTON_LEFT;
    event.button.down = true;
    ASSERT_TRUE(SDL_PushEvent(&event));
    frame();
    view.route = {input::Context::Gameplay, false};
    event.button.type = SDL_EVENT_MOUSE_BUTTON_UP;
    event.button.down = false;
    ASSERT_TRUE(SDL_PushEvent(&event));
    frame();
    EXPECT_EQ(
        view.pointer,
        (std::vector<host::PointerAction>{host::PointerAction::Down, host::PointerAction::Up}));

    view.route = {input::Context::BenchmarkViewer, true};
    event.button.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
    event.button.button = SDL_BUTTON_RIGHT;
    event.button.down = true;
    ASSERT_TRUE(SDL_PushEvent(&event));
    frame();
    ASSERT_TRUE(view.motion);
    EXPECT_TRUE(view.motion->looking);
    EXPECT_EQ(view.pointer.size(), 2u);
}

}
