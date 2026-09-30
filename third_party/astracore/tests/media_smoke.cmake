if(NOT FFMPEG OR NOT PROBE OR NOT VIDEO OR NOT WORK_DIR)
    message(FATAL_ERROR "FFMPEG, PROBE, VIDEO and WORK_DIR are required")
endif()
file(MAKE_DIRECTORY "${WORK_DIR}")
set(media "${WORK_DIR}/native-media.mkv")
set(color_media "${WORK_DIR}/native-color-metadata.mkv")
execute_process(COMMAND "${FFMPEG}" -nostdin -v error -y
    -f lavfi -i "testsrc2=size=96x64:rate=10:duration=1"
    -f lavfi -i "sine=frequency=440:sample_rate=16000:duration=1"
    -c:v ffv1 -c:a pcm_s16le -shortest "${media}"
    RESULT_VARIABLE generated OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(NOT generated EQUAL 0)
    message(FATAL_ERROR "Media fixture generation failed (${generated}): ${error}")
endif()
execute_process(COMMAND "${FFMPEG}" -nostdin -v error -y
    -f lavfi -i "testsrc2=size=96x64:rate=10:duration=1"
    -vf "setparams=colorspace=bt709:range=tv"
    -c:v ffv1 "${color_media}"
    RESULT_VARIABLE generated OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(NOT generated EQUAL 0)
    message(FATAL_ERROR "Color metadata fixture generation failed (${generated}): ${error}")
endif()
execute_process(COMMAND "${PROBE}" "${media}" WORKING_DIRECTORY "${WORK_DIR}"
    RESULT_VARIABLE tested OUTPUT_VARIABLE output ERROR_VARIABLE error)
message(STATUS "${output}")
if(NOT tested EQUAL 0)
    message(FATAL_ERROR "Real media probe/audio extraction failed (${tested}): ${error}")
endif()
execute_process(COMMAND "${VIDEO}" "${media}" "${color_media}" WORKING_DIRECTORY "${WORK_DIR}"
    RESULT_VARIABLE tested OUTPUT_VARIABLE output ERROR_VARIABLE error)
message(STATUS "${output}")
if(NOT tested EQUAL 0)
    message(FATAL_ERROR "Real video decode failed (${tested}): ${error}")
endif()
execute_process(COMMAND "${LOADER}" "${NATIVE}" "${media}"
    RESULT_VARIABLE tested OUTPUT_VARIABLE output ERROR_VARIABLE error)
message(STATUS "${output}")
if(NOT tested EQUAL 0)
    message(FATAL_ERROR "Native dynamic loading failed (${tested}): ${error}")
endif()
