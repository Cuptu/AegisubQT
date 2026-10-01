cmake_minimum_required(VERSION 3.20)
project(AegisubAstraCore VERSION 5.1.0 LANGUAGES C)

set(ASTRA_FFMPEG_SDK "" CACHE PATH "Windows FFmpeg shared SDK containing include/ and MSVC import libraries in lib/")
# FFmpeg 6.1 is the oldest supported SDK (including codec-parameter side data).
if(WIN32 AND ASTRA_FFMPEG_SDK)
    set(_astra_sdk_libs)
    foreach(_astra_component IN ITEMS avformat avcodec avutil swresample swscale)
        set(_astra_library "${ASTRA_FFMPEG_SDK}/lib/${_astra_component}.lib")
        if(NOT EXISTS "${_astra_library}")
            message(FATAL_ERROR "Missing FFmpeg SDK import library: ${_astra_library}")
        endif()
        list(APPEND _astra_sdk_libs "${_astra_library}")
    endforeach()
    if(NOT EXISTS "${ASTRA_FFMPEG_SDK}/include/libavformat/avformat.h")
        message(FATAL_ERROR "Missing FFmpeg SDK headers in ${ASTRA_FFMPEG_SDK}/include")
    endif()
    add_library(astra_ffmpeg_sdk INTERFACE)
    target_include_directories(astra_ffmpeg_sdk INTERFACE "${ASTRA_FFMPEG_SDK}/include")
    target_link_libraries(astra_ffmpeg_sdk INTERFACE ${_astra_sdk_libs})
    set(_astra_ffmpeg_target astra_ffmpeg_sdk)
    set(CMAKE_REQUIRED_INCLUDES "${ASTRA_FFMPEG_SDK}/include")
    set(CMAKE_REQUIRED_LIBRARIES ${_astra_sdk_libs})
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(ASTRA_FFMPEG REQUIRED IMPORTED_TARGET
        libavformat>=60.16 libavcodec>=60.31 libavutil>=58.29
        libswresample>=4.12 libswscale>=7.5)
    set(_astra_ffmpeg_target PkgConfig::ASTRA_FFMPEG)
    set(CMAKE_REQUIRED_LIBRARIES ${_astra_ffmpeg_target})
endif()
include(CheckSymbolExists)
check_symbol_exists(avcodec_get_supported_config "libavcodec/avcodec.h"
    ASTRA_HAVE_SUPPORTED_CONFIG)
unset(CMAKE_REQUIRED_LIBRARIES)
unset(CMAKE_REQUIRED_INCLUDES)

add_library(aegisub_astracore SHARED src/astracore.c)
target_compile_features(aegisub_astracore PRIVATE c_std_11)
target_compile_definitions(aegisub_astracore PRIVATE ASTRACORE_BUILD
    $<$<BOOL:${ASTRA_HAVE_SUPPORTED_CONFIG}>:ASTRA_HAVE_SUPPORTED_CONFIG>)
target_include_directories(aegisub_astracore PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_link_libraries(aegisub_astracore PRIVATE ${_astra_ffmpeg_target})
if(UNIX)
    target_link_libraries(aegisub_astracore PRIVATE m)
endif()
set_target_properties(aegisub_astracore PROPERTIES
    OUTPUT_NAME AstraCore.Native C_VISIBILITY_PRESET hidden
    AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)
if(WIN32)
    set_target_properties(aegisub_astracore PROPERTIES PREFIX "")
elseif(APPLE)
    set_target_properties(aegisub_astracore PROPERTIES INSTALL_RPATH "@loader_path")
else()
    set_target_properties(aegisub_astracore PROPERTIES INSTALL_RPATH "$ORIGIN")
endif()

# Also usable standalone to test the actual C backend without the Qt application.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    include(CTest)
    install(TARGETS aegisub_astracore RUNTIME DESTINATION bin LIBRARY DESTINATION lib)
endif()
if(BUILD_TESTING)
    foreach(probe IN ITEMS probe_smoke test_editing video_contract)
        add_executable(astra_native_${probe} tests/${probe}.c)
        set_target_properties(astra_native_${probe} PROPERTIES
            AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)
        target_link_libraries(astra_native_${probe} PRIVATE aegisub_astracore)
        if(NOT probe STREQUAL "video_contract")
            add_test(NAME astra_native_${probe} COMMAND astra_native_${probe})
        endif()
    endforeach()
    find_program(ASTRA_TEST_FFMPEG ffmpeg REQUIRED)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    # This executable has no link dependency on AstraCore or FFmpeg. It can be
    # copied to the deployed app's executable directory to check dlopen/LoadLibrary.
    add_executable(astra_native_dynamic_load tests/dynamic_load.c)
    target_include_directories(astra_native_dynamic_load PRIVATE include)
    target_link_libraries(astra_native_dynamic_load PRIVATE ${CMAKE_DL_LIBS})
    if(UNIX)
        target_link_libraries(astra_native_dynamic_load PRIVATE m)
    endif()
    set_target_properties(astra_native_dynamic_load PROPERTIES
        AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)
    add_test(NAME astra_native_media_smoke COMMAND ${CMAKE_COMMAND}
        "-DFFMPEG=${ASTRA_TEST_FFMPEG}"
        "-DPROBE=$<TARGET_FILE:astra_native_probe_smoke>"
        "-DVIDEO=$<TARGET_FILE:astra_native_video_contract>"
        "-DLOADER=$<TARGET_FILE:astra_native_dynamic_load>"
        "-DNATIVE=$<TARGET_FILE:aegisub_astracore>"
        "-DPYTHON=${Python3_EXECUTABLE}"
        "-DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/media-test/$<CONFIG>"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/media_smoke.cmake")
    set_tests_properties(astra_native_media_smoke PROPERTIES TIMEOUT 30)
endif()
