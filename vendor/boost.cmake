FetchContent_Declare(boost
    URL https://archives.boost.io/release/1.72.0/source/boost_1_72_0.tar.gz
    URL_HASH SHA256=c66e88d5786f2ca4dbebb14e06b566fb642a1a6947ad8cc9091f9f445134143f
    SOURCE_SUBDIR metin2-unused
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

FetchContent_MakeAvailable(boost)

add_library(boost INTERFACE)

target_include_directories(boost SYSTEM INTERFACE "${boost_SOURCE_DIR}")
