cmake_minimum_required(VERSION 3.20)
project(libaegisub LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(LIBAEGISUB_SOURCES
    src/dialogue_parser.cpp
    src/karaoke.cpp
    src/string_codec.cpp
    src/time.cpp
    src/color.cpp
    src/keyframe.cpp
    src/vfr.cpp
    src/ycbcr.cpp
    src/ycbcr_conv.cpp
    src/fs.cpp
    src/io.cpp
    src/util.cpp
)

add_library(libaegisub STATIC ${LIBAEGISUB_SOURCES})

target_include_directories(libaegisub PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(MSVC)
    target_compile_options(libaegisub PRIVATE /permissive- /utf-8 /EHsc)
endif()
