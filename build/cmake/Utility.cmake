# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

if(CMAKE_C_COMPILER_ID MATCHES "[Cc]lang")
    set(CMAKE_COMPILER_IS_CLANG 1)
endif()
if(CMAKE_SYSTEM_NAME STREQUAL Linux)
    set(LINUX 1)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
    set(USE_UNITY_BUILDS ON CACHE BOOL "Use single-TU builds (aka. Unity builds)")
else()
    set(USE_UNITY_BUILDS OFF CACHE BOOL "Use single-TU builds (aka. Unity builds)")
endif()
if(USE_UNITY_BUILDS)
    function(enable_unity_build TARGET)
        get_target_property(sources ${TARGET} SOURCES)
        string(GENEX_STRIP "${sources}" sources)

        set(unity_file_c "${CMAKE_CURRENT_BINARY_DIR}/${TARGET}_unity.c")
        set(unity_file_cpp "${CMAKE_CURRENT_BINARY_DIR}/${TARGET}_unity.cpp")
        file(REMOVE ${unity_file_c} ${unity_file_cpp})

        foreach(src ${sources})
            get_source_file_property(language ${src} LANGUAGE)
            if(IS_ABSOLUTE ${src})
                set(src_full ${src})
            else()
                set(src_full "${CMAKE_CURRENT_SOURCE_DIR}/${src}")
            endif()
            if(language STREQUAL "C")
                set_source_files_properties(${src} PROPERTIES HEADER_FILE_ONLY 1)
                file(APPEND ${unity_file_c} "#include \"${src_full}\"\n")
            elseif(language STREQUAL "CXX")
                set_source_files_properties(${src} PROPERTIES HEADER_FILE_ONLY 1)
                file(APPEND ${unity_file_cpp} "#include \"${src_full}\"\n")
            endif()
        endforeach()

        if(EXISTS ${unity_file_c})
            target_sources(${TARGET} PRIVATE ${unity_file_c})
        endif()
        if(EXISTS ${unity_file_cpp})
            target_sources(${TARGET} PRIVATE ${unity_file_cpp})
        endif()
    endfunction()
else()
    function(enable_unity_build TARGET)
    endfunction()
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
            -DEXCLUDE="${opt_exclude_escaped}"
            -P "${utility_list_dir}/AmalgamateSourceFiles.cmake" "${SRC}" "${DEST}"
        VERBATIM)

    target_sources(${TARGET} PRIVATE "${SRC}")
    set_source_files_properties("${SRC}" PROPERTIES HEADER_FILE_ONLY 1)
endfunction()

set(ADDED_PROJECTS)
function(add_projects PATTERN)
    file(GLOB projects "${PATTERN}")
    foreach(prj ${projects})
        get_filename_component(path "${prj}" REALPATH)
        get_filename_component(name "${prj}" NAME)
        if(EXISTS "${path}/CMakeLists.txt")
            list(FIND ADDED_PROJECTS "${path}" project_idx)
            if (project_idx EQUAL -1)
                string(TOUPPER "BUILD_${name}" build_var)
                set(${build_var} ON CACHE BOOL "")
                if(${build_var})
                    add_subdirectory("${path}")
                    list(APPEND ADDED_PROJECTS "${path}")
                endif()
            endif()
        endif()
    endforeach()
    set(ADDED_PROJECTS "${ADDED_PROJECTS}" PARENT_SCOPE)
endfunction()
