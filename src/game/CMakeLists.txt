


file(GLOB_RECURSE GAME_SOURCES "*.cpp" "*.h")

add_library(metin2_game STATIC ${GAME_SOURCES})

find_package(ZLIB REQUIRED)
find_package(OpenSSL REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LZO2 REQUIRED IMPORTED_TARGET lzo2)

target_link_libraries(metin2_game
    PUBLIC common game metin2_config metin2_log net poly sql thecore
    PRIVATE lua50 lzo2 mariadb monocypher stb
    ZLIB::ZLIB OpenSSL::Crypto
)

target_include_directories(metin2_game PRIVATE ../)
