cmake_minimum_required(VERSION 3.20)

set(PROJECT_NAME nosystem)

project(${PROJECT_NAME} VERSION 0.1 LANGUAGES CXX)
project(${PROJECT_NAME} VERSION 0.1 LANGUAGES C)

set(YAML_BUILD_SHARED_LIBS OFF)
set(YAML_CPP_BUILD_TOOLS OFF)

add_subdirectory(3rdparty/yaml-cpp)
include_directories(3rdpart/yaml-cpp/include)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fuse-cxa-atexit")

add_library(${PROJECT_NAME}
    SHARED

    nosystem.cpp
    nosystem.h
)

target_link_libraries(${PROJECT_NAME}
    yaml-cpp
)

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)

set_target_properties(${PROJECT_NAME} PROPERTIES
    FRAMEWORK TRUE
    PUBLIC_HEADER "nosystem.h"
    MACOSX_FRAMEWORK_IDENTIFIER me.fredl.nosystem
    MACOSX_FRAMEWORK_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_FRAMEWORK_SHORT_VERSION_STRING ${PROJECT_VERSION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    XCODE_ATTRIBUTE_SKIP_INSTALL "Yes"
)
