find_package(PkgConfig REQUIRED)

pkg_check_modules(LZO2 REQUIRED IMPORTED_TARGET lzo2)

add_library(lzo2 INTERFACE)

target_link_libraries(lzo2
    INTERFACE PkgConfig::LZO2
)
