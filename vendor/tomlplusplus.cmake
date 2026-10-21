FetchContent_Declare(tomlplusplus
    URL https://github.com/marzer/tomlplusplus/archive/refs/tags/v3.4.0.tar.gz
    URL_HASH SHA256=8517f65938a4faae9ccf8ebb36631a38c1cadfb5efa85d9a72e15b9e97d25155
    SOURCE_SUBDIR metin2-unused
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

FetchContent_MakeAvailable(tomlplusplus)

add_library(tomlplusplus INTERFACE)

target_include_directories(tomlplusplus
    SYSTEM INTERFACE "${tomlplusplus_SOURCE_DIR}/include"
)

target_compile_definitions(tomlplusplus INTERFACE TOML_EXCEPTIONS=0)
