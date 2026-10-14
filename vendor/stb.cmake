FetchContent_Declare(stb
    URL https://github.com/nothings/stb/archive/2c980bb59875b0d32144a71867fbdebb2f77cd20.tar.gz
    URL_HASH SHA256=9a955b1b49a4410088a2e0ee2a9c057c3c907d0c1d75454144cb980aca0ba515
    SOURCE_SUBDIR metin2-unused
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

FetchContent_MakeAvailable(stb)

add_library(stb INTERFACE)

target_include_directories(stb
    SYSTEM INTERFACE "${stb_SOURCE_DIR}"
)
