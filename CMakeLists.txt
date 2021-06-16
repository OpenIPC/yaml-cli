cmake_minimum_required(VERSION 3.5)

project(yaml-cli LANGUAGES C)

set(CMAKE_VERBOSE_MAKEFILE ON)

find_package(yaml REQUIRED)

add_executable(${PROJECT_NAME} "")

file(GLOB_RECURSE PRJ_SRCS src/*.c src/*.h)
target_sources(${PROJECT_NAME} PRIVATE ${PRJ_SRCS})

target_include_directories(${PROJECT_NAME} SYSTEM PUBLIC
    ${PROJECT_SOURCE_DIR}/src
    )

target_link_libraries(${PROJECT_NAME} PUBLIC ${yaml_LIBRARIES})
