find_package(PkgConfig REQUIRED)

pkg_check_modules(MARIADB REQUIRED IMPORTED_TARGET libmariadb>=3.4)

add_library(mariadb INTERFACE)

target_link_libraries(mariadb
    INTERFACE PkgConfig::MARIADB
)
