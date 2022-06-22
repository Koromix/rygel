# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU Affero General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Affero General Public License for more details.
#
# You should have received a copy of the GNU Affero General Public License
# along with this program. If not, see https://www.gnu.org/licenses/.

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
