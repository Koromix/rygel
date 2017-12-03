# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

if(CMAKE_C_COMPILER_ID MATCHES "[Cc]lang")
    set(CMAKE_COMPILER_IS_CLANG 1)
endif()
if(CMAKE_SYSTEM_NAME STREQUAL Linux)
    set(LINUX 1)
endif()

set(utility_list_dir "${CMAKE_CURRENT_LIST_DIR}")
function(add_amalgamated_file TARGET DEST SRC)
    cmake_parse_arguments("OPT" "" "" "EXCLUDE" ${ARGN})

    if(NOT IS_ABSOLUTE "${DEST}")
        set(DEST "${CMAKE_CURRENT_BINARY_DIR}/${DEST}")
    endif()
    if(NOT IS_ABSOLUTE "${SRC}")
        set(SRC "${CMAKE_CURRENT_SOURCE_DIR}/${SRC}")
    endif()

    # Without that the semicolons are turned into spaces... Fuck CMake.
    string(REPLACE ";" "\\;" opt_exclude_escaped "${OPT_EXCLUDE}")
    add_custom_command(
        TARGET "${TARGET}" POST_BUILD
        COMMAND ${CMAKE_COMMAND}
            -DEXCLUDE="${opt_exclude_escaped}" -P "${utility_list_dir}/AmalgamateSourceFiles.cmake" "${SRC}" "${DEST}")

    target_sources(${TARGET} PRIVATE "${SRC}")
    set_source_files_properties("${SRC}" PROPERTIES HEADER_FILE_ONLY 1)
endfunction()
