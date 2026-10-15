add_library(thecore
    heart.cpp
    internal.h
    main.cpp
    signal.cpp
    utils.cpp
)

target_include_directories(thecore PUBLIC include)
target_link_libraries(thecore PUBLIC metin2_log)
