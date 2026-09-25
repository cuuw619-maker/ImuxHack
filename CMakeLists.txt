cmake_minimum_required(VERSION 3.21)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)

project(ImuxHack VERSION 0.1.0)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS src/*.cpp)
add_library(${PROJECT_NAME} SHARED ${SOURCES})

if (NOT DEFINED ENV{GEODE_SDK})
    message(FATAL_ERROR "Unable to find Geode SDK! Please define GEODE_SDK environment variable to point to Geode")
endif()

add_subdirectory($ENV{GEODE_SDK} ${CMAKE_CURRENT_BINARY_DIR}/geode)
setup_geode_mod(${PROJECT_NAME}
    EXTERNALS nwo5.ui-scaling:2.1.1
)

# EditorTab-API publishes its header through the Geode dependency package,
# but its include directory is not exposed by the current SDK toolchain.
# Add both possible package layouts explicitly; nonexistent paths are harmless.
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}/geode-deps/alphalaneous.editortab_api
    ${CMAKE_CURRENT_BINARY_DIR}/geode-deps/alphalaneous.editortab_api/include
)
