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

# Optional third-party deps (kept for future experimentation).
include(cmake/CPM.cmake)
include(cmake/ThirdParty.cmake)

add_executable(sandbox
  src/main.cpp
  src/core/App.cpp
  src/core/SpriteCache.cpp
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

target_compile_options(sandbox PRIVATE
  $<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang,AppleClang>:-Wall;-Wextra;-Wpedantic>
)

include(cmake/DevTools.cmake)

if(BUILD_TESTING)
  add_test(NAME sandbox_smoke_offscreen
           COMMAND sandbox --frames 120 --video-driver offscreen)
  set_tests_properties(sandbox_smoke_offscreen PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
endif()
