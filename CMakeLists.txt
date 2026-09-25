cmake_minimum_required(VERSION 3.20)
project(AegisubQT VERSION 4.0.0 LANGUAGES C CXX)

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
find_package(Qt6 REQUIRED COMPONENTS Core Gui Quick Qml QuickControls2 ShaderTools Multimedia LinguistTools)

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

set(SOURCES
    src/main.cpp
    src/bridge/AstraCoreBridge.h
    src/bridge/AstraCoreBridge.cpp
    src/bridge/AegisubCoreBridge.h
    src/bridge/AegisubCoreBridge.cpp
    src/bridge/RecentFilesManager.h
    src/bridge/RecentFilesManager.cpp
    src/model/SubtitleModel.h
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
    src/video/VisualTools.h
    src/video/VisualTools.cpp
    src/audio/AudioController.h
    src/audio/AudioController.cpp
    src/audio/AudioDisplayController.h
    src/audio/AudioDisplayController.cpp
    src/audio/AudioPcmProvider.h
    src/audio/AudioPcmProvider.cpp
    src/audio/AudioSliceDevice.h
    src/audio/AegisubStftCore.h
    src/audio/AegisubStftCore.cpp
    src/audio/SpectrogramShaderMaterial.h
    src/audio/SpectrogramShaderMaterial.cpp
    src/audio/SpectrogramItem.h
    src/audio/SpectrogramItem.cpp
    src/automation/lpeg/lpeg.c
    src/automation/LuaTextExtents.h
    src/automation/LuaTextExtents.cpp
    src/automation/LuaModules.h
    src/automation/LuaModules.cpp
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

# Compile QRHI shaders at build time.
qt6_add_shaders(AegisubQT "aegisub_shaders"
    PREFIX "/"
    BATCHABLE
    FILES
        shaders/spectrogram.vert
)

qt6_add_shaders(AegisubQT "aegisub_shaders_frag"
    PREFIX "/"
    FILES
        shaders/spectrogram.frag
)

target_include_directories(AegisubQT PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/audio
    ${CMAKE_CURRENT_SOURCE_DIR}/src/video
    ${CMAKE_CURRENT_SOURCE_DIR}/src/bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/src/model
    ${CMAKE_CURRENT_SOURCE_DIR}/src/automation
    ${CMAKE_CURRENT_SOURCE_DIR}/luajit/src
    ${CMAKE_CURRENT_SOURCE_DIR}/libaegisub/include
)

if(QT_INSTALL_QML_PATH)
    target_compile_definitions(AegisubQT PRIVATE QT_INSTALL_QML_PATH="${QT_INSTALL_QML_PATH}")
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

# LuaJIT: Windows uses in-tree prebuilt lua51.lib;
# POSIX platforms build static library from source via LuaJIT makefile.
if(WIN32)
    add_library(luajit STATIC IMPORTED GLOBAL)
    set_target_properties(luajit PROPERTIES
        IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/luajit/src/lua51.lib"
    )
else()
    include(ExternalProject)
    # LuaJIT's Makefile hard-errors on macOS unless MACOSX_DEPLOYMENT_TARGET is
    # exported, so always provide one (11.0 = oldest macOS supported by Qt 6.8).
    if(APPLE)
        if(NOT CMAKE_OSX_DEPLOYMENT_TARGET)
            set(AEGISUB_LUAJIT_DEPLOYMENT_TARGET "11.0")
        else()
            set(AEGISUB_LUAJIT_DEPLOYMENT_TARGET "${CMAKE_OSX_DEPLOYMENT_TARGET}")
        endif()
        set(AEGISUB_LUAJIT_BUILD_COMMAND
            ${CMAKE_COMMAND} -E env "MACOSX_DEPLOYMENT_TARGET=${AEGISUB_LUAJIT_DEPLOYMENT_TARGET}" make -C <SOURCE_DIR>)
    else()
        set(AEGISUB_LUAJIT_BUILD_COMMAND make -C <SOURCE_DIR>)
    endif()
    ExternalProject_Add(luajit_build
        SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/luajit"
        CONFIGURE_COMMAND ""
        BUILD_COMMAND ${AEGISUB_LUAJIT_BUILD_COMMAND}
        BUILD_IN_SOURCE 1
        INSTALL_COMMAND ""
        BUILD_BYPRODUCTS "${CMAKE_CURRENT_SOURCE_DIR}/luajit/src/libluajit.a"
    )
    add_library(luajit STATIC IMPORTED GLOBAL)
    set_target_properties(luajit PROPERTIES
        IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/luajit/src/libluajit.a"
    )
    add_dependencies(luajit luajit_build)
endif()

target_link_libraries(AegisubQT PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Quick
    Qt6::Qml
    Qt6::QuickControls2
    Qt6::Multimedia
    luajit
    libaegisub
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

# Copy runtime assets, QML, automation scripts, and compiled .qm files next to the binary.
# Native DLLs are copied if present (AstraCore.Native is an optional runtime component).
set(AEGISUB_POST_BUILD_COMMANDS
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets"
        $<TARGET_FILE_DIR:AegisubQT>/assets
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/automation"
        $<TARGET_FILE_DIR:AegisubQT>/automation
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/qml"
        $<TARGET_FILE_DIR:AegisubQT>/qml
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:AegisubQT>/locale
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${AEGISUB_QM_FILES}
        $<TARGET_FILE_DIR:AegisubQT>/locale
)

if(WIN32 AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/assets/bin/AstraCore.Native.dll")
    list(APPEND AEGISUB_POST_BUILD_COMMANDS
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/bin/AstraCore.Native.dll"
            $<TARGET_FILE_DIR:AegisubQT>
    )
endif()

add_custom_command(TARGET AegisubQT POST_BUILD ${AEGISUB_POST_BUILD_COMMANDS})

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
if(WIN32 AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/assets/bin/AstraCore.Native.dll")
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/assets/bin/AstraCore.Native.dll" DESTINATION bin)
endif()

# Linux desktop integration: application launcher entry and hicolor icon.
if(UNIX AND NOT APPLE)
    install(FILES packaging/AegisubQT.desktop DESTINATION share/applications)
    install(FILES assets/branding/icon_64.png
        DESTINATION share/icons/hicolor/64x64/apps
        RENAME AegisubQT.png
    )
endif()

