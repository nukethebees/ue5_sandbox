#pragma once

#include <InputMappingContext.h>
#include <SpaceGame/ships/player/SpaceGamePlayerController.h>
#include <SpaceGame/ui/LevelCompletionWidget.h>

struct FPlayerControllerTestAccess {
    static void prepare_completion(ASpaceGamePlayerController& controller) {
        controller.modal_ui_.completion_menu_ =
            NewObject<ml::ioj::ULevelCompletionWidget>(&controller);
        controller.modal_ui_.begin_suspend(EPlayerControlContext::Player);
    }

    static auto restore_context(ASpaceGamePlayerController const& controller) {
        return controller.modal_ui_.resume_context(EPlayerControlContext::None);
    }

    static void prepare_input(ASpaceGamePlayerController& controller,
                              UEnhancedInputComponent& component,
                              IEnhancedInputSubsystemInterface& subsystem,
                              FSpaceShipControllerInputs const& input) {
        controller.input = input;
        controller.global_input.mapping_context = NewObject<UInputMappingContext>(&controller);
        controller.global_input.toggle_menu = input.move;
        controller.control_contexts_.initialise(controller,
                                                component,
                                                subsystem,
                                                controller.input,
                                                controller.observer_input,
                                                controller.benchmark_input,
                                                controller.global_input);
        controller.begin_play_finished_ = true;
    }

    static void shutdown_input(ASpaceGamePlayerController& controller) {
        controller.control_contexts_.shutdown();
    }

    static auto ship_input(ASpaceGamePlayerController const& controller)
        -> FSpaceShipControllerInputs const& {
        return controller.input;
    }
    static auto global_input(ASpaceGamePlayerController const& controller)
        -> FGlobalControlInputs const& {
        return controller.global_input;
    }

    static void toggle_pause(ASpaceGamePlayerController& controller) {
        controller.toggle_pause_game();
    }
    static auto has_modal(ASpaceGamePlayerController const& controller) -> bool {
        return controller.modal_ui_.has_modal();
    }
    static void complete(ASpaceGamePlayerController& controller,
                         FTestMissionCompletion const& completion) {
        controller.on_mission_completed(completion);
    }
    static void close_completion(ASpaceGamePlayerController& controller) {
        if (auto* const menu{controller.modal_ui_.completion_menu_.Get()}; IsValid(menu)) {
            menu->DeactivateWidget();
        }
    }
    static void end_play(ASpaceGamePlayerController& controller) {
        controller.EndPlay(EEndPlayReason::Destroyed);
    }
    static auto select_context(ASpaceGamePlayerController& controller,
                               EPlayerControlContext context) -> bool {
        return controller.control_contexts_.set_control_context(context);
    }
};
