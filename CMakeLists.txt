cmake_minimum_required(VERSION 3.20)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
project(AegisubQT VERSION 4.0.2 LANGUAGES C CXX)
include(CTest)

# Enable Windows Resource Compiler only on Windows (.rc files are platform-specific).
if(WIN32)
    enable_language(RC)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Qt6 installation prefix passed via -DCMAKE_PREFIX_PATH=<path>, e.g.:
#   Windows: cmake -B build -DCMAKE_PREFIX_PATH=C:/Qt
#   macOS:   cmake -B build -DCMAKE_PREFIX_PATH=~/Qt/6.x.x/macos
#   Linux:   system packages or -DCMAKE_PREFIX_PATH=/opt/Qt/6.x.x/gcc_64
find_package(Qt6 6.11.2 REQUIRED COMPONENTS Core Core5Compat Gui Quick Qml QuickControls2 ShaderTools Multimedia LinguistTools)
option(AEGISUB_DEVELOPMENT_QML "Load QML directly from this source checkout" OFF)

# Query Qt QML imports directory at configure time as a fallback import path.
get_target_property(_qt_qmake_executable Qt6::qmake IMPORTED_LOCATION)
if(_qt_qmake_executable)
    execute_process(
        COMMAND "${_qt_qmake_executable}" -query QT_INSTALL_QML
        OUTPUT_VARIABLE QT_INSTALL_QML_PATH
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    execute_process(
        COMMAND "${_qt_qmake_executable}" -query QT_INSTALL_LIBS
        OUTPUT_VARIABLE QT_INSTALL_LIBS_PATH
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
endif()

add_subdirectory(libaegisub)
include(cmake/AutomationRegex.cmake)
option(AEGISUB_BUILD_NATIVE_MEDIA "Build AstraCore from the bundled source (FFmpeg 6.1+ SDK required)" ${UNIX})
if(APPLE AND BUILD_TESTING)
    add_executable(astra_deployed_bridge_smoke tests/deployed_media_bridge.cpp
        src/bridge/AstraCoreBridge.cpp src/bridge/AstraCoreBridge.h)
    target_include_directories(astra_deployed_bridge_smoke PRIVATE src/bridge)
    target_link_libraries(astra_deployed_bridge_smoke PRIVATE Qt6::Gui)
    set_target_properties(astra_deployed_bridge_smoke PROPERTIES
        BUILD_RPATH "@executable_path/../Frameworks")
endif()
if(AEGISUB_BUILD_NATIVE_MEDIA)
    add_subdirectory(third_party/astracore)
endif()

set(SOURCES
    src/main.cpp
    src/bridge/AppLogBuffer.h
    src/bridge/AstraCoreBridge.h
    src/bridge/AstraCoreBridge.cpp
    src/bridge/AegisubCoreBridge.h
    src/bridge/AegisubCoreBridge.cpp
    src/bridge/RecentFilesManager.h
    src/bridge/RecentFilesManager.cpp
    src/model/SubtitleModel.h
    src/model/AssExtraData.h
    src/model/SubtitleModel.cpp
    src/model/StyleStorageManager.h
    src/model/StyleStorageManager.cpp
    src/bridge/LanguageManager.h
    src/bridge/LanguageManager.cpp
    src/video/VideoProvider.h
    src/video/VideoProvider.cpp
    src/video/AstraVideoProvider.h
    src/video/AstraVideoProvider.cpp
    src/video/DummyVideoProvider.h
    src/video/DummyVideoProvider.cpp
    src/video/VideoFrameImageProvider.h
    src/video/VideoSurfaceItem.h
    src/video/VideoSurfaceItem.cpp
    src/video/VideoController.h
    src/video/VideoController.cpp
    src/video/VideoDisplayController.h
    src/video/VideoDisplayController.cpp
    src/video/VisualToolBase.h
    src/video/VisualToolBase.cpp
    src/video/VisualOverride.cpp
    src/video/VisualTools.h
    src/video/VisualTools.cpp
    src/audio/AudioController.h
    src/audio/AudioController.cpp
    src/audio/AudioDisplayController.h
    src/audio/AudioDisplayController.cpp
    src/audio/AudioPcmProvider.h
    src/audio/AudioPcmProvider.cpp
    src/audio/AudioClipWriter.h
    src/audio/AudioClipWriter.cpp
    src/audio/AudioSliceDevice.h
    src/audio/AegisubStftCore.h
    src/audio/AegisubStftCore.cpp
    src/audio/AsyncSpectrumWindow.cpp
    src/audio/SpectrogramShaderMaterial.h
    src/audio/SpectrogramShaderMaterial.cpp
    src/audio/SpectrogramItem.h
    src/audio/SpectrogramItem.cpp
    src/automation/lpeg/lpeg.c
    src/automation/LuaTextExtents.h
    src/automation/LuaTextExtents.cpp
    src/automation/LuaModules.h
    src/automation/LuaModules.cpp
    src/automation/LuaRegex.cpp
    src/automation/LuaAssFileBridge.h
    src/automation/LuaAssFileBridge.cpp
    src/automation/LuaScript.h
    src/automation/LuaScript.cpp
    src/automation/AutomationManager.h
    src/automation/AutomationManager.cpp
)

if(WIN32)
    list(APPEND SOURCES src/aegisub.rc)
endif()

add_executable(AegisubQT WIN32 ${SOURCES})
if(BUILD_TESTING)
    add_executable(async_spectrum_window_test tests/async_spectrum_window.cpp
        src/audio/AsyncSpectrumWindow.cpp src/audio/AegisubStftCore.cpp src/audio/AudioPcmProvider.cpp
        src/bridge/AstraCoreBridge.cpp)
    target_include_directories(async_spectrum_window_test PRIVATE src/audio src/bridge)
    target_link_libraries(async_spectrum_window_test PRIVATE Qt6::Gui Qt6::Test)
    add_test(NAME async_spectrum_window COMMAND async_spectrum_window_test)
    set_tests_properties(async_spectrum_window PROPERTIES TIMEOUT 45)
    add_executable(audio_load_state_test tests/audio_load_state.cpp
        src/audio/SpectrogramItem.cpp src/audio/SpectrogramShaderMaterial.cpp
        src/audio/AudioController.cpp src/audio/AudioSliceDevice.h
        src/audio/AudioPcmProvider.cpp src/audio/AudioClipWriter.cpp src/audio/AegisubStftCore.cpp src/audio/AsyncSpectrumWindow.cpp
        src/video/VideoController.cpp src/video/VideoProvider.cpp src/video/DummyVideoProvider.cpp
        src/video/AstraVideoProvider.cpp src/bridge/AstraCoreBridge.cpp
        src/bridge/AegisubCoreBridge.cpp src/model/SubtitleModel.cpp)
    target_include_directories(audio_load_state_test PRIVATE src/audio src/video src/bridge src/model)
    target_link_libraries(audio_load_state_test PRIVATE Qt6::Gui Qt6::Quick Qt6::Multimedia Qt6::Core5Compat Qt6::Test libaegisub)
    add_dependencies(audio_load_state_test AegisubQT)
    if(AEGISUB_BUILD_NATIVE_MEDIA)
        target_compile_definitions(audio_load_state_test PRIVATE AEGISUB_TEST_EXPECT_NATIVE=1)
        if(NOT WIN32)
            add_custom_command(TARGET audio_load_state_test POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "$<TARGET_FILE:aegisub_astracore>" "$<TARGET_FILE_DIR:audio_load_state_test>")
        endif()
    endif()
    if(MSVC)
        target_compile_options(audio_load_state_test PRIVATE /utf-8)
    endif()
    add_test(NAME audio_load_state COMMAND audio_load_state_test)
    set_tests_properties(audio_load_state PROPERTIES TIMEOUT 45 SKIP_RETURN_CODE 77 ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software")
    add_executable(visual_override_test tests/visual_override.cpp src/video/VisualOverride.cpp)
    target_include_directories(visual_override_test PRIVATE src/video src/bridge)
    target_link_libraries(visual_override_test PRIVATE Qt6::Gui libaegisub)
    if(MSVC)
        target_compile_options(visual_override_test PRIVATE /utf-8)
    endif()
    add_test(NAME visual_override COMMAND visual_override_test)
    add_executable(audio_clip_writer_test tests/audio_clip_writer.cpp
        src/audio/AudioClipWriter.cpp src/audio/AudioPcmProvider.cpp
        src/bridge/AstraCoreBridge.cpp)
    target_include_directories(audio_clip_writer_test PRIVATE src/audio src/bridge)
    target_link_libraries(audio_clip_writer_test PRIVATE Qt6::Gui)
    if(MSVC)
        target_compile_options(audio_clip_writer_test PRIVATE /utf-8)
    endif()
    add_test(NAME audio_clip_writer COMMAND audio_clip_writer_test)
    add_executable(automation_filesystem_test tests/automation_filesystem.cpp)
    target_include_directories(automation_filesystem_test PRIVATE src/automation)
    if(MSVC)
        target_compile_options(automation_filesystem_test PRIVATE /utf-8)
    endif()
    add_test(NAME automation_filesystem COMMAND automation_filesystem_test)
    # Exercise the real POSIX Qt backend even when the developer host is Windows.
    add_executable(automation_text_extents_qt_test tests/automation_text_extents.cpp src/automation/LuaTextExtents.cpp)
    target_include_directories(automation_text_extents_qt_test PRIVATE src/automation)
    target_compile_definitions(automation_text_extents_qt_test PRIVATE AEGISUB_TEST_QT_TEXT_EXTENTS)
    target_link_libraries(automation_text_extents_qt_test PRIVATE Qt6::Gui)
    if(MSVC)
        target_compile_options(automation_text_extents_qt_test PRIVATE /utf-8)
    endif()
    add_test(NAME automation_text_extents_qt COMMAND automation_text_extents_qt_test)
    set_tests_properties(automation_text_extents_qt PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    if(WIN32)
        add_executable(automation_text_extents_gdi_test tests/automation_text_extents.cpp src/automation/LuaTextExtents.cpp)
        target_include_directories(automation_text_extents_gdi_test PRIVATE src/automation)
        target_link_libraries(automation_text_extents_gdi_test PRIVATE Qt6::Gui gdi32)
        if(MSVC)
            target_compile_options(automation_text_extents_gdi_test PRIVATE /utf-8)
        endif()
        add_test(NAME automation_text_extents_gdi COMMAND automation_text_extents_gdi_test)
        set_tests_properties(automation_text_extents_gdi PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    endif()
    # Keep the test C ABI fixture in an isolated application directory: the
    # production application must never discover or deploy this library.
    find_package(Threads REQUIRED)
    set(_astra_test_directory "${CMAKE_CURRENT_BINARY_DIR}/provider-test/$<CONFIG>")
    add_library(astra_native_fixture MODULE tests/astra_native_fixture.cpp)
    target_link_libraries(astra_native_fixture PRIVATE Threads::Threads)
    set_target_properties(astra_native_fixture PROPERTIES OUTPUT_NAME "AstraCore.Native"
        RUNTIME_OUTPUT_DIRECTORY "${_astra_test_directory}/assets/bin"
        ARCHIVE_OUTPUT_DIRECTORY "${_astra_test_directory}/assets/bin"
        LIBRARY_OUTPUT_DIRECTORY "${_astra_test_directory}/assets/bin")
    if(APPLE)
        set_target_properties(astra_native_fixture PROPERTIES SUFFIX ".dylib")
    endif()
    add_executable(astra_provider_lifecycle_test tests/astra_provider_lifecycle.cpp
        src/video/AstraVideoProvider.cpp src/video/AstraVideoProvider.h
        src/video/VideoProvider.h src/bridge/AstraCoreBridge.cpp src/bridge/AstraCoreBridge.h)
    set_target_properties(astra_provider_lifecycle_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${_astra_test_directory}")
    target_include_directories(astra_provider_lifecycle_test PRIVATE src/video src/bridge)
    target_link_libraries(astra_provider_lifecycle_test PRIVATE Qt6::Gui Threads::Threads)
    add_dependencies(astra_provider_lifecycle_test astra_native_fixture)
    foreach(_scenario queued-reopen native-reopen stale metadata close-active close-idle destroy-active shutdown-active)
        add_test(NAME astra_provider_${_scenario} COMMAND astra_provider_lifecycle_test ${_scenario})
        set_tests_properties(astra_provider_${_scenario} PROPERTIES TIMEOUT 15)
    endforeach()
    set(_astra_contract_directory "${CMAKE_CURRENT_BINARY_DIR}/bridge-contract/$<CONFIG>")
    add_library(astra_contract_fixture MODULE tests/astra_contract_fixture.cpp)
    target_compile_definitions(astra_contract_fixture PRIVATE ASTRACORE_BUILD)
    set_target_properties(astra_contract_fixture PROPERTIES OUTPUT_NAME "AstraCore.Native"
        RUNTIME_OUTPUT_DIRECTORY "${_astra_contract_directory}/assets/bin"
        ARCHIVE_OUTPUT_DIRECTORY "${_astra_contract_directory}/assets/bin"
        LIBRARY_OUTPUT_DIRECTORY "${_astra_contract_directory}/assets/bin")
    if(APPLE)
        set_target_properties(astra_contract_fixture PROPERTIES SUFFIX ".dylib")
    endif()
    add_executable(astra_bridge_contract_test tests/astra_bridge_contract.cpp
        src/bridge/AstraCoreBridge.cpp src/bridge/AstraCoreBridge.h)
    target_include_directories(astra_bridge_contract_test PRIVATE src/bridge)
    target_link_libraries(astra_bridge_contract_test PRIVATE Qt6::Gui)
    set_target_properties(astra_bridge_contract_test PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${_astra_contract_directory}")
    add_dependencies(astra_bridge_contract_test astra_contract_fixture)
    foreach(_scenario abi frames spectrum metadata)
        add_test(NAME astra_bridge_${_scenario} COMMAND astra_bridge_contract_test ${_scenario})
        set_tests_properties(astra_bridge_${_scenario} PROPERTIES TIMEOUT 15)
    endforeach()
    add_library(astra_path_dependency SHARED tests/astra_path_dependency.cpp)
    add_library(astra_path_fixture MODULE tests/astra_path_fixture.cpp)
    target_compile_definitions(astra_path_fixture PRIVATE ASTRACORE_BUILD)
    target_link_libraries(astra_path_fixture PRIVATE astra_path_dependency)
    set_target_properties(astra_path_fixture PROPERTIES OUTPUT_NAME "AstraCore.Native")
    foreach(_path_target astra_path_dependency astra_path_fixture)
        set_target_properties(${_path_target} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/path-fixture/$<CONFIG>"
            LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/path-fixture/$<CONFIG>"
            ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/path-fixture/$<CONFIG>")
    endforeach()
    if(APPLE)
        set_target_properties(astra_path_fixture PROPERTIES SUFFIX ".dylib")
    endif()
    add_executable(astra_library_paths_test tests/astra_library_paths.cpp
        src/bridge/AstraCoreBridge.cpp src/bridge/AstraCoreBridge.h)
    target_include_directories(astra_library_paths_test PRIVATE src/bridge)
    target_link_libraries(astra_library_paths_test PRIVATE Qt6::Gui)
    add_dependencies(astra_library_paths_test astra_path_fixture)
    # These spawned relocation tests exercise Win32 dependency-search flags.
    # Unix deployment/rpath execution is separately covered by native media gates.
    if(WIN32)
        foreach(_scenario cwd trusted dependency broken escape)
            add_test(NAME astra_bridge_path_${_scenario} COMMAND astra_library_paths_test
                ${_scenario} $<TARGET_FILE:astra_path_fixture> $<TARGET_FILE:astra_path_dependency>)
            set_tests_properties(astra_bridge_path_${_scenario} PROPERTIES TIMEOUT 15)
        endforeach()
    endif()
endif()

# These shaders have no includes. Explicit dependencies avoid qsb's unescaped
# depfile paths breaking incremental builds in directories containing spaces.
set(AEGISUB_QSB_FILES "")
foreach(_shader spectrogram.vert spectrogram.frag)
    set(_batchable_shader_args "")
    if(_shader STREQUAL "spectrogram.vert")
        list(APPEND _batchable_shader_args --batchable)
    endif()
    set(_source "${CMAKE_CURRENT_SOURCE_DIR}/shaders/${_shader}")
    set(_output "${CMAKE_CURRENT_BINARY_DIR}/.qsb/shaders/${_shader}.qsb")
    add_custom_command(OUTPUT "${_output}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/.qsb/shaders"
        COMMAND Qt6::qsb ${_batchable_shader_args} --glsl "100es,120,150" --hlsl 50 --msl 12 -o "${_output}" "${_source}"
        DEPENDS "${_source}" Qt6::qsb
        VERBATIM
    )
    set_source_files_properties("${_output}" PROPERTIES GENERATED TRUE QT_RESOURCE_ALIAS "shaders/${_shader}.qsb")
    list(APPEND AEGISUB_QSB_FILES "${_output}")
endforeach()
qt6_add_resources(AegisubQT "aegisub_shaders" PREFIX "/" FILES ${AEGISUB_QSB_FILES})
if(BUILD_TESTING)
    qt6_add_resources(audio_load_state_test "audio_test_shaders" PREFIX "/" FILES ${AEGISUB_QSB_FILES})
endif()

# LuaJIT: Discover source from subprojects/luajit, vendor/luajit, or fallback to FetchContent
set(LUAJIT_SRC_DIR "")
foreach(_candidate_dir
    "${CMAKE_CURRENT_SOURCE_DIR}/subprojects/luajit"
    "${CMAKE_CURRENT_SOURCE_DIR}/vendor/luajit"
    "${CMAKE_CURRENT_SOURCE_DIR}/luajit"
)
    if(EXISTS "${_candidate_dir}/src/luaconf.h")
        set(LUAJIT_SRC_DIR "${_candidate_dir}")
        message(STATUS "Found local LuaJIT source at: ${LUAJIT_SRC_DIR}")
        break()
    endif()
endforeach()

if(NOT LUAJIT_SRC_DIR)
    message(STATUS "Local LuaJIT source not found; retrieving via FetchContent...")
    include(FetchContent)
    FetchContent_Declare(
        luajit_repo
        GIT_REPOSITORY https://github.com/LuaJIT/LuaJIT.git
        GIT_TAG        v2.1
        GIT_SHALLOW    TRUE
    )
    FetchContent_GetProperties(luajit_repo)
    if(NOT luajit_repo_POPULATED)
        FetchContent_Populate(luajit_repo)
    endif()
    set(LUAJIT_SRC_DIR "${luajit_repo_SOURCE_DIR}")
endif()
set(LUAJIT_BUILD_ROOT "${CMAKE_CURRENT_BINARY_DIR}/luajit-build")
if(WIN32)
    set(LUAJIT_BUILD_DIR "${LUAJIT_BUILD_ROOT}/$<CONFIG>")
else()
    set(LUAJIT_BUILD_DIR "${LUAJIT_BUILD_ROOT}")
endif()

target_include_directories(AegisubQT PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/audio
    ${CMAKE_CURRENT_SOURCE_DIR}/src/video
    ${CMAKE_CURRENT_SOURCE_DIR}/src/bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/src/model
    ${CMAKE_CURRENT_SOURCE_DIR}/src/automation
    ${LUAJIT_BUILD_DIR}/src
    ${LUAJIT_SRC_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/libaegisub/include
)

if(QT_INSTALL_QML_PATH)
    target_compile_definitions(AegisubQT PRIVATE QT_INSTALL_QML_PATH="${QT_INSTALL_QML_PATH}")
endif()
if(AEGISUB_DEVELOPMENT_QML)
    target_compile_definitions(AegisubQT PRIVATE AEGISUB_DEVELOPMENT_QML="${CMAKE_CURRENT_SOURCE_DIR}/qml")
endif()

# macOS: build a proper .app bundle so macdeployqt can produce a .dmg.
if(APPLE)
    set_target_properties(AegisubQT PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME "AegisubQT"
        MACOSX_BUNDLE_GUI_IDENTIFIER "org.aegisub.aegisubqt"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    )
endif()

# Compile LuaJIT in the build tree, leaving the source checkout untouched.
include(ExternalProject)

if(WIN32)
    set(LUAJIT_STATIC_LIB "${LUAJIT_BUILD_DIR}/src/lua51.lib")
    if(NOT MSVC)
        message(FATAL_ERROR "Windows builds currently require an MSVC Qt kit and compiler")
    endif()
    if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
        set(CMAKE_BUILD_TYPE Release CACHE STRING "Build configuration" FORCE)
    endif()
    set(LUAJIT_BUILD_ARGS "lua52compat $<$<CONFIG:Debug>:debug >static")

    set(LUAJIT_VCVARS_LINE "")
    if(NOT DEFINED ENV{INCLUDE} AND CMAKE_C_COMPILER)
        get_filename_component(_vc_tools_dir "${CMAKE_C_COMPILER}" DIRECTORY)
        get_filename_component(_vc_root "${_vc_tools_dir}/../../../../.." ABSOLUTE)
        set(_vcvarsall "${_vc_root}/Auxiliary/Build/vcvarsall.bat")
        if(EXISTS "${_vcvarsall}")
            if(CMAKE_SIZEOF_VOID_P EQUAL 8)
                set(LUAJIT_VCVARS_LINE "call \"${_vcvarsall}\" x64\r\n")
            else()
                set(LUAJIT_VCVARS_LINE "call \"${_vcvarsall}\" x86\r\n")
            endif()
        endif()
    endif()

    set(LUAJIT_BATCH_FILE "${CMAKE_CURRENT_BINARY_DIR}/build_luajit_$<CONFIG>.bat")
    file(GENERATE OUTPUT "${LUAJIT_BATCH_FILE}"
        CONTENT "@echo off\r\n${LUAJIT_VCVARS_LINE}if errorlevel 1 exit /b %ERRORLEVEL%\r\nset \"CL=/MD$<$<CONFIG:Debug>:d>\"\r\ncd /d \"${LUAJIT_BUILD_DIR}/src\" || exit /b 1\r\ncall msvcbuild.bat ${LUAJIT_BUILD_ARGS}\r\nexit /b %ERRORLEVEL%\r\n"
    )
    find_package(Git QUIET)
    set(_luajit_relver "ROLLING")
    if(GIT_FOUND)
        execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${LUAJIT_SRC_DIR}" show -s --format=%ct
            OUTPUT_VARIABLE _luajit_git_relver OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
        if(_luajit_git_relver MATCHES "^[0-9]+$")
            set(_luajit_relver "${_luajit_git_relver}")
        endif()
    endif()
    if(_luajit_relver STREQUAL "ROLLING" AND EXISTS "${LUAJIT_SRC_DIR}/.relver")
        file(READ "${LUAJIT_SRC_DIR}/.relver" _luajit_archive_relver)
        string(STRIP "${_luajit_archive_relver}" _luajit_archive_relver)
        if(_luajit_archive_relver MATCHES "^[0-9]+$")
            set(_luajit_relver "${_luajit_archive_relver}")
        endif()
    endif()
    file(CONFIGURE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/luajit.relver" CONTENT "${_luajit_relver}\n" @ONLY)
    file(GLOB_RECURSE _luajit_sources CONFIGURE_DEPENDS
        "${LUAJIT_SRC_DIR}/src/*.c" "${LUAJIT_SRC_DIR}/src/*.h"
        "${LUAJIT_SRC_DIR}/src/*.lua" "${LUAJIT_SRC_DIR}/src/*.dasc"
        "${LUAJIT_SRC_DIR}/dynasm/*.lua" "${LUAJIT_SRC_DIR}/src/msvcbuild.bat")
    add_custom_command(OUTPUT "${LUAJIT_STATIC_LIB}"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${LUAJIT_SRC_DIR}/src" "${LUAJIT_BUILD_DIR}/src"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${LUAJIT_SRC_DIR}/dynasm" "${LUAJIT_BUILD_DIR}/dynasm"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_BINARY_DIR}/luajit.relver" "${LUAJIT_BUILD_DIR}/.relver"
        COMMAND cmd.exe /c "${LUAJIT_BATCH_FILE}"
        DEPENDS ${_luajit_sources} "${LUAJIT_BATCH_FILE}" "${CMAKE_CURRENT_BINARY_DIR}/luajit.relver"
        VERBATIM
    )
    add_custom_target(luajit_build DEPENDS "${LUAJIT_STATIC_LIB}")
else()
    set(LUAJIT_STATIC_LIB "${LUAJIT_BUILD_DIR}/src/libluajit.a")
    if(APPLE)
        if(NOT CMAKE_OSX_DEPLOYMENT_TARGET)
            set(AEGISUB_LUAJIT_DEPLOYMENT_TARGET "11.0")
        else()
            set(AEGISUB_LUAJIT_DEPLOYMENT_TARGET "${CMAKE_OSX_DEPLOYMENT_TARGET}")
        endif()
        set(AEGISUB_LUAJIT_BUILD_COMMAND
            ${CMAKE_COMMAND} -E env "MACOSX_DEPLOYMENT_TARGET=${AEGISUB_LUAJIT_DEPLOYMENT_TARGET}" make -C "${LUAJIT_BUILD_DIR}" "XCFLAGS=-DLUAJIT_ENABLE_LUA52COMPAT")
    else()
        set(AEGISUB_LUAJIT_BUILD_COMMAND make -C "${LUAJIT_BUILD_DIR}" "XCFLAGS=-DLUAJIT_ENABLE_LUA52COMPAT")
    endif()

    ExternalProject_Add(luajit_build
        SOURCE_DIR "${LUAJIT_SRC_DIR}"
        DOWNLOAD_COMMAND ""
        CONFIGURE_COMMAND ${CMAKE_COMMAND} -E copy_directory "${LUAJIT_SRC_DIR}" "${LUAJIT_BUILD_DIR}"
        # Rebuild existing objects when this ExternalProject command changes;
        # make does not detect compiler-flag changes from object timestamps.
        BUILD_COMMAND ${AEGISUB_LUAJIT_BUILD_COMMAND} clean
            COMMAND ${AEGISUB_LUAJIT_BUILD_COMMAND}
        BUILD_IN_SOURCE 0
        INSTALL_COMMAND ""
        BUILD_BYPRODUCTS "${LUAJIT_STATIC_LIB}"
    )
endif()

add_library(luajit STATIC IMPORTED GLOBAL)
set_target_properties(luajit PROPERTIES
    IMPORTED_LOCATION "${LUAJIT_STATIC_LIB}"
)
if(WIN32)
    if(CMAKE_CONFIGURATION_TYPES)
        set(_luajit_configs ${CMAKE_CONFIGURATION_TYPES})
    else()
        set(_luajit_configs "${CMAKE_BUILD_TYPE}")
    endif()
    set_property(TARGET luajit PROPERTY IMPORTED_CONFIGURATIONS "${_luajit_configs}")
    foreach(_config IN LISTS _luajit_configs)
        string(TOUPPER "${_config}" _upper_config)
        set_property(TARGET luajit PROPERTY "IMPORTED_LOCATION_${_upper_config}"
            "${LUAJIT_BUILD_ROOT}/${_config}/src/lua51.lib")
    endforeach()
endif()
add_dependencies(luajit luajit_build)
add_dependencies(AegisubQT luajit_build)

if(BUILD_TESTING)
    add_executable(automation_export_filters_test tests/automation_export_filters.cpp
        tests/audio_clip_ui_stubs.h
        tests/editor_timeline.h
        src/automation/LuaScript.cpp src/automation/LuaAssFileBridge.cpp
        src/automation/LuaModules.cpp src/automation/LuaRegex.cpp
        src/automation/LuaTextExtents.cpp src/automation/lpeg/lpeg.c
        src/automation/AutomationManager.cpp src/model/SubtitleModel.cpp
        src/bridge/AegisubCoreBridge.cpp)
    target_include_directories(automation_export_filters_test PRIVATE
        src/automation src/model src/bridge ${LUAJIT_BUILD_DIR}/src ${LUAJIT_SRC_DIR}/src)
    target_link_libraries(automation_export_filters_test PRIVATE
        Qt6::Gui Qt6::Quick Qt6::QuickControls2 Qt6::Core5Compat luajit libaegisub aegisub_automation_regex)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    target_link_libraries(automation_export_filters_test PRIVATE Qt6::Test)
    add_dependencies(automation_export_filters_test luajit_build)
    target_compile_definitions(automation_export_filters_test PRIVATE
        AUTOMATION_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/automation")
    if(MSVC)
        target_compile_options(automation_export_filters_test PRIVATE /utf-8)
    endif()
    if(WIN32)
        add_custom_command(TARGET automation_export_filters_test POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${AEGISUB_ICU_RUNTIME_DLLS}
                "$<TARGET_FILE_DIR:automation_export_filters_test>")
    endif()
    add_test(NAME automation_export_filters COMMAND automation_export_filters_test)
    set_tests_properties(automation_export_filters PROPERTIES TIMEOUT 30 ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software")
endif()

target_link_libraries(AegisubQT PRIVATE
    Qt6::Core
    Qt6::Core5Compat
    Qt6::Gui
    Qt6::Quick
    Qt6::Qml
    Qt6::QuickControls2
    Qt6::Multimedia
    luajit
    libaegisub
    aegisub_automation_regex
)

# Compile translations (.ts -> .qm) as part of the build.
set(AEGISUB_TS_FILES
    locale/aegisub_zh_CN.ts
    locale/aegisub_zh_TW.ts
    locale/aegisub_ja.ts
    locale/aegisub_ko.ts
    locale/aegisub_fr_FR.ts
    locale/aegisub_de.ts
    locale/aegisub_ru.ts
    locale/aegisub_es.ts
)
qt_add_lrelease(AegisubQT
    TS_FILES ${AEGISUB_TS_FILES}
    QM_FILES_OUTPUT_VARIABLE AEGISUB_QM_FILES
)

# Copy runtime assets, QML, automation scripts, and compiled .qm files next to the binary
# (or into Contents/Resources for macOS app bundles).
if(APPLE)
    set(AEGISUB_POST_BUILD_DEST "$<TARGET_BUNDLE_DIR:AegisubQT>/Contents/Resources")
else()
    set(AEGISUB_POST_BUILD_DEST "$<TARGET_FILE_DIR:AegisubQT>")
endif()

set(AEGISUB_POST_BUILD_COMMANDS
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets"
        "${AEGISUB_POST_BUILD_DEST}/assets"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/automation"
        "${AEGISUB_POST_BUILD_DEST}/automation"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/qml"
        "${AEGISUB_POST_BUILD_DEST}/qml"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${AEGISUB_POST_BUILD_DEST}/locale"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${AEGISUB_QM_FILES}
        "${AEGISUB_POST_BUILD_DEST}/locale"
)

set(AEGISUB_MEDIA_RUNTIME_DIR "" CACHE PATH "FFmpeg runtime directory used to deploy media tools and dependencies")
set(AEGISUB_FFMPEG_EXECUTABLE "" CACHE FILEPATH "Complete FFmpeg CLI; defaults to ffmpeg.exe in the media runtime directory")
if(AEGISUB_BUILD_NATIVE_MEDIA)
    add_dependencies(AegisubQT aegisub_astracore)
    if(APPLE)
        set(_astra_deploy_dir "$<TARGET_BUNDLE_DIR:AegisubQT>/Contents/Frameworks")
        install(TARGETS aegisub_astracore LIBRARY DESTINATION AegisubQT.app/Contents/Frameworks)
    else()
        set(_astra_deploy_dir "$<TARGET_FILE_DIR:AegisubQT>")
        install(TARGETS aegisub_astracore RUNTIME DESTINATION bin LIBRARY DESTINATION lib)
    endif()
    if(WIN32 AND AEGISUB_MEDIA_RUNTIME_DIR)
        get_filename_component(_msvc_binary_dir "${CMAKE_LINKER}" DIRECTORY)
        find_program(AEGISUB_DUMPBIN dumpbin HINTS "${_msvc_binary_dir}" REQUIRED)
        list(APPEND AEGISUB_POST_BUILD_COMMANDS
            COMMAND ${CMAKE_COMMAND}
                "-DMEDIA_DIR=${AEGISUB_MEDIA_RUNTIME_DIR}"
                "-DFFMPEG_EXECUTABLE=${AEGISUB_FFMPEG_EXECUTABLE}"
                "-DASTRA_LIBRARY=$<TARGET_FILE:aegisub_astracore>"
                "-DDUMPBIN=${AEGISUB_DUMPBIN}"
                "-DDESTINATION=$<TARGET_FILE_DIR:AegisubQT>/assets/bin"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/DeployWindowsMedia.cmake")
    else()
        list(APPEND AEGISUB_POST_BUILD_COMMANDS
            COMMAND ${CMAKE_COMMAND} -E make_directory "${_astra_deploy_dir}"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:aegisub_astracore>" "${_astra_deploy_dir}")
    endif()
    install(DIRECTORY third_party/astracore/ DESTINATION share/AegisubQT/source/astracore)
    install(FILES third_party/astracore/LICENSE third_party/astracore/PROVENANCE.md
        DESTINATION share/AegisubQT/licenses/AstraCore)
    list(APPEND AEGISUB_POST_BUILD_COMMANDS
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/third_party/astracore"
            "${AEGISUB_POST_BUILD_DEST}/source/astracore")
elseif(WIN32 AND AEGISUB_MEDIA_RUNTIME_DIR)
    get_filename_component(_msvc_binary_dir "${CMAKE_LINKER}" DIRECTORY)
    find_program(AEGISUB_DUMPBIN dumpbin HINTS "${_msvc_binary_dir}" REQUIRED)
    list(APPEND AEGISUB_POST_BUILD_COMMANDS
        COMMAND ${CMAKE_COMMAND}
            "-DMEDIA_DIR=${AEGISUB_MEDIA_RUNTIME_DIR}"
            "-DFFMPEG_EXECUTABLE=${AEGISUB_FFMPEG_EXECUTABLE}"
            "-DASTRA_LIBRARY=${CMAKE_CURRENT_SOURCE_DIR}/assets/bin/AstraCore.Native.dll"
            "-DDUMPBIN=${AEGISUB_DUMPBIN}"
            "-DDESTINATION=$<TARGET_FILE_DIR:AegisubQT>/assets/bin"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/DeployWindowsMedia.cmake")
elseif(WIN32 AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/assets/bin/AstraCore.Native.dll")
    list(APPEND AEGISUB_POST_BUILD_COMMANDS
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/bin/AstraCore.Native.dll"
            $<TARGET_FILE_DIR:AegisubQT>
    )
endif()

add_custom_command(TARGET AegisubQT POST_BUILD ${AEGISUB_POST_BUILD_COMMANDS})
if(WIN32)
    add_custom_command(TARGET AegisubQT POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${AEGISUB_ICU_RUNTIME_DLLS}
            "$<TARGET_FILE_DIR:AegisubQT>")
    install(FILES ${AEGISUB_ICU_RUNTIME_DLLS} DESTINATION bin)
endif()
add_custom_command(TARGET AegisubQT POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/licenses"
        "${AEGISUB_POST_BUILD_DEST}/licenses")
install(DIRECTORY licenses/ DESTINATION share/AegisubQT/licenses)

# Installation rules (default prefix: /usr/local or C:/Program Files/AegisubQT, overridable via CMAKE_INSTALL_PREFIX).
install(TARGETS AegisubQT
    RUNTIME DESTINATION bin
    BUNDLE DESTINATION .
)

# Linux: resolve Qt libraries both in the build tree and after `cmake --install`
# ($ORIGIN-relative for prefix-local lib dirs, absolute fallback for self-built
# Qt installations outside the default linker search path).
if(UNIX AND NOT APPLE AND QT_INSTALL_LIBS_PATH)
    set_target_properties(AegisubQT PROPERTIES
        BUILD_RPATH "${QT_INSTALL_LIBS_PATH}"
        INSTALL_RPATH "$ORIGIN/../lib;${QT_INSTALL_LIBS_PATH}"
    )
endif()

install(DIRECTORY qml assets automation
    DESTINATION share/AegisubQT
)
install(FILES ${AEGISUB_QM_FILES} DESTINATION share/AegisubQT/locale)
if(WIN32 AND AEGISUB_MEDIA_RUNTIME_DIR AND NOT AEGISUB_BUILD_NATIVE_MEDIA)
    install(DIRECTORY "$<TARGET_FILE_DIR:AegisubQT>/assets/bin/" DESTINATION bin)
elseif(WIN32 AND NOT AEGISUB_BUILD_NATIVE_MEDIA AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/assets/bin/AstraCore.Native.dll")
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/assets/bin/AstraCore.Native.dll" DESTINATION bin)
endif()

# Linux desktop integration: application launcher entry and hicolor icon.
if(UNIX AND NOT APPLE)
    install(FILES packaging/AegisubQT.desktop DESTINATION share/applications)
    install(FILES assets/branding/icon_64.png
        DESTINATION share/icons/hicolor/64x64/apps
        RENAME AegisubQT.png
    )
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/assets/branding/AegisubQT.svg")
        install(FILES assets/branding/AegisubQT.svg
            DESTINATION share/icons/hicolor/scalable/apps
        )
    endif()
endif()

