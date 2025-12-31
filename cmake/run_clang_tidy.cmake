cmake_minimum_required(VERSION 3.20)

if(NOT DEFINED CLANG_TIDY_EXE)
  message(FATAL_ERROR "CLANG_TIDY_EXE is required")
endif()
if(NOT DEFINED BUILD_DIR)
  message(FATAL_ERROR "BUILD_DIR is required")
endif()
if(NOT DEFINED FILE_LIST)
  message(FATAL_ERROR "FILE_LIST is required")
endif()
if(NOT DEFINED SOURCE_DIR)
  message(FATAL_ERROR "SOURCE_DIR is required")
endif()

file(TO_CMAKE_PATH "${SOURCE_DIR}" _source_dir)
set(_header_filter "^${_source_dir}/src/")

if(NOT EXISTS "${FILE_LIST}")
  message(FATAL_ERROR "FILE_LIST does not exist: ${FILE_LIST}")
endif()

file(STRINGS "${FILE_LIST}" _files)

foreach(_file IN LISTS _files)
  if(_file STREQUAL "")
    continue()
  endif()
  execute_process(
    COMMAND "${CLANG_TIDY_EXE}" "-p=${BUILD_DIR}" "-header-filter=${_header_filter}" "${_file}"
    RESULT_VARIABLE _res
  )
  if(NOT _res EQUAL 0)
    message(FATAL_ERROR "clang-tidy failed (${_res}): ${_file}")
  endif()
endforeach()
