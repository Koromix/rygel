# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this program. If not, see https://www.gnu.org/licenses/.

if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo" OR
   CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
    set(USE_UNITY_BUILDS ON CACHE BOOL "Use single-TU builds (aka. Unity builds)")
else()
    set(USE_UNITY_BUILDS OFF CACHE BOOL "Use single-TU builds (aka. Unity builds)")
endif()

function(add_node_addon)
    cmake_parse_arguments(ARG "" "NAME" "SOURCES" ${ARGN})

    add_library(${ARG_NAME} SHARED ${ARG_SOURCES} ${NODE_JS_SOURCES})
    set_target_properties(${ARG_NAME} PROPERTIES PREFIX "" SUFFIX ".node")
    target_include_directories(${ARG_NAME} PRIVATE ${NODE_JS_INCLUDE_DIRS})
    target_link_libraries(${ARG_NAME} PRIVATE ${NODE_JS_LIBRARIES})
    target_compile_options(${ARG_NAME} PRIVATE ${NODE_JS_COMPILE_FLAGS})
    if (NODE_JS_LINK_FLAGS)
        target_link_options(${ARG_NAME} PRIVATE ${NODE_JS_LINK_FLAGS})
    endif()
endfunction()

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

        target_compile_definitions(${TARGET} PRIVATE -DUNITY_BUILD=1)
    endfunction()
else()
    function(enable_unity_build TARGET)
    endfunction()
endif()
