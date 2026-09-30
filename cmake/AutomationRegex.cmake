# Keep Automation 4's upstream Boost.Regex/ICU semantics, including UTF-8
# offsets, replacement formatting and the exposed Perl syntax flags.
include_guard(GLOBAL)
include(FetchContent)

FetchContent_Declare(aegisub_boost_regex
    URL https://codeload.github.com/boostorg/regex/tar.gz/f439e22ae41fa647bd7de16c2d92deb232593985
    URL_HASH SHA256=7e9fed9aebf522720dc6d4723fd4e8a9fdeb0089e79ce2ffb96762e2a773ff24
    SOURCE_SUBDIR cmake-unused)
FetchContent_MakeAvailable(aegisub_boost_regex)

find_package(ICU COMPONENTS data i18n uc QUIET)
if(NOT ICU_FOUND AND MSVC AND CMAKE_SIZEOF_VOID_P EQUAL 8
        AND NOT CMAKE_SYSTEM_PROCESSOR MATCHES "ARM|arm|aarch"
        AND NOT CMAKE_GENERATOR_PLATFORM MATCHES "ARM|arm"
        AND NOT CMAKE_CXX_COMPILER_ARCHITECTURE_ID MATCHES "ARM|arm")
    FetchContent_Declare(aegisub_icu
        URL https://github.com/unicode-org/icu/releases/download/release-78.3/icu4c-78.3-Win64-MSVC2022.zip
        URL_HASH SHA256=446b671f9437227daa79e221d4521d75793f9ecd65ac44c06e34dd848f201ac2
        SOURCE_SUBDIR cmake-unused)
    FetchContent_MakeAvailable(aegisub_icu)
    set(ICU_ROOT "${aegisub_icu_SOURCE_DIR}")
    # A failed FindICU can still cache individual Windows SDK libraries. They
    # must not be combined with the downloaded ICU headers/data library.
    set(ICU_INCLUDE_DIR "${ICU_ROOT}/include" CACHE PATH "ICU headers" FORCE)
    foreach(_component data i18n uc)
        string(TOUPPER "${_component}" _upper_component)
        if(_component STREQUAL "data")
            set(_library_name icudt)
        elseif(_component STREQUAL "i18n")
            set(_library_name icuin)
        else()
            set(_library_name icuuc)
        endif()
        foreach(_suffix "" _RELEASE _DEBUG)
            unset(ICU_${_upper_component}_LIBRARY${_suffix} CACHE)
            unset(ICU_${_upper_component}_LIBRARY${_suffix})
        endforeach()
        find_library(ICU_${_upper_component}_LIBRARY_RELEASE NAMES ${_library_name}
            PATHS "${ICU_ROOT}/lib64" NO_DEFAULT_PATH REQUIRED)
    endforeach()
endif()
find_package(ICU REQUIRED COMPONENTS data i18n uc)

add_library(aegisub_automation_regex INTERFACE)
target_include_directories(aegisub_automation_regex SYSTEM INTERFACE
    "${aegisub_boost_regex_SOURCE_DIR}/include")
target_compile_definitions(aegisub_automation_regex INTERFACE BOOST_REGEX_STANDALONE)
target_link_libraries(aegisub_automation_regex INTERFACE ICU::data ICU::i18n ICU::uc)

if(WIN32)
    # FindICU exposes import libraries, not DLL locations. Resolve the runtime
    # beside the selected ICU root/libraries and require a complete closure.
    get_filename_component(_icu_libdir "${ICU_UC_LIBRARY_RELEASE}" DIRECTORY)
    file(GLOB AEGISUB_ICU_RUNTIME_DLLS
        "${ICU_ROOT}/bin64/icu*.dll" "${ICU_ROOT}/bin/icu*.dll"
        "${_icu_libdir}/../bin64/icu*.dll" "${_icu_libdir}/../bin/icu*.dll")
    list(FILTER AEGISUB_ICU_RUNTIME_DLLS INCLUDE REGEX "/icu(dt|in|uc)[0-9]+\\.dll$")
    list(REMOVE_DUPLICATES AEGISUB_ICU_RUNTIME_DLLS)
    list(LENGTH AEGISUB_ICU_RUNTIME_DLLS _icu_dll_count)
    if(_icu_dll_count LESS 3)
        message(FATAL_ERROR "ICU data/i18n/uc DLLs are required beside the selected Windows ICU installation")
    endif()
endif()
