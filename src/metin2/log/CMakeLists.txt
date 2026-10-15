add_library(metin2_log
    log.cpp
    log.h
)

target_include_directories(metin2_log PUBLIC ${PROJECT_SOURCE_DIR}/src)
