FetchContent_Declare(monocypher
    URL https://monocypher.org/download/monocypher-4.0.2.tar.gz
    URL_HASH SHA256=38d07179738c0c90677dba3ceb7a7b8496bcfea758ba1a53e803fed30ae0879c
    SOURCE_SUBDIR metin2-unused
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

FetchContent_MakeAvailable(monocypher)

add_library(monocypher STATIC "${monocypher_SOURCE_DIR}/src/monocypher.c")
target_include_directories(monocypher
    SYSTEM PUBLIC "${monocypher_SOURCE_DIR}/src"
)
