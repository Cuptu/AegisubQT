# Called in script mode so runtime dependency scanning occurs after linking.
cmake_minimum_required(VERSION 3.20)
if(POLICY CMP0207)
    cmake_policy(SET CMP0207 NEW)
endif()
if(NOT FFMPEG_EXECUTABLE)
    set(FFMPEG_EXECUTABLE "${MEDIA_DIR}/ffmpeg.exe")
endif()
if(NOT IS_DIRECTORY "${MEDIA_DIR}" OR NOT EXISTS "${ASTRA_LIBRARY}" OR
   NOT EXISTS "${FFMPEG_EXECUTABLE}")
    message(FATAL_ERROR "Astra library and an FFmpeg runtime directory containing ffmpeg.exe are required")
endif()
execute_process(COMMAND "${FFMPEG_EXECUTABLE}" -hide_banner -encoders
    RESULT_VARIABLE ffmpeg_result OUTPUT_VARIABLE encoders ERROR_VARIABLE ffmpeg_error)
if(NOT ffmpeg_result EQUAL 0 OR NOT encoders MATCHES "[ \t]pcm_s16le[ \t]")
    message(FATAL_ERROR "FFmpeg must run and provide pcm_s16le audio encoding. Set AEGISUB_FFMPEG_EXECUTABLE to a complete CLI build. ${ffmpeg_error}")
endif()
set(CMAKE_GET_RUNTIME_DEPENDENCIES_PLATFORM windows+pe)
set(CMAKE_GET_RUNTIME_DEPENDENCIES_TOOL dumpbin)
set(CMAKE_GET_RUNTIME_DEPENDENCIES_COMMAND "${DUMPBIN}")
file(GET_RUNTIME_DEPENDENCIES
    LIBRARIES "${ASTRA_LIBRARY}"
    EXECUTABLES "${FFMPEG_EXECUTABLE}"
    DIRECTORIES "${MEDIA_DIR}"
    RESOLVED_DEPENDENCIES_VAR dependencies
    UNRESOLVED_DEPENDENCIES_VAR missing
    CONFLICTING_DEPENDENCIES_PREFIX conflicts
    PRE_EXCLUDE_REGEXES "^[Aa][Pp][Ii]-[Mm][Ss]-" "^[Ee][Xx][Tt]-[Mm][Ss]-"
    POST_EXCLUDE_REGEXES ".*[/\\][Ss][Yy][Ss][Tt][Ee][Mm]32[/\\].*")
if(missing OR conflicts_FILENAMES)
    message(FATAL_ERROR "Incomplete media dependency closure: ${missing}; conflicts: ${conflicts_FILENAMES}")
endif()
file(MAKE_DIRECTORY "${DESTINATION}")
file(COPY "${ASTRA_LIBRARY}" "${FFMPEG_EXECUTABLE}" ${dependencies}
     DESTINATION "${DESTINATION}")
message(STATUS "Deployed Astra/FFmpeg runtime dependency closure to ${DESTINATION}")
