cmake_minimum_required(VERSION 3.5)

project(yaml-cli LANGUAGES C VERSION 0.0.4)

set(CMAKE_VERBOSE_MAKEFILE ON)

#find_package(yaml REQUIRED)

# The vendored libyaml declares cmake_minimum_required(VERSION 3.0), which
# CMake >= 4.0 refuses to honour. Grant it the oldest still-supported policy
# version so the subproject configures without patching vendored sources.
if(NOT DEFINED CMAKE_POLICY_VERSION_MINIMUM)
  set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
endif()

# Keep `ctest` focused on yaml-cli's own cases: the vendored libyaml only
# registers its suite when BUILD_TESTING is ON, so default it OFF here.
set(BUILD_TESTING OFF CACHE BOOL "Build the vendored libyaml test tree")

add_subdirectory(libyaml)

add_executable(${PROJECT_NAME} "")

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  message(STATUS "Build DEBUG configuration")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ggdb -O0 -fno-omit-frame-pointer -fsanitize=address")
  set(CMAKE_LINKER_FLAGS "${CMAKE_LINKER_FLAGS} -fsanitize=address")
endif()

file(GLOB_RECURSE PRJ_SRCS src/*.c src/*.h)
target_sources(${PROJECT_NAME} PRIVATE ${PRJ_SRCS})

target_compile_definitions(${PROJECT_NAME} PRIVATE PROJECT_VERSION="${CMAKE_PROJECT_VERSION}")

target_include_directories(${PROJECT_NAME} SYSTEM PUBLIC
    ${PROJECT_SOURCE_DIR}/src
    )

#target_link_libraries(${PROJECT_NAME} PUBLIC ${yaml_LIBRARIES})
target_link_libraries(${PROJECT_NAME} PUBLIC yaml)

install (TARGETS ${PROJECT_NAME} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# --- Tests -----------------------------------------------------------------
# Black-box tests drive the built binary (tests/run.sh). Each case is
# registered separately so `ctest` and CI report failures per case. Run with:
#   ctest --test-dir build --output-on-failure
enable_testing()

set(YAML_CLI_TEST_CASES
    get get_nested get_missing
    set set_output_file set_new_key
    del del_prunes_empty_parent del_prunes_recursive
    get_array_element get_array_whole set_array_replace set_array_append
    del_array_element del_array_prunes_empty numeric_mapping_key
    unknown_option no_mode missing_file)

foreach(case IN LISTS YAML_CLI_TEST_CASES)
    add_test(
        NAME ${case}
        COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/tests/run.sh
                $<TARGET_FILE:${PROJECT_NAME}> ${case})
endforeach()
