add_executable(native-player-input-tests)
target_sources(native-player-input-tests
  PRIVATE
    sdl_input_tests.cpp
    pause_menu_tests.cpp
    ../input/input_system.cpp
    ../input/physical/physical_input_system.cpp
    ../pause_menu.cpp
)
target_include_directories(native-player-input-tests PRIVATE ../.. ../include ../../../Plugins/SpaceGame/Source/SpaceGame/Private)
target_compile_definitions(native-player-input-tests PRIVATE IOJ_PLAYER_EXPORTS SDL_MAIN_HANDLED)
target_link_libraries(native-player-input-tests PRIVATE native-presentation-interface native-levels SDL3::SDL3-static GTest::gtest_main)
ioj_enable_warnings(native-player-input-tests)
ioj_apply_native_abi(native-player-input-tests)
add_test(NAME native-player-input-tests COMMAND native-player-input-tests)
set_tests_properties(native-player-input-tests PROPERTIES
  LABELS "unit;all;native;native-player-input"
  ENVIRONMENT "SDL_VIDEO_DRIVER=dummy;SDL_GAMECONTROLLER_IGNORE_DEVICES_EXCEPT=0xf00d/0x1234"
)
