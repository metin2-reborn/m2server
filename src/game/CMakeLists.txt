


file(GLOB_RECURSE GAME_SOURCES "*.cpp" "*.h")

add_executable(server ${GAME_SOURCES})

find_package(ZLIB REQUIRED)
find_package(OpenSSL REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LZO2 REQUIRED IMPORTED_TARGET lzo2)

target_link_libraries(server
    PRIVATE common game net poly sql thecore
    PRIVATE boost lua50 lzo2 mariadb monocypher stb
    ZLIB::ZLIB OpenSSL::Crypto
)

target_include_directories(server PRIVATE ../)
