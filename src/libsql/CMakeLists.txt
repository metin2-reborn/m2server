add_library(sql
    AsyncSQL.cpp
    AsyncSQL.h
)

target_link_libraries(sql
    PUBLIC mariadb
    PRIVATE thecore
)
