add_library(thecore
    buffer.cpp
    hangul.cpp
    heart.cpp
    internal.h
    log.cpp
    main.cpp
    signal.cpp
    utils.cpp
)

target_include_directories(thecore PUBLIC include)