# Optional third-party deps (fetched via CPM).
#
# Keep these optional. Some are OFF by default so builds stay fast and avoid
# extra dependencies unless explicitly requested.

option(SANDBOX_WITH_FMT "Enable fmt (formatting)" OFF)
option(SANDBOX_WITH_SPDLOG "Enable spdlog (logging)" OFF)
option(SANDBOX_WITH_IMGUI "Enable Dear ImGui (debug UI)" OFF)
option(SANDBOX_WITH_TRACY "Enable Tracy client (profiling)" OFF)
option(SANDBOX_WITH_GLM "Enable glm (math types)" OFF)
option(SANDBOX_WITH_ENTT "Enable EnTT (ECS library reference)" OFF)
option(SANDBOX_WITH_TOMLPP "Enable toml++ (TOML parsing)" OFF)

set(SANDBOX_FMT_GIT_TAG "11.0.2" CACHE STRING "fmt git tag/commit")
set(SANDBOX_SPDLOG_GIT_TAG "v1.15.0" CACHE STRING "spdlog git tag/commit")
set(SANDBOX_IMGUI_DOCKING_COMMIT "9ca7ea00c80c1cb95b31cb3efa6f88cea33edf2a" CACHE STRING "imgui docking branch commit")
set(SANDBOX_IMGUI_GIT_TAG "${SANDBOX_IMGUI_DOCKING_COMMIT}" CACHE STRING "imgui git tag/commit")
set(SANDBOX_TRACY_GIT_TAG "v0.11.1" CACHE STRING "tracy git tag/commit")
set(SANDBOX_GLM_GIT_TAG "1.0.1" CACHE STRING "glm git tag/commit")
set(SANDBOX_ENTT_GIT_TAG "v3.14.0" CACHE STRING "EnTT git tag/commit")
set(SANDBOX_TOMLPP_GIT_TAG "v3.4.0" CACHE STRING "toml++ git tag/commit")

# ImGui docking branch is recommended when enabled.
# If the build directory has an old release tag pinned, bump it automatically so
# "ImGui on" keeps working.
if(SANDBOX_WITH_IMGUI AND (SANDBOX_IMGUI_GIT_TAG MATCHES "^v[0-9]+" OR SANDBOX_IMGUI_GIT_TAG MATCHES "^[0-9]+\\.[0-9]+"))
  message(WARNING "SANDBOX_IMGUI_GIT_TAG=${SANDBOX_IMGUI_GIT_TAG} is a release tag; switching to ImGui docking branch commit "
                  "${SANDBOX_IMGUI_DOCKING_COMMIT}. If you want a clean fetch, delete your build directory.")
  set(SANDBOX_IMGUI_GIT_TAG "${SANDBOX_IMGUI_DOCKING_COMMIT}" CACHE STRING "imgui git tag/commit" FORCE)
endif()

# fmt
if(SANDBOX_WITH_FMT OR SANDBOX_WITH_SPDLOG)
  CPMAddPackage(
    NAME fmt
    GITHUB_REPOSITORY fmtlib/fmt
    GIT_TAG ${SANDBOX_FMT_GIT_TAG}
    OPTIONS "FMT_TEST OFF" "FMT_DOC OFF" "FMT_FUZZ OFF"
  )
endif()

# spdlog
if(SANDBOX_WITH_SPDLOG)
  set(SPDLOG_FMT_EXTERNAL ON CACHE BOOL "" FORCE)
  CPMAddPackage(
    NAME spdlog
    GITHUB_REPOSITORY gabime/spdlog
    GIT_TAG ${SANDBOX_SPDLOG_GIT_TAG}
    OPTIONS
      "SPDLOG_BUILD_TESTS OFF"
      "SPDLOG_BUILD_EXAMPLE OFF"
      "SPDLOG_BUILD_BENCH OFF"
      "SPDLOG_FMT_EXTERNAL ON"
  )
endif()

# glm (header-only)
if(SANDBOX_WITH_GLM)
  CPMAddPackage(
    NAME glm
    GITHUB_REPOSITORY g-truc/glm
    GIT_TAG ${SANDBOX_GLM_GIT_TAG}
    DOWNLOAD_ONLY YES
  )
  if(glm_ADDED AND NOT TARGET glm::glm)
    add_library(glm_glm INTERFACE)
    add_library(glm::glm ALIAS glm_glm)
    target_include_directories(glm_glm INTERFACE "${glm_SOURCE_DIR}")
  endif()
endif()

