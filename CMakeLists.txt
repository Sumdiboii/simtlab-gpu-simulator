cmake_minimum_required(VERSION 3.20)

project(SimtLab VERSION 0.1.0 LANGUAGES CXX)

include(CTest)

option(BUILD_TESTING "Build and register tests" ON)

add_executable(simtlab
    src/main.cpp
)

target_compile_features(simtlab PRIVATE cxx_std_20)
set_target_properties(simtlab PROPERTIES
    CXX_EXTENSIONS OFF
)

if(MSVC)
    target_compile_options(simtlab PRIVATE /W4)
else()
    target_compile_options(simtlab PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
