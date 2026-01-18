# SPDX-License-Identifier: MIT
# SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo" OR
   CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
    set(USE_UNITY_BUILDS ON CACHE BOOL "Use single-TU builds (aka. Unity builds)")
else()
    set(USE_UNITY_BUILDS OFF CACHE BOOL "Use single-TU builds (aka. Unity builds)")
endif()

if(NODE_JS_LINK_DEF)
    set(NODE_JS_LINK_LIB "${CMAKE_CURRENT_BINARY_DIR}/node.lib")
    if (MSVC)
        add_custom_command(OUTPUT node.lib
                           COMMAND ${CMAKE_AR} ${CMAKE_STATIC_LINKER_FLAGS}
                                   /def:${NODE_JS_LINK_DEF} /out:${NODE_JS_LINK_LIB}
                           WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                           MAIN_DEPENDENCY ${NODE_JS_LINK_DEF})
    else()
        add_custom_command(OUTPUT node.lib
                           COMMAND ${CMAKE_DLLTOOL} -d ${NODE_JS_LINK_DEF} -l ${NODE_JS_LINK_LIB}
                           WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                           MAIN_DEPENDENCY ${NODE_JS_LINK_DEF})
    endif()
endif()

function(add_node_addon)
    cmake_parse_arguments(ARG "" "NAME" "SOURCES" ${ARGN})
    add_library(${ARG_NAME} SHARED ${ARG_SOURCES} ${NODE_JS_SOURCES})
    target_link_node(${ARG_NAME})
    set_target_properties(${ARG_NAME} PROPERTIES PREFIX "" SUFFIX ".node")
endfunction()

function(target_link_node TARGET)
    target_include_directories(${TARGET} PRIVATE ${NODE_JS_INCLUDE_DIRS})
    if(NODE_JS_LINK_DEF)
        target_sources(${TARGET} PRIVATE node.lib)
    endif()
    if(NODE_JS_LINK_LIB)
        target_link_libraries(${TARGET} PRIVATE ${NODE_JS_LINK_LIB})
    endif()
    target_compile_options(${TARGET} PRIVATE ${NODE_JS_COMPILE_FLAGS})
    if(NODE_JS_LINK_FLAGS)
        target_link_options(${TARGET} PRIVATE ${NODE_JS_LINK_FLAGS})
    endif()
endfunction()

if(USE_UNITY_BUILDS)
    function(enable_unity_build TARGET)
        cmake_parse_arguments(ARG "" "" "EXCLUDE" ${ARGN})

        get_target_property(sources ${TARGET} SOURCES)
        string(GENEX_STRIP "${sources}" sources)

        set(unity_file_c "${CMAKE_CURRENT_BINARY_DIR}/${TARGET}_unity.c")
        set(unity_file_cpp "${CMAKE_CURRENT_BINARY_DIR}/${TARGET}_unity.cpp")
        file(REMOVE ${unity_file_c} ${unity_file_cpp})

        set(c_definitions "")
        set(cpp_definitions "")

        foreach(src ${sources})
            if (src IN_LIST ARG_EXCLUDE)
                continue()
            endif()

            get_source_file_property(language ${src} LANGUAGE)
            get_property(definitions SOURCE ${src} PROPERTY COMPILE_DEFINITIONS)
            if(IS_ABSOLUTE ${src})
                set(src_full ${src})
            else()
                set(src_full "${CMAKE_CURRENT_SOURCE_DIR}/${src}")
            endif()
            if(language STREQUAL "C")
                set_source_files_properties(${src} PROPERTIES HEADER_FILE_ONLY 1)
                file(APPEND ${unity_file_c} "#include \"${src_full}\"\n")
                if (definitions)
                    set(c_definitions "${c_definitions} ${definitions}")
                endif()
            elseif(language STREQUAL "CXX")
                set_source_files_properties(${src} PROPERTIES HEADER_FILE_ONLY 1)
                file(APPEND ${unity_file_cpp} "#include \"${src_full}\"\n")
                if (definitions)
                    set(cpp_definitions "${cpp_definitions} ${definitions}")
                endif()
            endif()
        endforeach()

        if(EXISTS ${unity_file_c})
            target_sources(${TARGET} PRIVATE ${unity_file_c})
            if(c_definitions)
                set_source_files_properties(${unity_file_c} PROPERTIES COMPILE_DEFINITIONS ${c_definitions})
            endif()
        endif()
        if(EXISTS ${unity_file_cpp})
            target_sources(${TARGET} PRIVATE ${unity_file_cpp})
            if(cpp_definitions)
                set_source_files_properties(${unity_file_cpp} PROPERTIES COMPILE_DEFINITIONS ${cpp_definitions})
            endif()
        endif()

        target_compile_definitions(${TARGET} PRIVATE -DUNITY_BUILD=1)
    endfunction()
else()
    function(enable_unity_build TARGET)
    endfunction()
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "(amd64|x86_64)")
        foreach(lang C CXX)
            set(CMAKE_${lang}_FLAGS_RELEASE "${CMAKE_${lang}_FLAGS_RELEASE} -mpopcnt -msse4.1 -msse4.2 -mssse3 -mcx16")
            set(CMAKE_${lang}_FLAGS_RELWITHDEBINFO "${CMAKE_${lang}_FLAGS_RELWITHDEBINFO} -mpopcnt -msse4.1 -msse4.2 -mssse3 -mcx16")
        endforeach()
    endif()
endif()
