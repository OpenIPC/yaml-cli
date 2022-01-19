cmake_minimum_required(VERSION 3.5)

project(yaml-cli LANGUAGES C VERSION 0.0.4)

set(CMAKE_VERBOSE_MAKEFILE ON)

#find_package(yaml REQUIRED)

add_subdirectory(libyaml)

add_executable(${PROJECT_NAME} "")

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  message(STATUS "Build DEBUG configuration")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ggdb -O0 -fno-omit-frame-pointer -fsanitize=address")
  set(CMAKE_LINKER_FLAGS "${CMAKE_LINKER_FLAGS} -fsanitize=address")
endif()

file(GLOB_RECURSE PRJ_SRCS src/*.c src/*.h yaml-cli.astylerc)
target_sources(${PROJECT_NAME} PRIVATE ${PRJ_SRCS})

target_compile_definitions(${PROJECT_NAME} PRIVATE PROJECT_VERSION="${CMAKE_PROJECT_VERSION}")

target_include_directories(${PROJECT_NAME} SYSTEM PUBLIC
    ${PROJECT_SOURCE_DIR}/src
    )

#target_link_libraries(${PROJECT_NAME} PUBLIC ${yaml_LIBRARIES})
target_link_libraries(${PROJECT_NAME} PUBLIC yaml)

install (TARGETS ${PROJECT_NAME} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
