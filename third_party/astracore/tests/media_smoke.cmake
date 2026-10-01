if(NOT FFMPEG OR NOT PROBE OR NOT VIDEO OR NOT LOADER OR NOT NATIVE OR NOT PYTHON OR NOT WORK_DIR)
    message(FATAL_ERROR "FFMPEG, PROBE, VIDEO, LOADER, NATIVE, PYTHON and WORK_DIR are required")
endif()
file(MAKE_DIRECTORY "${WORK_DIR}")
set(media "${WORK_DIR}/native-media.mkv")
set(color_media "${WORK_DIR}/native-color-metadata.mkv")
set(delayed_media "${WORK_DIR}/native-bframes.mkv")
execute_process(COMMAND "${PYTHON}" "${CMAKE_CURRENT_LIST_DIR}/../scripts/make-test-inputs.py" "${WORK_DIR}"
    RESULT_VARIABLE generated ERROR_VARIABLE error)
if(NOT generated EQUAL 0)
    message(FATAL_ERROR "PNG/WAV fixture generation failed: ${error}")
endif()
execute_process(COMMAND "${FFMPEG}" -nostdin -v error -y
    -framerate 10 -i "${WORK_DIR}/frame%02d.png" -i "${WORK_DIR}/tone.wav"
    -c:v libx264 -bf 0 -qp 0 -pix_fmt yuv420p -c:a pcm_s16le -shortest "${media}"
    RESULT_VARIABLE generated OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(NOT generated EQUAL 0)
    message(FATAL_ERROR "Media fixture generation failed (${generated}): ${error}")
endif()
execute_process(COMMAND "${FFMPEG}" -nostdin -v error -y
    -framerate 10 -i "${WORK_DIR}/frame%02d.png"
    -colorspace bt709 -color_range tv
    -c:v libx264 -bf 0 -qp 0 -pix_fmt yuv420p "${color_media}"
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
execute_process(COMMAND "${FFMPEG}" -nostdin -v error -y
    -framerate 10 -i "${WORK_DIR}/frame%02d.png" -c:v libx264 -bf 3 -crf 18 -pix_fmt yuv420p "${delayed_media}"
    RESULT_VARIABLE generated ERROR_VARIABLE error)
if(NOT generated EQUAL 0)
    message(FATAL_ERROR "B-frame fixture generation failed: ${error}")
endif()
execute_process(COMMAND "${VIDEO}" "${media}" "${color_media}" "${delayed_media}" WORKING_DIRECTORY "${WORK_DIR}"
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
