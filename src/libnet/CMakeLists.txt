add_library(net
    connection.cpp
    io.cpp
    listener.cpp
)

target_include_directories(net PUBLIC include)

target_link_libraries(net
    PUBLIC asio
    PRIVATE metin2_log thecore
)
