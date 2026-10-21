FetchContent_Declare(lua
    URL https://www.lua.org/ftp/lua-5.0.3.tar.gz
    URL_HASH SHA256=1193a61b0e08acaa6eee0eecf29709179ee49c71baebc59b682a25c3b5a45671
    SOURCE_SUBDIR metin2-unused
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

FetchContent_MakeAvailable(lua)

foreach(name llex.c llex.h)
    configure_file("${CMAKE_CURRENT_LIST_DIR}/patches/lua/${name}" "${lua_SOURCE_DIR}/src/${name}" COPYONLY)
endforeach()

file(GLOB lua_core "${lua_SOURCE_DIR}/src/*.c")
list(FILTER lua_core EXCLUDE REGEX "/ltests\\.c$")
file(GLOB lua_lib "${lua_SOURCE_DIR}/src/lib/*.c")

add_library(lua50 STATIC ${lua_core} ${lua_lib})

target_include_directories(lua50 PUBLIC "${lua_SOURCE_DIR}/include")

add_library(lua50_internals INTERFACE)

target_include_directories(lua50_internals INTERFACE "${lua_SOURCE_DIR}/src")
target_link_libraries(lua50_internals INTERFACE lua50)
target_link_libraries(lua50 PUBLIC m ${CMAKE_DL_LIBS})

if(M2_SANITIZE)
    # Lua 5.0 reads its default metatable before setting it up; keep AddressSanitizer, skip UBSan.
    target_compile_options(lua50 PRIVATE -fno-sanitize=undefined)
endif()