# EnTT (header-only)
if(SANDBOX_WITH_ENTT)
  CPMAddPackage(
    NAME entt
    GITHUB_REPOSITORY skypjack/entt
    GIT_TAG ${SANDBOX_ENTT_GIT_TAG}
    DOWNLOAD_ONLY YES
  )
  if(entt_ADDED AND NOT TARGET EnTT::EnTT)
    add_library(entt_entt INTERFACE)
    add_library(EnTT::EnTT ALIAS entt_entt)
    if(EXISTS "${entt_SOURCE_DIR}/src")
      target_include_directories(entt_entt INTERFACE "${entt_SOURCE_DIR}/src")
    elseif(EXISTS "${entt_SOURCE_DIR}/single_include")
      target_include_directories(entt_entt INTERFACE "${entt_SOURCE_DIR}/single_include")
    else()
      target_include_directories(entt_entt INTERFACE "${entt_SOURCE_DIR}")
    endif()
  endif()
endif()

# Dear ImGui
if(SANDBOX_WITH_IMGUI)
  CPMAddPackage(
    NAME imgui
    GITHUB_REPOSITORY ocornut/imgui
    GIT_TAG ${SANDBOX_IMGUI_GIT_TAG}
    DOWNLOAD_ONLY YES
  )

  if(imgui_ADDED AND NOT TARGET imgui::imgui)
    add_library(imgui STATIC)
    add_library(imgui::imgui ALIAS imgui)

    target_sources(imgui PRIVATE
      "${imgui_SOURCE_DIR}/imgui.cpp"
      "${imgui_SOURCE_DIR}/imgui_draw.cpp"
      "${imgui_SOURCE_DIR}/imgui_tables.cpp"
      "${imgui_SOURCE_DIR}/imgui_widgets.cpp"
    )

    if(EXISTS "${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp")
      target_sources(imgui PRIVATE "${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp")
      target_include_directories(imgui PUBLIC "${imgui_SOURCE_DIR}/backends")
      target_link_libraries(imgui PUBLIC SDL3::SDL3)
    endif()

    if(EXISTS "${imgui_SOURCE_DIR}/backends/imgui_impl_sdlrenderer3.cpp")
      target_sources(imgui PRIVATE "${imgui_SOURCE_DIR}/backends/imgui_impl_sdlrenderer3.cpp")
    endif()

    target_include_directories(imgui PUBLIC "${imgui_SOURCE_DIR}")
  endif()
endif()

# Tracy client
if(SANDBOX_WITH_TRACY)
  find_package(Threads REQUIRED)

  CPMAddPackage(
    NAME tracy
    GITHUB_REPOSITORY wolfpld/tracy
    GIT_TAG ${SANDBOX_TRACY_GIT_TAG}
    DOWNLOAD_ONLY YES
  )

  if(tracy_ADDED AND NOT TARGET tracy::tracy_client)
    set(_tracy_client_cpp "")
    if(EXISTS "${tracy_SOURCE_DIR}/public/TracyClient.cpp")
      set(_tracy_client_cpp "${tracy_SOURCE_DIR}/public/TracyClient.cpp")
    elseif(EXISTS "${tracy_SOURCE_DIR}/public/tracy/TracyClient.cpp")
      set(_tracy_client_cpp "${tracy_SOURCE_DIR}/public/tracy/TracyClient.cpp")
    endif()

    if(NOT _tracy_client_cpp STREQUAL "")
      add_library(tracy_client STATIC "${_tracy_client_cpp}")
      add_library(tracy::tracy_client ALIAS tracy_client)
      target_include_directories(tracy_client PUBLIC "${tracy_SOURCE_DIR}/public")
      target_compile_definitions(tracy_client PUBLIC TRACY_ENABLE)
      target_link_libraries(tracy_client PUBLIC Threads::Threads)
    endif()
  endif()
endif()

# toml++ (header-only)
if(SANDBOX_WITH_TOMLPP)
  CPMAddPackage(
    NAME tomlplusplus
    GITHUB_REPOSITORY marzer/tomlplusplus
    GIT_TAG ${SANDBOX_TOMLPP_GIT_TAG}
    DOWNLOAD_ONLY YES
  )
  if(tomlplusplus_ADDED AND NOT TARGET tomlplusplus::tomlplusplus)
    add_library(tomlplusplus_tomlplusplus INTERFACE)
    add_library(tomlplusplus::tomlplusplus ALIAS tomlplusplus_tomlplusplus)
    target_include_directories(tomlplusplus_tomlplusplus INTERFACE "${tomlplusplus_SOURCE_DIR}/include")
  endif()
endif()
