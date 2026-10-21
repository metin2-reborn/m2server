# Every library here is a folder of src/metin2 and the namespace of the same name; sources include
# each other as <metin2/...> from src/.
function(m2_library name)
    cmake_parse_arguments(PARSE_ARGV 1 LIB "" "" "SOURCES")
    add_library(${name} STATIC ${LIB_SOURCES})
    target_include_directories(${name} PUBLIC ${PROJECT_SOURCE_DIR}/src)
endfunction()

m2_library(metin2_log SOURCES log/log.cpp log/log.h)

file(GLOB CORE_SOURCES CONFIGURE_DEPENDS core/*.cpp core/*.h)
m2_library(metin2_core SOURCES ${CORE_SOURCES})
target_link_libraries(metin2_core PUBLIC metin2_log)

file(GLOB NET_SOURCES CONFIGURE_DEPENDS net/*.cpp net/*.h)
m2_library(metin2_net SOURCES ${NET_SOURCES})
target_link_libraries(metin2_net PUBLIC asio metin2_core metin2_log)

file(GLOB SQL_SOURCES CONFIGURE_DEPENDS sql/*.cpp sql/*.h)
m2_library(metin2_sql SOURCES ${SQL_SOURCES})
target_link_libraries(metin2_sql PUBLIC mariadb metin2_core metin2_log)

file(GLOB POLY_SOURCES CONFIGURE_DEPENDS poly/*.cpp poly/*.h)
m2_library(metin2_poly SOURCES ${POLY_SOURCES})

file(GLOB COMMON_SOURCES CONFIGURE_DEPENDS common/*.cpp common/*.h proto/*.h)
m2_library(metin2_common SOURCES ${COMMON_SOURCES})
target_link_libraries(metin2_common PUBLIC metin2_core metin2_log PRIVATE asio)

m2_library(metin2_config SOURCES config/config.cpp config/config.h config/settings.h)
target_link_libraries(metin2_config PUBLIC metin2_log PRIVATE tomlplusplus)

find_package(ZLIB REQUIRED)
find_package(OpenSSL REQUIRED)

file(GLOB_RECURSE GAME_SOURCES CONFIGURE_DEPENDS game/*.cpp game/*.h game/*.hpp)
m2_library(metin2_game SOURCES ${GAME_SOURCES})
target_link_libraries(metin2_game
    PUBLIC metin2_common metin2_config metin2_core metin2_log metin2_net metin2_poly metin2_sql
    PRIVATE lua50 lua50_internals lzo2 monocypher stb ZLIB::ZLIB OpenSSL::Crypto
)

file(GLOB_RECURSE DB_SOURCES CONFIGURE_DEPENDS db/*.cpp db/*.h)
m2_library(metin2_db SOURCES ${DB_SOURCES})
target_link_libraries(metin2_db PUBLIC metin2_common metin2_config metin2_core metin2_log metin2_net metin2_poly metin2_sql)

# One executable for every role; server.toml decides which one a process runs.
add_executable(server main.cpp)
target_link_libraries(server PRIVATE metin2_db metin2_game metin2_config metin2_log)
target_compile_definitions(server PRIVATE M2_VERSION="${PROJECT_VERSION}")
set_target_properties(server PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
