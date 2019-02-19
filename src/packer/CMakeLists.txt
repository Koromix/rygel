# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

add_executable(packer
    ../wrappers/json.hh
    packer.cc
    packer.hh
    output.cc
    output.hh
)

target_link_libraries(packer PRIVATE libcc)
if(WIN32)
    target_link_libraries(packer PRIVATE shlwapi)
endif()

add_library(libasset STATIC
    libasset.cc
    libasset.hh
)
if(UNIX)
    target_link_libraries(libpacker PUBLIC dl)
endif()

function(target_assets TARGET)
    cmake_parse_arguments("OPT" "" "MODE;MERGE_RULES" "OPTIONS;SOURCES" ${ARGN})

    set(output_src "${CMAKE_CURRENT_BINARY_DIR}/${TARGET}_assets.cc")
    set(output_target "${TARGET}_assets")
    set(output_dependencies packer ${OPT_SOURCES})

    if(OPT_MERGE_RULES)
        list(APPEND OPT_OPTIONS "-M${OPT_MERGE_RULES}")
        list(APPEND output_dependencies ${OPT_MERGE_RULES})
    endif()

    if(OPT_MODE STREQUAL "ModuleIfDebug")
        if(CMAKE_BUILD_TYPE STREQUAL "Debug")
            set(OPT_MODE "Module")
        else()
            set(OPT_MODE "Static")
        endif()
    endif()
    if(OPT_MODE STREQUAL "Module")
        set(OPT_OPTIONS "${OPT_OPTIONS} -e")
    endif()

    add_custom_command(
        OUTPUT ${output_src}
        COMMAND $<TARGET_FILE:packer>
        ARGS ${OPT_OPTIONS} "-O${output_src}" ${OPT_SOURCES}
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        DEPENDS ${output_dependencies}
    )

    if(NOT OPT_MODE OR OPT_MODE STREQUAL "Static")
        target_sources(${TARGET} PRIVATE ${output_src})
    elseif(OPT_MODE STREQUAL "Module")
        add_library(${output_target} MODULE ${output_src})
        set_target_properties(${output_target} PROPERTIES PREFIX "")
        add_dependencies(${TARGET} ${output_target})
    else()
        message(FATAL_ERROR "Option MODE must be 'Static' (default), 'Module' or 'ModuleIfDebug'")
    endif()
endfunction()
