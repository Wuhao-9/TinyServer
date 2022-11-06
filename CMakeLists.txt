cmake_minimum_required(VERSION 3.0)

project(Webserver)

set(CMAKE_BUILD_TYPE "Release")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -o0 -ggdb")

include_directories(${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/logger)

add_subdirectory(logger)

aux_source_directory(${CMAKE_SOURCE_DIR}/src SRC)

add_executable(webserver ${SRC})

target_link_libraries(webserver logger pthread)
