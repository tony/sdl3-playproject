cmake_minimum_required(VERSION 3.25)
project(sdl3_sandbox LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(CTest)
include(FetchContent)

option(SANDBOX_SDL_SHARED "Build SDL3 shared library (otherwise static)" OFF)
if(SANDBOX_SDL_SHARED)
  set(SDL_SHARED ON CACHE BOOL "" FORCE)
  set(SDL_STATIC OFF CACHE BOOL "" FORCE)
else()
  set(SDL_SHARED OFF CACHE BOOL "" FORCE)
  set(SDL_STATIC ON CACHE BOOL "" FORCE)
endif()

set(SANDBOX_SDL3_GIT_TAG "release-3.2.28" CACHE STRING "SDL3 Git tag/commit to fetch")
FetchContent_Declare(
  SDL3
  GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
  GIT_TAG        ${SANDBOX_SDL3_GIT_TAG}
  GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(SDL3)

# stb_image for PNG loading (header-only)
FetchContent_Declare(
  stb
  GIT_REPOSITORY https://github.com/nothings/stb.git
  GIT_TAG        master
  GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(stb)

include(cmake/CPM.cmake)
include(cmake/ThirdParty.cmake)

set(SHMUP_SOURCES
  src/shmup/ShmupGame.cpp
  src/shmup/config/ShipConfig.cpp
  src/shmup/config/WeaponConfig.cpp
  src/shmup/config/SatelliteConfig.cpp
  src/shmup/controller/ShmupController.cpp
  src/shmup/controller/FirePatterns.cpp
  src/shmup/systems/ShmupSystems.cpp
)

add_executable(sandbox
  src/main.cpp
  src/core/App.cpp
  src/core/DebugUI.cpp
  src/core/Input.cpp
  src/core/InputScript.cpp
  src/core/Prefs.cpp
  src/core/SpriteCache.cpp
  src/ecs/Systems.cpp
  src/ecs/World.cpp
  src/stage/Stage.cpp
  src/enemy/EnemyConfig.cpp
  src/collectible/CollectibleConfig.cpp
  src/character/CharacterConfig.cpp
  src/character/CharacterController.cpp
  src/character/Actions.cpp
  src/visual/Shapes.cpp
  src/visual/Form.cpp
  src/visual/FormToml.cpp
  src/visual/CharacterForms.cpp
  ${SHMUP_SOURCES}
)

target_include_directories(sandbox PRIVATE src ${stb_SOURCE_DIR})
target_link_libraries(sandbox PRIVATE SDL3::SDL3)

if(SANDBOX_WITH_FMT)
  target_link_libraries(sandbox PRIVATE fmt::fmt)
endif()
if(SANDBOX_WITH_SPDLOG)
  target_link_libraries(sandbox PRIVATE spdlog::spdlog)
endif()
if(SANDBOX_WITH_IMGUI)
  target_link_libraries(sandbox PRIVATE imgui::imgui)
  target_compile_definitions(sandbox PRIVATE SANDBOX_WITH_IMGUI=1)
endif()
if(SANDBOX_WITH_TRACY)
  target_link_libraries(sandbox PRIVATE tracy::tracy_client)
  target_compile_definitions(sandbox PRIVATE TRACY_ENABLE)
endif()
if(SANDBOX_WITH_GLM)
  target_link_libraries(sandbox PRIVATE glm::glm)
endif()
if(SANDBOX_WITH_ENTT)
  target_link_libraries(sandbox PRIVATE EnTT::EnTT)
  target_compile_definitions(sandbox PRIVATE SANDBOX_HAS_ENTT=1)
endif()
if(SANDBOX_WITH_TOMLPP)
  target_link_libraries(sandbox PRIVATE tomlplusplus::tomlplusplus)
  target_compile_definitions(sandbox PRIVATE SANDBOX_HAS_TOMLPP=1)
endif()

target_compile_options(sandbox PRIVATE
  $<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang,AppleClang>:-Wall;-Wextra;-Wpedantic>
)

include(cmake/DevTools.cmake)

if(BUILD_TESTING)
  function(sandbox_add_test)
    add_test(${ARGV})
    list(FIND ARGV "NAME" _name_index)
    if(_name_index EQUAL -1)
      message(FATAL_ERROR "sandbox_add_test requires NAME")
    endif()
    math(EXPR _name_index "${_name_index}+1")
    list(GET ARGV ${_name_index} _test_name)
    set_tests_properties("${_test_name}" PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
  endfunction()

  sandbox_add_test(NAME sandbox_validate_assets COMMAND sandbox --validate)
  sandbox_add_test(NAME sandbox_validate_assets_strict COMMAND sandbox --validate --strict)

  sandbox_add_test(NAME sandbox_smoke_offscreen COMMAND sandbox --no-prefs --frames 120 --video-driver offscreen --expect-grounded --expect-no-respawn)
  sandbox_add_test(NAME sandbox_smoke_headless COMMAND sandbox --no-prefs --frames 120 --no-render --expect-grounded --expect-no-respawn)
  sandbox_add_test(NAME sandbox_smoke_offscreen_no_ui COMMAND sandbox --no-prefs --frames 120 --video-driver offscreen --no-ui --expect-grounded --expect-no-respawn)

  file(GLOB SANDBOX_CHARACTER_TOMLS CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/assets/characters/*.toml")
  list(SORT SANDBOX_CHARACTER_TOMLS)
  foreach(_toml IN LISTS SANDBOX_CHARACTER_TOMLS)
    get_filename_component(_stem "${_toml}" NAME_WE)
    string(REGEX REPLACE "[^A-Za-z0-9_]" "_" _stem "${_stem}")
    set(_smoke_frames 120)
    if(_stem STREQUAL "megamanx")
      set(_smoke_frames 180)
    endif()
    sandbox_add_test(NAME "sandbox_smoke_character_${_stem}" COMMAND sandbox --no-prefs --frames ${_smoke_frames} --video-driver offscreen --character "${_toml}" --expect-grounded --expect-no-respawn)
  endforeach()

  file(GLOB SANDBOX_STAGE_TOMLS CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/assets/stages/*.toml")
  list(SORT SANDBOX_STAGE_TOMLS)
  foreach(_toml IN LISTS SANDBOX_STAGE_TOMLS)
    get_filename_component(_stem "${_toml}" NAME_WE)
    string(REGEX REPLACE "[^A-Za-z0-9_]" "_" _stem "${_stem}")
    sandbox_add_test(NAME "sandbox_smoke_stage_${_stem}" COMMAND sandbox --no-prefs --frames 120 --video-driver offscreen --stage "${_toml}" --expect-grounded --expect-no-respawn)
  endforeach()

  # Extra regression spawns for slope_torture (seams/one-way/sub-pixel edge cases).
  set(_slope_torture_stage "${CMAKE_SOURCE_DIR}/assets/dev/stages/slope_torture.toml")
  foreach(_spawn IN ITEMS seam_tie one_way_seam one_way_seam_right subpixel_seam subpixel_seam_right peak valley steep_slope_top right_edge)
    sandbox_add_test(
      NAME "sandbox_smoke_stage_slope_torture_spawn_${_spawn}"
      COMMAND sandbox --no-prefs --frames 120 --video-driver offscreen --stage "${_slope_torture_stage}" --spawn-point "${_spawn}" --expect-grounded --expect-no-respawn
    )
  endforeach()

	  set(_slope_torture_subpixel_script
	      "${CMAKE_SOURCE_DIR}/assets/input_scripts/slope_torture_subpixel_traverse.toml"
	  )
	  set(_slope_torture_subpixel_left_script
	      "${CMAKE_SOURCE_DIR}/assets/input_scripts/slope_torture_subpixel_traverse_left.toml"
	  )
	  set(_slope_torture_seam_tie_script
	      "${CMAKE_SOURCE_DIR}/assets/input_scripts/slope_torture_seam_tie_traverse.toml"
	  )
	  set(_slope_torture_seam_tie_left_script
	      "${CMAKE_SOURCE_DIR}/assets/input_scripts/slope_torture_seam_tie_traverse_left.toml"
	  )
	  set(_slope_torture_one_way_seam_script
	      "${CMAKE_SOURCE_DIR}/assets/input_scripts/slope_torture_one_way_seam_traverse.toml"
	  )
	  set(_slope_torture_one_way_seam_left_script
	      "${CMAKE_SOURCE_DIR}/assets/input_scripts/slope_torture_one_way_seam_traverse_left.toml"
	  )
	  set(_slope_torture_peak_script
	      "${CMAKE_SOURCE_DIR}/assets/input_scripts/slope_torture_peak_traverse.toml"
	  )
	  set(_slope_torture_valley_script
	      "${CMAKE_SOURCE_DIR}/assets/input_scripts/slope_torture_valley_traverse.toml"
	  )
	  set(_slope_torture_step_up_slope_script
	      "${CMAKE_SOURCE_DIR}/assets/input_scripts/slope_torture_step_up_slope_traverse.toml"
	  )
	  set(_slope_torture_steep_slope_script
	      "${CMAKE_SOURCE_DIR}/assets/input_scripts/slope_torture_steep_slope_traverse.toml"
	  )
	  sandbox_add_test(
	    NAME sandbox_smoke_stage_slope_torture_traverse_subpixel_seam
	    COMMAND sandbox --no-prefs --frames 120 --video-driver offscreen --stage "${_slope_torture_stage}" --spawn-point subpixel_seam --input-script "${_slope_torture_subpixel_script}" --expect-grounded --expect-no-respawn --expect-min-x 2800 --expect-max-y 640
	  )

	  sandbox_add_test(
	    NAME sandbox_smoke_stage_slope_torture_traverse_subpixel_seam_quantized
	    COMMAND sandbox --no-prefs --frames 120 --video-driver offscreen --stage "${_slope_torture_stage}" --character "${CMAKE_SOURCE_DIR}/assets/dev/characters/test_subpixel256.toml" --spawn-point subpixel_seam --input-script "${_slope_torture_subpixel_script}" --expect-grounded --expect-no-respawn --expect-min-x 2800 --expect-max-y 640
	  )

	  sandbox_add_test(
	    NAME sandbox_smoke_stage_slope_torture_traverse_subpixel_seam_quantized_pixel_collision
	    COMMAND sandbox --no-prefs --frames 120 --video-driver offscreen --stage "${_slope_torture_stage}" --character "${CMAKE_SOURCE_DIR}/assets/dev/characters/sonic.toml" --spawn-point subpixel_seam --input-script "${_slope_torture_subpixel_script}" --expect-grounded --expect-no-respawn --expect-min-x 2800 --expect-max-y 640
	  )

	  sandbox_add_test(
	    NAME sandbox_smoke_stage_slope_torture_traverse_subpixel_seam_left
	    COMMAND sandbox --no-prefs --frames 120 --video-driver offscreen --stage "${_slope_torture_stage}" --spawn-point subpixel_seam_right --input-script "${_slope_torture_subpixel_left_script}" --expect-grounded --expect-no-respawn --expect-min-x 2550 --expect-max-x 2760 --expect-max-y 640
	  )

	  sandbox_add_test(
	    NAME sandbox_smoke_stage_slope_torture_traverse_seam_tie
	    COMMAND sandbox --no-prefs --frames 120 --video-driver offscreen --stage "${_slope_torture_stage}" --spawn-point seam_tie --input-script "${_slope_torture_seam_tie_script}" --expect-grounded --expect-no-respawn --expect-min-x 520 --expect-max-y 640
	  )

	  sandbox_add_test(
	    NAME sandbox_smoke_stage_slope_torture_traverse_seam_tie_left
	    COMMAND sandbox --no-prefs --frames 120 --video-driver offscreen --stage "${_slope_torture_stage}" --spawn-point seam_tie --input-script "${_slope_torture_seam_tie_left_script}" --expect-grounded --expect-no-respawn --expect-min-x 320 --expect-max-x 370 --expect-max-y 640
	  )

	  sandbox_add_test(
	    NAME sandbox_smoke_stage_slope_torture_traverse_one_way_seam
	    COMMAND sandbox --no-prefs --frames 120 --video-driver offscreen --stage "${_slope_torture_stage}" --spawn-point one_way_seam --input-script "${_slope_torture_one_way_seam_script}" --expect-grounded --expect-no-respawn --expect-min-x 1100 --expect-max-y 640
	  )

	  sandbox_add_test(
	    NAME sandbox_smoke_stage_slope_torture_traverse_one_way_seam_left
	    COMMAND sandbox --no-prefs --frames 120 --video-driver offscreen --stage "${_slope_torture_stage}" --spawn-point one_way_seam_right --input-script "${_slope_torture_one_way_seam_left_script}" --expect-grounded --expect-no-respawn --expect-min-x 820 --expect-max-x 1150 --expect-max-y 450
	  )

	  sandbox_add_test(
	    NAME sandbox_smoke_stage_slope_torture_traverse_peak
	    COMMAND sandbox --no-prefs --frames 120 --video-driver offscreen --stage "${_slope_torture_stage}" --spawn-point peak --input-script "${_slope_torture_peak_script}" --expect-grounded --expect-no-respawn --expect-min-x 1240 --expect-max-y 640
	  )

	  sandbox_add_test(
	    NAME sandbox_smoke_stage_slope_torture_traverse_valley
	    COMMAND sandbox --no-prefs --frames 120 --video-driver offscreen --stage "${_slope_torture_stage}" --spawn-point valley --input-script "${_slope_torture_valley_script}" --expect-grounded --expect-no-respawn --expect-min-x 1870 --expect-max-y 650
	  )

	  sandbox_add_test(
	    NAME sandbox_smoke_stage_slope_torture_traverse_step_up_slope
	    COMMAND sandbox --no-prefs --frames 120 --video-driver offscreen --stage "${_slope_torture_stage}" --spawn-point step_up_slope --input-script "${_slope_torture_step_up_slope_script}" --expect-grounded --expect-no-respawn --expect-max-y 650
	  )

	  sandbox_add_test(
	    NAME sandbox_smoke_stage_slope_torture_traverse_steep_slope_left
	    COMMAND sandbox --no-prefs --frames 120 --video-driver offscreen --stage "${_slope_torture_stage}" --spawn-point steep_slope_top --input-script "${_slope_torture_steep_slope_script}" --expect-grounded --expect-no-respawn --expect-max-x 2230 --expect-max-y 640
	  )

	  set(_mechanics_torture_stage "${CMAKE_SOURCE_DIR}/assets/dev/stages/mechanics_torture.toml")
	  set(_mechanics_torture_drop_through_script
	      "${CMAKE_SOURCE_DIR}/assets/input_scripts/mechanics_torture_drop_through.toml"
	  )
	  set(_mechanics_torture_fly_rise_script
	      "${CMAKE_SOURCE_DIR}/assets/input_scripts/mechanics_torture_fly_rise.toml"
	  )
	  set(_mechanics_torture_fly_jump_again_script
	      "${CMAKE_SOURCE_DIR}/assets/input_scripts/mechanics_torture_fly_jump_again.toml"
	  )
	  sandbox_add_test(
	    NAME sandbox_smoke_stage_mechanics_torture_drop_through
	    COMMAND sandbox --no-prefs --frames 120 --no-render --stage "${_mechanics_torture_stage}" --spawn-point one_way_drop --input-script "${_mechanics_torture_drop_through_script}" --expect-grounded --expect-no-respawn --expect-min-y 440
	  )

	  sandbox_add_test(
	    NAME sandbox_smoke_stage_mechanics_torture_drop_through_hold_zero
	    COMMAND sandbox --no-prefs --frames 60 --no-render --stage "${_mechanics_torture_stage}" --character "${CMAKE_SOURCE_DIR}/assets/dev/characters/test_drop_through_hold.toml" --spawn-point one_way_drop --input-script "${_mechanics_torture_drop_through_script}" --expect-grounded --expect-no-respawn --expect-max-y 330
	  )

	  sandbox_add_test(
	    NAME sandbox_smoke_stage_mechanics_torture_fly_rise
	    COMMAND sandbox --no-prefs --frames 60 --no-render --stage "${_mechanics_torture_stage}" --character "${CMAKE_SOURCE_DIR}/assets/dev/characters/kirby.toml" --spawn-point fly_rise --input-script "${_mechanics_torture_fly_rise_script}" --expect-no-respawn --expect-max-y 260
	  )
	  sandbox_add_test(
	    NAME sandbox_smoke_stage_mechanics_torture_fly_jump_again
	    COMMAND sandbox --no-prefs --frames 60 --no-render --stage "${_mechanics_torture_stage}" --character "${CMAKE_SOURCE_DIR}/assets/dev/characters/kirby.toml" --spawn-point fly_rise --input-script "${_mechanics_torture_fly_jump_again_script}" --expect-no-respawn --expect-max-y 340
	  )

	  set(_mechanics_torture_coyote_script
	      "${CMAKE_SOURCE_DIR}/assets/input_scripts/mechanics_torture_coyote_jump.toml"
	  )
	  sandbox_add_test(
	    NAME sandbox_smoke_stage_mechanics_torture_coyote_jump
	    COMMAND sandbox --no-prefs --frames 180 --no-render --stage "${_mechanics_torture_stage}" --spawn-point coyote --input-script "${_mechanics_torture_coyote_script}" --expect-grounded --expect-no-respawn --expect-min-x 880
	  )

	  set(_mechanics_torture_jump_buffer_script
	      "${CMAKE_SOURCE_DIR}/assets/input_scripts/mechanics_torture_jump_buffer.toml"
	  )
	  set(_mechanics_torture_jump_buffer_early_script
	      "${CMAKE_SOURCE_DIR}/assets/input_scripts/mechanics_torture_jump_buffer_early.toml"
	  )
	  sandbox_add_test(
	    NAME sandbox_smoke_stage_mechanics_torture_jump_buffer
	    COMMAND sandbox --no-prefs --frames 60 --no-render --stage "${_mechanics_torture_stage}" --spawn-point jump_buffer --input-script "${_mechanics_torture_jump_buffer_script}" --expect-no-respawn --expect-max-y 465
	  )

	  sandbox_add_test(
	    NAME sandbox_smoke_stage_mechanics_torture_simon_no_jump_buffer_no_buffered_jump
	    COMMAND sandbox --no-prefs --frames 50 --no-render --stage "${_mechanics_torture_stage}" --character "${CMAKE_SOURCE_DIR}/assets/dev/characters/simon.toml" --spawn-point jump_buffer --input-script "${_mechanics_torture_jump_buffer_early_script}" --expect-grounded --expect-no-respawn
	  )

	  set(_mechanics_torture_slide_window_script
	      "${CMAKE_SOURCE_DIR}/assets/input_scripts/mechanics_torture_slide_window.toml"
	  )
	  sandbox_add_test(
	    NAME sandbox_smoke_stage_mechanics_torture_slide_window
	    COMMAND sandbox --no-prefs --frames 8 --no-render --stage "${_mechanics_torture_stage}" --character "${CMAKE_SOURCE_DIR}/assets/dev/characters/test_slide_window.toml" --spawn-point default --input-script "${_mechanics_torture_slide_window_script}" --expect-grounded --expect-no-respawn --expect-min-vx 1.2 --expect-max-vx 1.3
	  )

	  set(_mechanics_torture_spin_stop_script
	      "${CMAKE_SOURCE_DIR}/assets/input_scripts/mechanics_torture_spin_stop.toml"
	  )
	  sandbox_add_test(
	    NAME sandbox_smoke_stage_mechanics_torture_spin_stop
	    COMMAND sandbox --no-prefs --frames 60 --no-render --stage "${_mechanics_torture_stage}" --character "${CMAKE_SOURCE_DIR}/assets/dev/characters/test_spin.toml" --spawn-point default --input-script "${_mechanics_torture_spin_stop_script}" --expect-grounded --expect-no-respawn --expect-max-vx 0.2
	  )

	  set(_mechanics_torture_dash_script
	      "${CMAKE_SOURCE_DIR}/assets/input_scripts/mechanics_torture_dash_right.toml"
	  )
	  set(_mechanics_torture_dash_hold_short_script
	      "${CMAKE_SOURCE_DIR}/assets/input_scripts/mechanics_torture_dash_hold_short.toml"
	  )
	  set(_mechanics_torture_dash_hold_long_script
	      "${CMAKE_SOURCE_DIR}/assets/input_scripts/mechanics_torture_dash_hold_long.toml"
	  )
	  set(_mechanics_torture_air_dash_jump_again_script
	      "${CMAKE_SOURCE_DIR}/assets/input_scripts/mechanics_torture_air_dash_jump_again.toml"
	  )
	  set(_move_instant_run_right_script
	      "${CMAKE_SOURCE_DIR}/assets/input_scripts/move_instant_run_right.toml"
	  )
	  sandbox_add_test(
	    NAME sandbox_smoke_stage_mechanics_torture_instant_move_reaches_speed
	    COMMAND sandbox --no-prefs --frames 2 --no-render --stage "${_mechanics_torture_stage}" --character "${CMAKE_SOURCE_DIR}/assets/dev/characters/test_instant.toml" --spawn-point default --input-script "${_move_instant_run_right_script}" --expect-grounded --expect-no-respawn --expect-min-vx 0.7 --expect-max-vx 0.8
	  )

	  set(_mechanics_torture_no_air_control_reverse_script
	      "${CMAKE_SOURCE_DIR}/assets/input_scripts/mechanics_torture_no_air_control_reverse.toml"
	  )
	  set(_mechanics_torture_air_turn_resistance_script
	      "${CMAKE_SOURCE_DIR}/assets/input_scripts/mechanics_torture_air_turn_resistance.toml"
	  )
	  sandbox_add_test(
	    NAME sandbox_smoke_stage_mechanics_torture_no_air_control_reverse
	    COMMAND sandbox --no-prefs --frames 20 --no-render --stage "${_mechanics_torture_stage}" --character "${CMAKE_SOURCE_DIR}/assets/dev/characters/test_instant_no_air_control.toml" --spawn-point default --input-script "${_mechanics_torture_no_air_control_reverse_script}" --expect-no-respawn --expect-min-vx 0.7 --expect-max-vx 0.8 --expect-max-y 470
	  )
	  sandbox_add_test(
	    NAME sandbox_smoke_stage_mechanics_torture_air_turn_resistance
	    COMMAND sandbox --no-prefs --frames 20 --no-render --stage "${_mechanics_torture_stage}" --character "${CMAKE_SOURCE_DIR}/assets/dev/characters/test_air_turn_resistance.toml" --spawn-point default --input-script "${_mechanics_torture_air_turn_resistance_script}" --expect-no-respawn --expect-min-vx 0.6 --expect-max-y 470
	  )

	  sandbox_add_test(
	    NAME sandbox_smoke_stage_mechanics_torture_dash_right
	    COMMAND sandbox --no-prefs --frames 18 --no-render --stage "${_mechanics_torture_stage}" --character "${CMAKE_SOURCE_DIR}/assets/dev/characters/megamanx.toml" --spawn-point default --input-script "${_mechanics_torture_dash_script}" --expect-grounded --expect-no-respawn --expect-min-vx 1.7 --expect-max-vx 1.8
	  )
	  sandbox_add_test(
	    NAME sandbox_smoke_stage_mechanics_torture_dash_hold_short
	    COMMAND sandbox --no-prefs --frames 8 --no-render --stage "${_mechanics_torture_stage}" --character "${CMAKE_SOURCE_DIR}/assets/dev/characters/test_dash_hold.toml" --spawn-point default --input-script "${_mechanics_torture_dash_hold_short_script}" --expect-grounded --expect-no-respawn --expect-max-vx 1.0
	  )
	  sandbox_add_test(
	    NAME sandbox_smoke_stage_mechanics_torture_dash_hold_long
	    COMMAND sandbox --no-prefs --frames 8 --no-render --stage "${_mechanics_torture_stage}" --character "${CMAKE_SOURCE_DIR}/assets/dev/characters/test_dash_hold.toml" --spawn-point default --input-script "${_mechanics_torture_dash_hold_long_script}" --expect-grounded --expect-no-respawn --expect-min-vx 2.5
	  )
  sandbox_add_test(
    NAME sandbox_smoke_stage_mechanics_torture_air_dash_jump_again
    COMMAND sandbox --no-prefs --frames 20 --no-render --stage "${_mechanics_torture_stage}" --character "${CMAKE_SOURCE_DIR}/assets/dev/characters/zero_airdash.toml" --spawn-point default --input-script "${_mechanics_torture_air_dash_jump_again_script}" --expect-no-respawn --expect-min-vx 1.9 --expect-max-vx 2.1
  )

	  set(_mechanics_torture_spindash_script
	      "${CMAKE_SOURCE_DIR}/assets/input_scripts/mechanics_torture_spindash_charge_release.toml"
	  )
	  set(_mechanics_torture_spindash_window_script
	      "${CMAKE_SOURCE_DIR}/assets/input_scripts/mechanics_torture_spindash_window.toml"
	  )
	  set(_mechanics_torture_spindash_window_down_early_script
	      "${CMAKE_SOURCE_DIR}/assets/input_scripts/mechanics_torture_spindash_window_down_early.toml"
	  )
	  set(_mechanics_torture_spindash_window_same_frame_script
	      "${CMAKE_SOURCE_DIR}/assets/input_scripts/mechanics_torture_spindash_window_same_frame.toml"
	  )
	  set(_mechanics_torture_spindash_window_late_script
	      "${CMAKE_SOURCE_DIR}/assets/input_scripts/mechanics_torture_spindash_window_late.toml"
	  )
	  sandbox_add_test(
	    NAME sandbox_smoke_stage_mechanics_torture_spindash_charge_holds
	    COMMAND sandbox --no-prefs --frames 30 --no-render --stage "${_mechanics_torture_stage}" --character "${CMAKE_SOURCE_DIR}/assets/dev/characters/sonic.toml" --spawn-point default --input-script "${_mechanics_torture_spindash_script}" --expect-grounded --expect-no-respawn --expect-max-vx 1
	  )
	  sandbox_add_test(
	    NAME sandbox_smoke_stage_mechanics_torture_spindash_release_launch
	    COMMAND sandbox --no-prefs --frames 62 --no-render --stage "${_mechanics_torture_stage}" --character "${CMAKE_SOURCE_DIR}/assets/dev/characters/sonic.toml" --spawn-point default --input-script "${_mechanics_torture_spindash_script}" --expect-grounded --expect-no-respawn --expect-min-vx 5.4
	  )
	  sandbox_add_test(
	    NAME sandbox_smoke_stage_mechanics_torture_spindash_window
	    COMMAND sandbox --no-prefs --frames 11 --no-render --stage "${_mechanics_torture_stage}" --character "${CMAKE_SOURCE_DIR}/assets/dev/characters/test_spindash_window.toml" --spawn-point default --input-script "${_mechanics_torture_spindash_window_script}" --expect-grounded --expect-no-respawn --expect-min-vx 3.9 --expect-max-vx 4.1
	  )
	  sandbox_add_test(
	    NAME sandbox_smoke_stage_mechanics_torture_spindash_window_down_early
	    COMMAND sandbox --no-prefs --frames 12 --no-render --stage "${_mechanics_torture_stage}" --character "${CMAKE_SOURCE_DIR}/assets/dev/characters/test_spindash_window.toml" --spawn-point default --input-script "${_mechanics_torture_spindash_window_down_early_script}" --expect-no-respawn --expect-max-vx 1.1
	  )
	  sandbox_add_test(
	    NAME sandbox_smoke_stage_mechanics_torture_spindash_window_same_frame
	    COMMAND sandbox --no-prefs --frames 12 --no-render --stage "${_mechanics_torture_stage}" --character "${CMAKE_SOURCE_DIR}/assets/dev/characters/test_spindash_window.toml" --spawn-point default --input-script "${_mechanics_torture_spindash_window_same_frame_script}" --expect-grounded --expect-no-respawn --expect-min-vx 3.9 --expect-max-vx 4.1
	  )
	  sandbox_add_test(
	    NAME sandbox_smoke_stage_mechanics_torture_spindash_window_late
	    COMMAND sandbox --no-prefs --frames 21 --no-render --stage "${_mechanics_torture_stage}" --character "${CMAKE_SOURCE_DIR}/assets/dev/characters/test_spindash_window.toml" --spawn-point default --input-script "${_mechanics_torture_spindash_window_late_script}" --expect-grounded --expect-no-respawn --expect-max-vx 1.1
	  )

	  set(_mechanics_torture_glide_script
	      "${CMAKE_SOURCE_DIR}/assets/input_scripts/mechanics_torture_glide_fall.toml"
	  )
	  set(_mechanics_torture_glide_jump_again_script
	      "${CMAKE_SOURCE_DIR}/assets/input_scripts/mechanics_torture_glide_jump_again.toml"
	  )
	  sandbox_add_test(
	    NAME sandbox_smoke_stage_mechanics_torture_glide_fall
	    COMMAND sandbox --no-prefs --frames 120 --no-render --stage "${_mechanics_torture_stage}" --character "${CMAKE_SOURCE_DIR}/assets/dev/characters/knuckles.toml" --spawn-point glide_fall --input-script "${_mechanics_torture_glide_script}" --expect-no-respawn --expect-max-y 450
	  )
	  sandbox_add_test(
	    NAME sandbox_smoke_stage_mechanics_torture_glide_jump_again
	    COMMAND sandbox --no-prefs --frames 120 --no-render --stage "${_mechanics_torture_stage}" --character "${CMAKE_SOURCE_DIR}/assets/dev/characters/knuckles.toml" --spawn-point glide_fall --input-script "${_mechanics_torture_glide_jump_again_script}" --expect-no-respawn --expect-max-y 440
	  )

	  set(_mechanics_torture_full_hop_script
	      "${CMAKE_SOURCE_DIR}/assets/input_scripts/mechanics_torture_full_hop.toml"
	  )
	  sandbox_add_test(
	    NAME sandbox_smoke_stage_mechanics_torture_full_hop_height
	    COMMAND sandbox --no-prefs --frames 35 --no-render --stage "${_mechanics_torture_stage}" --character "${CMAKE_SOURCE_DIR}/assets/dev/characters/sonic.toml" --spawn-point default --input-script "${_mechanics_torture_full_hop_script}" --expect-no-respawn --expect-max-y 438
	  )

	  set(_mechanics_torture_short_hop_script
	      "${CMAKE_SOURCE_DIR}/assets/input_scripts/mechanics_torture_short_hop.toml"
	  )
	  sandbox_add_test(
	    NAME sandbox_smoke_stage_mechanics_torture_short_hop_height
	    COMMAND sandbox --no-prefs --frames 30 --no-render --stage "${_mechanics_torture_stage}" --character "${CMAKE_SOURCE_DIR}/assets/dev/characters/sonic.toml" --spawn-point default --input-script "${_mechanics_torture_short_hop_script}" --expect-no-respawn --expect-min-y 445 --expect-max-y 462
	  )

	  sandbox_add_test(
	    NAME sandbox_smoke_stage_mechanics_torture_fixed_jump_ignores_release
	    COMMAND sandbox --no-prefs --frames 35 --no-render --stage "${_mechanics_torture_stage}" --character "${CMAKE_SOURCE_DIR}/assets/dev/characters/test_fixed_jump.toml" --spawn-point default --input-script "${_mechanics_torture_short_hop_script}" --expect-no-respawn --expect-max-y 438
	  )

	  sandbox_add_test(
	    NAME sandbox_smoke_stage_mechanics_torture_simon_no_jump_buffer_still_jumps
	    COMMAND sandbox --no-prefs --frames 20 --no-render --stage "${_mechanics_torture_stage}" --character "${CMAKE_SOURCE_DIR}/assets/dev/characters/simon.toml" --spawn-point default --input-script "${_mechanics_torture_full_hop_script}" --expect-no-respawn --expect-max-y 460
	  )

	  sandbox_add_test(
	    NAME sandbox_smoke_stage_mechanics_torture_mario_full_hop_height
	    COMMAND sandbox --no-prefs --frames 35 --no-render --stage "${_mechanics_torture_stage}" --character "${CMAKE_SOURCE_DIR}/assets/dev/characters/mario.toml" --spawn-point default --input-script "${_mechanics_torture_full_hop_script}" --expect-no-respawn --expect-max-y 455
	  )

	  sandbox_add_test(
	    NAME sandbox_smoke_stage_mechanics_torture_mario_short_hop_height
	    COMMAND sandbox --no-prefs --frames 15 --no-render --stage "${_mechanics_torture_stage}" --character "${CMAKE_SOURCE_DIR}/assets/dev/characters/mario.toml" --spawn-point default --input-script "${_mechanics_torture_short_hop_script}" --expect-no-respawn --expect-min-y 435 --expect-max-y 445
	  )

	  set(_jump_table_run_jump_script
	      "${CMAKE_SOURCE_DIR}/assets/input_scripts/jump_table_run_jump.toml"
	  )
	  sandbox_add_test(
	    NAME sandbox_smoke_stage_mechanics_torture_jump_table_stand_jump
	    COMMAND sandbox --no-prefs --frames 35 --no-render --stage "${_mechanics_torture_stage}" --character "${CMAKE_SOURCE_DIR}/assets/dev/characters/test_jump_table.toml" --spawn-point default --input-script "${_mechanics_torture_full_hop_script}" --expect-no-respawn --expect-max-y 455
	  )
	  sandbox_add_test(
	    NAME sandbox_smoke_stage_mechanics_torture_jump_table_run_jump
	    COMMAND sandbox --no-prefs --frames 35 --no-render --stage "${_mechanics_torture_stage}" --character "${CMAKE_SOURCE_DIR}/assets/dev/characters/test_jump_table.toml" --spawn-point default --input-script "${_jump_table_run_jump_script}" --expect-no-respawn --expect-max-y 445
	  )

	  sandbox_add_test(
	    NAME sandbox_smoke_stage_mechanics_torture_fall_gravity
	    COMMAND sandbox --no-prefs --frames 45 --no-render --stage "${_mechanics_torture_stage}" --character "${CMAKE_SOURCE_DIR}/assets/dev/characters/sonic.toml" --spawn-point glide_fall --expect-no-respawn --expect-min-y 300 --expect-max-y 340
	  )

	  set(_wall_torture_stage "${CMAKE_SOURCE_DIR}/assets/dev/stages/wall_torture.toml")
	  set(_wall_torture_wall_jump_script
	      "${CMAKE_SOURCE_DIR}/assets/input_scripts/wall_torture_wall_jump.toml"
	  )
	  set(_wall_torture_dash_wall_kick_script
	      "${CMAKE_SOURCE_DIR}/assets/input_scripts/wall_torture_dash_wall_kick.toml"
	  )
	  set(_wall_torture_wall_climb_script
	      "${CMAKE_SOURCE_DIR}/assets/input_scripts/wall_torture_wall_climb.toml"
	  )
	  set(_wall_torture_wall_descend_script
	      "${CMAKE_SOURCE_DIR}/assets/input_scripts/wall_torture_wall_descend.toml"
	  )
	  set(_wall_torture_wall_slide_script "${_move_instant_run_right_script}")
	  sandbox_add_test(
	    NAME sandbox_smoke_stage_wall_torture_wall_jump
	    COMMAND sandbox --no-prefs --frames 1 --no-render --stage "${_wall_torture_stage}" --character "${CMAKE_SOURCE_DIR}/assets/dev/characters/test_wall.toml" --spawn-point air_wall --input-script "${_wall_torture_wall_jump_script}" --expect-no-respawn --expect-min-vx -1.6 --expect-max-vx -1.4
	  )

	  sandbox_add_test(
	    NAME sandbox_smoke_stage_wall_torture_dash_wall_kick
	    COMMAND sandbox --no-prefs --frames 40 --no-render --stage "${_wall_torture_stage}" --character "${CMAKE_SOURCE_DIR}/assets/dev/characters/megamanx.toml" --spawn-point air_wall --input-script "${_wall_torture_dash_wall_kick_script}" --expect-no-respawn --expect-min-vx -1.8 --expect-max-vx -1.7
	  )

	  sandbox_add_test(
	    NAME sandbox_smoke_stage_wall_torture_wall_climb
	    COMMAND sandbox --no-prefs --frames 30 --no-render --stage "${_wall_torture_stage}" --character "${CMAKE_SOURCE_DIR}/assets/dev/characters/knuckles.toml" --spawn-point air_wall_low --input-script "${_wall_torture_wall_climb_script}" --expect-no-respawn --expect-max-y 330
	  )

	  sandbox_add_test(
	    NAME sandbox_smoke_stage_wall_torture_wall_descend
	    COMMAND sandbox --no-prefs --frames 30 --no-render --stage "${_wall_torture_stage}" --character "${CMAKE_SOURCE_DIR}/assets/dev/characters/knuckles.toml" --spawn-point air_wall_low --input-script "${_wall_torture_wall_descend_script}" --expect-no-respawn --expect-min-y 390
	  )

	  sandbox_add_test(
	    NAME sandbox_smoke_stage_wall_torture_wall_slide
	    COMMAND sandbox --no-prefs --frames 120 --no-render --stage "${_wall_torture_stage}" --character "${CMAKE_SOURCE_DIR}/assets/dev/characters/test_wall.toml" --spawn-point air_wall --input-script "${_wall_torture_wall_slide_script}" --expect-no-respawn --expect-max-y 360
	  )

	  set(_zones_torture_stage "${CMAKE_SOURCE_DIR}/assets/dev/stages/zones_torture.toml")
	  set(_zones_torture_run_right_script
	      "${CMAKE_SOURCE_DIR}/assets/input_scripts/zones_torture_run_right.toml"
	  )
	  set(_zones_torture_run_then_release_script
	      "${CMAKE_SOURCE_DIR}/assets/input_scripts/zones_torture_run_then_release.toml"
	  )
	  set(_zones_env_character
	      "${CMAKE_SOURCE_DIR}/assets/dev/characters/test_env_overrides.toml"
	  )

	  sandbox_add_test(
	    NAME sandbox_smoke_stage_zones_torture_dry_run_right
	    COMMAND sandbox --no-prefs --frames 60 --no-render --stage "${_zones_torture_stage}" --spawn-point default --input-script "${_zones_torture_run_right_script}" --expect-grounded --expect-no-respawn --expect-min-vx 2.8
	  )

	  sandbox_add_test(
	    NAME sandbox_smoke_stage_zones_torture_water_run_right
	    COMMAND sandbox --no-prefs --frames 60 --no-render --stage "${_zones_torture_stage}" --spawn-point water --input-script "${_zones_torture_run_right_script}" --expect-grounded --expect-no-respawn --expect-min-vx 1.3 --expect-max-vx 1.7
	  )

	  sandbox_add_test(
	    NAME sandbox_smoke_stage_zones_torture_ice_slide
	    COMMAND sandbox --no-prefs --frames 60 --no-render --stage "${_zones_torture_stage}" --spawn-point ice --input-script "${_zones_torture_run_then_release_script}" --expect-grounded --expect-no-respawn --expect-min-vx 1.6
	  )

	  sandbox_add_test(
	    NAME sandbox_smoke_stage_zones_torture_water_env_override
	    COMMAND sandbox --no-prefs --frames 60 --no-render --stage "${_zones_torture_stage}" --character "${_zones_env_character}" --spawn-point water --input-script "${_zones_torture_run_right_script}" --expect-grounded --expect-no-respawn --expect-min-vx 2.4 --expect-max-vx 3.1
	  )

	  sandbox_add_test(
	    NAME sandbox_smoke_stage_zones_torture_ice_env_override
	    COMMAND sandbox --no-prefs --frames 60 --no-render --stage "${_zones_torture_stage}" --character "${_zones_env_character}" --spawn-point ice --input-script "${_zones_torture_run_then_release_script}" --expect-grounded --expect-no-respawn --expect-max-vx 0.5
	  )

	  set(_micro_knobs_ref_stage "${CMAKE_SOURCE_DIR}/assets/dev/stages/micro_knobs_ref.toml")
	  set(_micro_knobs_roll_tunnel_script
	      "${CMAKE_SOURCE_DIR}/assets/input_scripts/micro_knobs_roll_tunnel.toml"
	  )
	  set(_micro_knobs_roll_speed_script
	      "${CMAKE_SOURCE_DIR}/assets/input_scripts/micro_knobs_roll_speed.toml"
	  )
	  set(_micro_knobs_air_drag_script
	      "${CMAKE_SOURCE_DIR}/assets/input_scripts/micro_knobs_air_drag_jump_release.toml"
	  )
	  set(_micro_knobs_idle_script
	      "${CMAKE_SOURCE_DIR}/assets/input_scripts/micro_knobs_idle.toml"
	  )
	  set(_micro_knobs_turn_resist_script
	      "${CMAKE_SOURCE_DIR}/assets/input_scripts/micro_knobs_turn_resist_reverse.toml"
	  )
	  sandbox_add_test(
	    NAME sandbox_smoke_stage_micro_knobs_ref_roll_tunnel
	    COMMAND sandbox --no-prefs --frames 120 --no-render --stage "${_micro_knobs_ref_stage}" --character "${CMAKE_SOURCE_DIR}/assets/dev/characters/sonic.toml" --spawn-point roll_tunnel --input-script "${_micro_knobs_roll_tunnel_script}" --expect-grounded --expect-no-respawn --expect-min-x 3070
	  )
	  sandbox_add_test(
	    NAME sandbox_smoke_stage_micro_knobs_ref_roll_speed
	    COMMAND sandbox --no-prefs --frames 80 --no-render --stage "${_micro_knobs_ref_stage}" --character "${CMAKE_SOURCE_DIR}/assets/dev/characters/test_roll_speed.toml" --spawn-point default --input-script "${_micro_knobs_roll_speed_script}" --expect-grounded --expect-no-respawn --expect-min-vx 1.2 --expect-max-vx 1.6
	  )
	  sandbox_add_test(
	    NAME sandbox_smoke_stage_micro_knobs_ref_air_drag
	    COMMAND sandbox --no-prefs --frames 40 --no-render --stage "${_micro_knobs_ref_stage}" --character "${CMAKE_SOURCE_DIR}/assets/dev/characters/test_air_drag.toml" --spawn-point air_drag --input-script "${_micro_knobs_air_drag_script}" --expect-no-respawn --expect-max-vx 0.4
	  )
	  sandbox_add_test(
	    NAME sandbox_smoke_stage_micro_knobs_ref_turn_resist
	    COMMAND sandbox --no-prefs --frames 32 --no-render --stage "${_micro_knobs_ref_stage}" --character "${CMAKE_SOURCE_DIR}/assets/dev/characters/test_turn_resist.toml" --spawn-point turn_resist --input-script "${_micro_knobs_turn_resist_script}" --expect-grounded --expect-no-respawn --expect-max-vx -0.2
	  )
	  sandbox_add_test(
	    NAME sandbox_smoke_stage_micro_knobs_ref_jump_full_clamp_vy
	    COMMAND sandbox --no-prefs --frames 35 --no-render --stage "${_micro_knobs_ref_stage}" --character "${CMAKE_SOURCE_DIR}/assets/dev/characters/sonic.toml" --spawn-point default --input-script "${_mechanics_torture_full_hop_script}" --expect-no-respawn --expect-max-y 438
	  )
	  sandbox_add_test(
	    NAME sandbox_smoke_stage_micro_knobs_ref_jump_short_dual_gravity
	    COMMAND sandbox --no-prefs --frames 15 --no-render --stage "${_micro_knobs_ref_stage}" --character "${CMAKE_SOURCE_DIR}/assets/dev/characters/mario.toml" --spawn-point default --input-script "${_mechanics_torture_short_hop_script}" --expect-no-respawn --expect-max-y 445
	  )
	  sandbox_add_test(
	    NAME sandbox_smoke_stage_micro_knobs_ref_jump_full_fixed
	    COMMAND sandbox --no-prefs --frames 20 --no-render --stage "${_micro_knobs_ref_stage}" --character "${CMAKE_SOURCE_DIR}/assets/dev/characters/simon.toml" --spawn-point default --input-script "${_mechanics_torture_full_hop_script}" --expect-no-respawn --expect-max-y 460
	  )

	  sandbox_add_test(
	    NAME sandbox_smoke_stage_micro_knobs_ref_jump_hold_cap
	    COMMAND sandbox --no-prefs --frames 15 --no-render --stage "${_micro_knobs_ref_stage}" --character "${CMAKE_SOURCE_DIR}/assets/dev/characters/test_jump_hold_cap.toml" --spawn-point default --input-script "${_mechanics_torture_full_hop_script}" --expect-no-respawn --expect-min-y 440 --expect-max-y 450
	  )
	  sandbox_add_test(
	    NAME sandbox_smoke_stage_micro_knobs_ref_jump_rise_gravity_held
	    COMMAND sandbox --no-prefs --frames 18 --no-render --stage "${_micro_knobs_ref_stage}" --character "${CMAKE_SOURCE_DIR}/assets/dev/characters/test_rise_gravity.toml" --spawn-point default --input-script "${_mechanics_torture_full_hop_script}" --expect-no-respawn --expect-max-y 432
	  )
	  sandbox_add_test(
	    NAME sandbox_smoke_stage_micro_knobs_ref_jump_rise_gravity_released
	    COMMAND sandbox --no-prefs --frames 18 --no-render --stage "${_micro_knobs_ref_stage}" --character "${CMAKE_SOURCE_DIR}/assets/dev/characters/test_rise_gravity.toml" --spawn-point default --input-script "${_mechanics_torture_short_hop_script}" --expect-no-respawn --expect-max-y 444
	  )
	  sandbox_add_test(
	    NAME sandbox_smoke_stage_micro_knobs_ref_jump_rise_gravity_off
	    COMMAND sandbox --no-prefs --frames 18 --no-render --stage "${_micro_knobs_ref_stage}" --character "${CMAKE_SOURCE_DIR}/assets/dev/characters/test_rise_gravity_off.toml" --spawn-point default --input-script "${_mechanics_torture_full_hop_script}" --expect-no-respawn --expect-min-y 436 --expect-max-y 440
	  )
	  sandbox_add_test(
	    NAME sandbox_smoke_stage_micro_knobs_ref_jump_release_drop
	    COMMAND sandbox --no-prefs --frames 12 --no-render --stage "${_micro_knobs_ref_stage}" --character "${CMAKE_SOURCE_DIR}/assets/dev/characters/test_release_drop.toml" --spawn-point default --input-script "${_mechanics_torture_short_hop_script}" --expect-no-respawn --expect-min-y 460 --expect-max-y 465
	  )
	  sandbox_add_test(
	    NAME sandbox_smoke_stage_micro_knobs_ref_jump_release_drop_off
	    COMMAND sandbox --no-prefs --frames 12 --no-render --stage "${_micro_knobs_ref_stage}" --character "${CMAKE_SOURCE_DIR}/assets/dev/characters/test_release_drop_off.toml" --spawn-point default --input-script "${_mechanics_torture_short_hop_script}" --expect-no-respawn --expect-min-y 452 --expect-max-y 456
	  )
	  sandbox_add_test(
	    NAME sandbox_smoke_stage_micro_knobs_ref_move_instant
	    COMMAND sandbox --no-prefs --frames 2 --no-render --stage "${_micro_knobs_ref_stage}" --character "${CMAKE_SOURCE_DIR}/assets/dev/characters/test_instant.toml" --spawn-point default --input-script "${_move_instant_run_right_script}" --expect-grounded --expect-no-respawn --expect-min-vx 0.7 --expect-max-vx 0.8
	  )
	  sandbox_add_test(
	    NAME sandbox_smoke_stage_micro_knobs_ref_zone_max_fall_speed
	    COMMAND sandbox --no-prefs --frames 60 --no-render --stage "${_micro_knobs_ref_stage}" --character "${CMAKE_SOURCE_DIR}/assets/dev/characters/sonic.toml" --spawn-point max_fall --input-script "${_micro_knobs_idle_script}" --expect-no-respawn --expect-min-y 330 --expect-max-y 360
	  )
	  sandbox_add_test(
	    NAME sandbox_smoke_stage_micro_knobs_ref_zone_water
	    COMMAND sandbox --no-prefs --frames 60 --no-render --stage "${_micro_knobs_ref_stage}" --spawn-point water --input-script "${_zones_torture_run_right_script}" --expect-grounded --expect-no-respawn --expect-min-vx 1.3 --expect-max-vx 1.7
	  )
	  sandbox_add_test(
	    NAME sandbox_smoke_stage_micro_knobs_ref_zone_friction
	    COMMAND sandbox --no-prefs --frames 45 --no-render --stage "${_micro_knobs_ref_stage}" --spawn-point friction --input-script "${_zones_torture_run_then_release_script}" --expect-grounded --expect-no-respawn --expect-max-vx 0.05
	  )
	  sandbox_add_test(
	    NAME sandbox_smoke_stage_micro_knobs_ref_zone_ground_friction
	    COMMAND sandbox --no-prefs --frames 40 --no-render --stage "${_micro_knobs_ref_stage}" --spawn-point ground_friction --input-script "${_zones_torture_run_then_release_script}" --expect-grounded --expect-no-respawn --expect-min-vx 2.5 --expect-max-vx 2.9
	  )
	  sandbox_add_test(
	    NAME sandbox_smoke_stage_micro_knobs_ref_zone_max_speed
	    COMMAND sandbox --no-prefs --frames 30 --no-render --stage "${_micro_knobs_ref_stage}" --spawn-point max_speed --input-script "${_zones_torture_run_right_script}" --expect-grounded --expect-no-respawn --expect-min-vx 1.4 --expect-max-vx 1.6
	  )
	  sandbox_add_test(
	    NAME sandbox_smoke_stage_micro_knobs_ref_zone_accel
	    COMMAND sandbox --no-prefs --frames 20 --no-render --stage "${_micro_knobs_ref_stage}" --spawn-point low_accel --input-script "${_zones_torture_run_right_script}" --expect-grounded --expect-no-respawn --expect-min-vx 0.7 --expect-max-vx 1.1
	  )
	  sandbox_add_test(
	    NAME sandbox_smoke_stage_micro_knobs_ref_zone_jump_impulse
	    COMMAND sandbox --no-prefs --frames 20 --no-render --stage "${_micro_knobs_ref_stage}" --character "${CMAKE_SOURCE_DIR}/assets/dev/characters/sonic.toml" --spawn-point jump_impulse --input-script "${_mechanics_torture_full_hop_script}" --expect-no-respawn --expect-min-y 456 --expect-max-y 467
	  )
	  sandbox_add_test(
	    NAME sandbox_smoke_stage_micro_knobs_ref_zone_gravity
	    COMMAND sandbox --no-prefs --frames 20 --no-render --stage "${_micro_knobs_ref_stage}" --character "${CMAKE_SOURCE_DIR}/assets/dev/characters/sonic.toml" --spawn-point heavy_gravity --input-script "${_mechanics_torture_full_hop_script}" --expect-no-respawn --expect-min-y 440 --expect-max-y 458
	  )
	  sandbox_add_test(
	    NAME sandbox_smoke_stage_micro_knobs_ref_zone_ice
	    COMMAND sandbox --no-prefs --frames 60 --no-render --stage "${_micro_knobs_ref_stage}" --spawn-point ice --input-script "${_zones_torture_run_then_release_script}" --expect-grounded --expect-no-respawn --expect-min-vx 1.6
	  )
	  sandbox_add_test(
	    NAME sandbox_smoke_stage_micro_knobs_ref_subpixel_quantize
	    COMMAND sandbox --no-prefs --frames 120 --no-render --stage "${_micro_knobs_ref_stage}" --character "${CMAKE_SOURCE_DIR}/assets/dev/characters/test_subpixel256.toml" --spawn-point default --input-script "${_zones_torture_run_right_script}" --expect-grounded --expect-no-respawn --expect-min-x 440 --expect-max-x 465
	  )

	  set(_slide_ceiling_torture_stage "${CMAKE_SOURCE_DIR}/assets/dev/stages/slide_ceiling_torture.toml")
	  set(_slide_ceiling_torture_script
	      "${CMAKE_SOURCE_DIR}/assets/input_scripts/slide_ceiling_torture_slide_under.toml"
	  )
	  sandbox_add_test(
	    NAME sandbox_smoke_stage_slide_ceiling_torture_hold_low
	    COMMAND sandbox --no-prefs --frames 70 --no-render --stage "${_slide_ceiling_torture_stage}" --character "${CMAKE_SOURCE_DIR}/assets/dev/characters/test_slide_window.toml" --spawn-point default --input-script "${_slide_ceiling_torture_script}" --expect-grounded --expect-no-respawn --expect-min-y 480
	  )

	  set(_no_backscroll_torture_stage "${CMAKE_SOURCE_DIR}/assets/dev/stages/no_backscroll_torture.toml")
	  set(_no_backscroll_torture_script
	      "${CMAKE_SOURCE_DIR}/assets/input_scripts/no_backscroll_torture_right_then_left.toml"
	  )
		  sandbox_add_test(
		    NAME sandbox_smoke_stage_no_backscroll_torture_no_backscroll
		    COMMAND sandbox --no-prefs --frames 240 --no-render --stage "${_no_backscroll_torture_stage}" --spawn-point default --input-script "${_no_backscroll_torture_script}" --expect-grounded --expect-no-respawn --expect-no-camera-backscroll
		  )

		  set(_camera_look_torture_stage "${CMAKE_SOURCE_DIR}/assets/dev/stages/camera_look_torture.toml")
		  set(_camera_look_down_script
		      "${CMAKE_SOURCE_DIR}/assets/input_scripts/camera_look_down.toml"
		  )
		  set(_camera_look_up_script
		      "${CMAKE_SOURCE_DIR}/assets/input_scripts/camera_look_up.toml"
		  )
		  sandbox_add_test(
		    NAME sandbox_smoke_stage_camera_look_torture_down
		    COMMAND sandbox --no-prefs --frames 30 --no-render --stage "${_camera_look_torture_stage}" --spawn-point default --input-script "${_camera_look_down_script}" --expect-grounded --expect-no-respawn --expect-min-cam-y 540 --expect-max-cam-y 590
		  )
		  sandbox_add_test(
		    NAME sandbox_smoke_stage_camera_look_torture_up
		    COMMAND sandbox --no-prefs --frames 30 --no-render --stage "${_camera_look_torture_stage}" --spawn-point default --input-script "${_camera_look_up_script}" --expect-grounded --expect-no-respawn --expect-min-cam-y 240 --expect-max-cam-y 300
		  )

		  set(_camera_lock_torture_stage "${CMAKE_SOURCE_DIR}/assets/dev/stages/camera_lock_torture.toml")
		  set(_camera_lock_run_right_script "${_move_instant_run_right_script}")
		  sandbox_add_test(
		    NAME sandbox_smoke_stage_camera_lock_torture_bounds
		    COMMAND sandbox --no-prefs --frames 180 --no-render --stage "${_camera_lock_torture_stage}" --spawn-point default --input-script "${_camera_lock_run_right_script}" --expect-grounded --expect-no-respawn --expect-min-x 2300 --expect-min-cam-x 1739 --expect-max-cam-x 1741
		  )

		  set(_hazards_torture_stage "${CMAKE_SOURCE_DIR}/assets/dev/stages/hazards_torture.toml")
		  sandbox_add_test(
		    NAME sandbox_smoke_stage_hazards_torture_knockback
		    COMMAND sandbox --no-prefs --frames 1 --no-render --stage "${_hazards_torture_stage}" --spawn-point default --expect-no-respawn --expect-hurt-count 1 --expect-min-vx 5.9 --expect-max-vx 6.1
		  )
		  sandbox_add_test(
		    NAME sandbox_smoke_stage_hazards_torture_iframes
		    COMMAND sandbox --no-prefs --frames 240 --no-render --stage "${_hazards_torture_stage}" --spawn-point default --expect-grounded --expect-no-respawn --expect-hurt-count 1 --expect-min-vx 0 --expect-max-vx 0
		  )

		  set(_hazards_ignore_iframes_torture_stage
		      "${CMAKE_SOURCE_DIR}/assets/dev/stages/hazards_ignore_iframes_torture.toml"
		  )
		  sandbox_add_test(
		    NAME sandbox_smoke_stage_hazards_ignore_iframes_torture_rehits
		    COMMAND sandbox --no-prefs --frames 2 --no-render --stage "${_hazards_ignore_iframes_torture_stage}" --spawn-point default --expect-grounded --expect-no-respawn --expect-hurt-count 2
		  )

		  set(_hazards_lockout_torture_stage
		      "${CMAKE_SOURCE_DIR}/assets/dev/stages/hazards_lockout_torture.toml"
		  )
		  set(_hazards_lockout_run_right_script
		      "${CMAKE_SOURCE_DIR}/assets/input_scripts/move_instant_run_right.toml"
		  )
		  sandbox_add_test(
		    NAME sandbox_smoke_stage_hazards_lockout_torture_lockout
		    COMMAND sandbox --no-prefs --frames 2 --no-render --stage "${_hazards_lockout_torture_stage}" --character "${CMAKE_SOURCE_DIR}/assets/dev/characters/test_instant.toml" --spawn-point default --input-script "${_hazards_lockout_run_right_script}" --expect-grounded --expect-no-respawn --expect-hurt-count 1 --expect-max-vx -5
		  )

		  set(_enemy_torture_stage "${CMAKE_SOURCE_DIR}/assets/dev/stages/enemy_torture.toml")
		  set(_enemy_torture_stomp_script
		      "${CMAKE_SOURCE_DIR}/assets/input_scripts/enemy_torture_stomp.toml"
		  )
		  set(_enemy_torture_attack_script
		      "${CMAKE_SOURCE_DIR}/assets/input_scripts/enemy_torture_attack.toml"
		  )
		  sandbox_add_test(
		    NAME sandbox_smoke_stage_enemy_torture_stomp
		    COMMAND sandbox --no-prefs --frames 40 --no-render --stage "${_enemy_torture_stage}" --spawn-point stomp --input-script "${_enemy_torture_stomp_script}" --expect-no-respawn --expect-hurt-count 0 --expect-enemy-kills 1
		  )
		  sandbox_add_test(
		    NAME sandbox_smoke_stage_enemy_torture_attack
		    COMMAND sandbox --no-prefs --frames 30 --no-render --stage "${_enemy_torture_stage}" --character "${CMAKE_SOURCE_DIR}/assets/characters/nimbus.toml" --spawn-point attack --input-script "${_enemy_torture_attack_script}" --expect-grounded --expect-no-respawn --expect-hurt-count 0 --expect-enemy-kills 1
		  )
		endif()
