add_library(thecore
    buffer.cpp
    fdwatch.cpp
    hangul.cpp
    heart.cpp
    internal.h
    log.cpp
    main.cpp
    signal.cpp
    socket.cpp
    utils.cpp
)

target_include_directories(thecore PUBLIC include)