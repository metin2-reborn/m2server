FetchContent_Declare(asio
    URL https://github.com/chriskohlhoff/asio/archive/refs/tags/asio-1-38-2.tar.gz
    URL_HASH SHA256=9f2648fa483e58a6bf848d970ee0ea650ca19ed7769dfa520ed4f7b8d27af1db
    SOURCE_SUBDIR metin2-unused
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

FetchContent_MakeAvailable(asio)

find_package(Threads REQUIRED)

add_library(asio INTERFACE)

target_include_directories(asio
    SYSTEM INTERFACE "${asio_SOURCE_DIR}/include"
)

target_compile_definitions(asio INTERFACE ASIO_STANDALONE ASIO_NO_DEPRECATED)
target_link_libraries(asio INTERFACE Threads::Threads)
