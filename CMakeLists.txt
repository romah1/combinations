cmake_minimum_required(VERSION 3.13)

include(test/Strict.cmake)

set(PROJECT_NAME combinations)
project(${PROJECT_NAME})

# Set up the compiler flags
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")
find_package(Filesystem REQUIRED)

# XML parsing subproject
add_subdirectory(xml)
add_library(xml_lib ALIAS pugixml)

# Inlcude directories
set(COMMON_INCLUDES ${PROJECT_SOURCE_DIR}/include)
include_directories(${COMMON_INCLUDES})

# Source files
file(GLOB SRC_FILES ${PROJECT_SOURCE_DIR}/src/*.cpp)

# Separate executable: main
list(REMOVE_ITEM SRC_FILES ${PROJECT_SOURCE_DIR}/src/main.cpp)

# Compile source files into a library
add_library(combinations_lib ${SRC_FILES})
target_compile_options(combinations_lib PUBLIC ${COMPILE_OPTS})
target_link_options(combinations_lib PUBLIC ${LINK_OPTS})
target_link_libraries(combinations_lib PUBLIC xml_lib std::filesystem)
setup_warnings(combinations_lib)

# Main is separate
add_executable(combinations ${PROJECT_SOURCE_DIR}/src/main.cpp)
target_compile_options(combinations PUBLIC ${COMPILE_OPTS})
target_link_options(combinations PUBLIC ${LINK_OPTS})
target_link_libraries(combinations combinations_lib)
setup_warnings(combinations)

# google test is a git submodule
add_subdirectory(googletest)

enable_testing()

# test is a git submodule
add_subdirectory(test)

add_test(NAME tests COMMAND runUnitTests)
