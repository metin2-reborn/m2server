cmake_minimum_required(VERSION 3.25)

project(metin2server
    VERSION 1.0
    LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
set(CMAKE_C_STANDARD 11)
set(CMAKE_POSITION_INDEPENDENT_CODE OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_POLICY_VERSION_MINIMUM 3.10)

add_subdirectory(vendor)

add_compile_options(
    -Wall -Wextra -Wno-unused-parameter
    $<$<CXX_COMPILER_ID:GNU>:-Wno-format-truncation>
    $<$<CONFIG:Debug>:-Werror>
)

add_subdirectory(src/common)
add_subdirectory(src/db)
add_subdirectory(src/game)
add_subdirectory(src/libgame)
add_subdirectory(src/libnet)
add_subdirectory(src/libpoly)
add_subdirectory(src/libsql)
add_subdirectory(src/libthecore)

option(BUILD_TESTING "Build the unit tests" ON)
if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_INSTALL_RPATH "$ORIGIN/../lib")

install(TARGETS server db RUNTIME DESTINATION bin)
install(PROGRAMS dist/metin2-runtime DESTINATION bin)
