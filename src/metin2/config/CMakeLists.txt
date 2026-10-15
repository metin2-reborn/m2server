add_library(metin2_config
    config.cpp
    config.h
    settings.h
)

target_include_directories(metin2_config PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(metin2_config PUBLIC metin2_log PRIVATE tomlplusplus)
