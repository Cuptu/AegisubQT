cmake_minimum_required(VERSION 3.20)
if(NOT APPLE)
    message(FATAL_ERROR "DeployMacMedia must run on macOS")
endif()
if(NOT BUNDLE OR NOT FFMPEG_EXECUTABLE OR NOT EXISTS "${FFMPEG_EXECUTABLE}")
    message(FATAL_ERROR "An existing BUNDLE and FFMPEG_EXECUTABLE are required")
endif()
get_filename_component(BUNDLE "${BUNDLE}" ABSOLUTE)
set(native "${BUNDLE}/Contents/Frameworks/libAstraCore.Native.dylib")
if(NOT EXISTS "${native}" OR NOT EXISTS "${BUNDLE}/Contents/MacOS/AegisubQT")
    message(FATAL_ERROR "Bundle is missing AegisubQT or the real native media library")
endif()
# The CLI is used by AudioPcmProvider. Its dependencies are part of this closure.
# Homebrew's bin/ffmpeg is a symlink into the Cellar. Copy the actual binary,
# so the bundle never retains a link back to the build machine.
file(REAL_PATH "${FFMPEG_EXECUTABLE}" ffmpeg_real)
file(COPY "${ffmpeg_real}" DESTINATION "${BUNDLE}/Contents/MacOS")
if(NOT EXISTS "${BUNDLE}/Contents/MacOS/ffmpeg")
    message(FATAL_ERROR "FFMPEG_EXECUTABLE must be named ffmpeg")
endif()

# AstraCore is loaded dynamically: explicitly seed it, rather than relying on
# the application's link graph. This copies recursive third-party dependencies,
# repairs Mach-O install names, and fails if verify_app finds external deps.
include(BundleUtilities)
fixup_bundle("${BUNDLE}" "${native}" "${MEDIA_LIBRARY_DIRS}")
message(STATUS "Native media dependency closure deployed and verified; re-sign the bundle after all fixups")
