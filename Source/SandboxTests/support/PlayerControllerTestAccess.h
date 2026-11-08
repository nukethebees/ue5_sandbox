#pragma once

#include <SpaceGame/ships/player/SpaceGamePlayerController.h>
#include <SpaceGame/ui/LevelCompletionWidget.h>

#include <InputMappingContext.h>

struct FPlayerControllerTestAccess {
    static void prepare_completion(ioj::ASpaceGamePlayerController& controller) {
        controller.modal_ui_.completion_menu_ = NewObject<ioj::ULevelCompletionWidget>(&controller);
        controller.modal_ui_.begin_suspend(::ioj::levels::ControlContext::Player);
    }

    static auto restore_context(ioj::ASpaceGamePlayerController const& controller) {
        return controller.modal_ui_.resume_context(::ioj::levels::ControlContext::None);
    }

    static void prepare_input(ioj::ASpaceGamePlayerController& controller,
                              UEnhancedInputComponent& component,
                              IEnhancedInputSubsystemInterface& subsystem,
                              ioj::FSpaceShipControllerInputs const& input) {
        controller.input = input;
        controller.global_input.mapping_context = NewObject<UInputMappingContext>(&controller);
        controller.global_input.toggle_menu = input.select_starfox;
        controller.control_contexts_.initialise(controller,
                                                component,
                                                subsystem,
                                                controller.input,
                                                controller.observer_input,
                                                controller.benchmark_input,
                                                controller.global_input);
        controller.begin_play_finished_ = true;
    }

    static void shutdown_input(ioj::ASpaceGamePlayerController& controller) {
        controller.control_contexts_.shutdown();
    }

    static auto ship_input(ioj::ASpaceGamePlayerController const& controller)
        -> ioj::FSpaceShipControllerInputs const& {
        return controller.input;
    }
    static auto global_input(ioj::ASpaceGamePlayerController const& controller)
        -> ioj::FGlobalControlInputs const& {
        return controller.global_input;
    }

    static void toggle_pause(ioj::ASpaceGamePlayerController& controller) {
        controller.toggle_pause_game();
    }
    static auto has_modal(ioj::ASpaceGamePlayerController const& controller) -> bool {
        return controller.modal_ui_.has_modal();
    }
    static void complete(ioj::ASpaceGamePlayerController& controller,
                         FLevelCompletion const& completion) {
        controller.on_level_completed(completion);
    }
    static void close_completion(ioj::ASpaceGamePlayerController& controller) {
        if (auto* const menu{controller.modal_ui_.completion_menu_.Get()}; IsValid(menu)) {
            menu->DeactivateWidget();
        }
    }
    static void end_play(ioj::ASpaceGamePlayerController& controller) {
        controller.EndPlay(EEndPlayReason::Destroyed);
    }
    static auto select_context(ioj::ASpaceGamePlayerController& controller,
                               ::ioj::levels::ControlContext context) -> bool {
        return controller.control_contexts_.set_control_context(context);
    }
};
