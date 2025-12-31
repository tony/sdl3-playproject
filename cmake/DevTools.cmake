# Local developer tooling targets (format/lint/check).
#
# Keep these as explicit targets so builds remain fast and don't require extra
# tools unless the developer opts in to running them.

file(GLOB_RECURSE SANDBOX_TOOLING_CPP CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/src/*.cpp")
file(GLOB_RECURSE SANDBOX_TOOLING_HEADERS CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/src/*.h")
set(SANDBOX_TOOLING_FILES ${SANDBOX_TOOLING_CPP} ${SANDBOX_TOOLING_HEADERS})

find_program(SANDBOX_CLANG_FORMAT_EXE NAMES clang-format)
find_program(SANDBOX_CLANG_TIDY_EXE NAMES clang-tidy)
find_program(SANDBOX_CPPCHECK_EXE NAMES cppcheck)
find_program(SANDBOX_CPPLINT_EXE NAMES cpplint)
find_program(SANDBOX_IWYU_EXE NAMES include-what-you-use iwyu)
find_program(SANDBOX_IWYU_TOOL_EXE NAMES iwyu_tool.py iwyu_tool)

function(_sandbox_missing_tool target tool install_hint)
  add_custom_target(
    ${target}
    COMMAND ${CMAKE_COMMAND} -E echo "${tool} not found (${install_hint})"
    COMMAND ${CMAKE_COMMAND} -E false
    VERBATIM
  )
endfunction()

if(SANDBOX_CLANG_FORMAT_EXE)
  add_custom_target(
    format
    COMMAND ${SANDBOX_CLANG_FORMAT_EXE} -i -style=file ${SANDBOX_TOOLING_FILES}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    VERBATIM
  )
else()
  _sandbox_missing_tool(format clang-format "install clang-format")
endif()

if(SANDBOX_CLANG_TIDY_EXE)
  set(_sandbox_clang_tidy_list "${CMAKE_BINARY_DIR}/clang_tidy_files.txt")
  file(WRITE "${_sandbox_clang_tidy_list}" "")
  foreach(_f IN LISTS SANDBOX_TOOLING_CPP)
    file(APPEND "${_sandbox_clang_tidy_list}" "${_f}\n")
  endforeach()

  add_custom_target(
    tidy
    COMMAND ${CMAKE_COMMAND}
            -DCLANG_TIDY_EXE=${SANDBOX_CLANG_TIDY_EXE}
            -DBUILD_DIR=${CMAKE_BINARY_DIR}
            -DFILE_LIST=${_sandbox_clang_tidy_list}
            -DSOURCE_DIR=${CMAKE_SOURCE_DIR}
            -P ${CMAKE_SOURCE_DIR}/cmake/run_clang_tidy.cmake
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    VERBATIM
  )
else()
  _sandbox_missing_tool(tidy clang-tidy "install clang-tidy")
endif()

if(SANDBOX_CPPCHECK_EXE)
  add_custom_target(
    cppcheck
    COMMAND ${SANDBOX_CPPCHECK_EXE}
            --enable=warning,performance,portability
            --suppress=missingIncludeSystem
            --suppress=unmatchedSuppression
            --suppress=missingInclude
            --suppress=duplicateAssignExpression
            --quiet
            --std=c++23
            --language=c++
            -I${CMAKE_SOURCE_DIR}/src
            --error-exitcode=1
            ${SANDBOX_TOOLING_FILES}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    VERBATIM
  )
else()
  _sandbox_missing_tool(cppcheck cppcheck "install cppcheck")
endif()

if(SANDBOX_CPPLINT_EXE)
  add_custom_target(
    cpplint
    COMMAND ${SANDBOX_CPPLINT_EXE} ${SANDBOX_TOOLING_FILES}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    VERBATIM
  )
else()
  _sandbox_missing_tool(cpplint cpplint "install cpplint")
endif()

if(SANDBOX_IWYU_TOOL_EXE)
  add_custom_target(
    iwyu
    COMMAND ${SANDBOX_IWYU_TOOL_EXE} -p ${CMAKE_BINARY_DIR} ${CMAKE_SOURCE_DIR}/src --
            -Xiwyu --mapping_file=${CMAKE_SOURCE_DIR}/cmake/iwyu.mapping
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    VERBATIM
  )
else()
  _sandbox_missing_tool(iwyu iwyu_tool.py "install iwyu (need iwyu_tool.py)")
endif()
