# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# R does not support MSVC, nothing we can do about it
if(NOT R_BINARY AND (CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_CLANG) AND NOT CMAKE_CROSSCOMPILING)
    if(WIN32)
        find_program(R_BINARY R HINTS "[HKEY_LOCAL_MACHINE\\SOFTWARE\\R-core\\R64;InstallPath]\\bin\\x64"
                                      "[HKEY_LOCAL_MACHINE\\SOFTWARE\\R-core\\R;InstallPath]\\bin\\i386")
        find_program(R_BINARY_RSCRIPT Rscript HINTS "[HKEY_LOCAL_MACHINE\\SOFTWARE\\R-core\\R64;InstallPath]\\bin\\x64"
                                                    "[HKEY_LOCAL_MACHINE\\SOFTWARE\\R-core\\R;InstallPath]\\bin\\i386")
        get_filename_component(r_path_root "${R_BINARY_RSCRIPT}/../../.." ABSOLUTE)
        get_filename_component(r_path_bin ${R_BINARY_RSCRIPT} DIRECTORY)
        find_path(R_INCLUDE_DIRS R.h HINTS "${r_path_root}/include")
        find_library(R_LIBRARY R HINTS "${r_path_bin}")
    else()
        find_program(R_BINARY R)
        find_program(R_BINARY_RSCRIPT Rscript)
        find_path(R_INCLUDE_DIRS R.h HINTS /usr/include /usr/local/include
                                     PATH_SUFFIXES R)
        find_library(R_LIBRARY R PATH_SUFFIXES R R/lib)
    endif()

    if(R_BINARY_RSCRIPT)
        execute_process(COMMAND ${R_BINARY_RSCRIPT} --version ERROR_VARIABLE R_VERSION)
        string(REGEX MATCH "[0-9]+\\.[0-9]+\\.[0-9]+" R_VERSION "${R_VERSION}")

        if(NOT R_RCPP_INCLUDE_DIRS)
            execute_process(
                COMMAND ${R_BINARY_RSCRIPT} -e "cat(paste(.libPaths(),collapse=';'))"
                OUTPUT_VARIABLE R_LIB_PATHS)
            find_path(R_RCPP_INCLUDE_DIRS Rcpp.h HINTS ${R_LIB_PATHS}
                                                 PATH_SUFFIXES Rcpp/include)
        endif()
    endif()
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(R REQUIRED_VARS R_BINARY R_BINARY_RSCRIPT R_INCLUDE_DIRS R_LIBRARY
                                    VERSION_VAR R_VERSION)
mark_as_advanced(R_BINARY R_BINARY_RSCRIPT R_INCLUDE_DIRS R_LIBRARY R_RCPP_INCLUDE_DIRS)

if(R_FOUND)
    set(install_directory "${CMAKE_BINARY_DIR}/R")

    if(NOT TARGET R_fake_make)
        if(NOT EXISTS "${CMAKE_BINARY_DIR}/R_fake_make.c")
            file(WRITE "${CMAKE_BINARY_DIR}/R_fake_make.c" "int main() { return 0; }")
        endif()
        add_executable(R_fake_make "${CMAKE_BINARY_DIR}/R_fake_make.c")
        set_target_properties(R_fake_make PROPERTIES OUTPUT_NAME make)
    endif()

    function(R_add_package TARGET DESCRIPTION)
        cmake_parse_arguments("OPT" "RCPP" "" "" ${ARGN})
        set(sources ${OPT_UNPARSED_ARGUMENTS})

        if(NOT IS_ABSOLUTE ${DESCRIPTION})
            get_filename_component(DESCRIPTION "${CMAKE_CURRENT_SOURCE_DIR}/${DESCRIPTION}" ABSOLUTE)
        endif()

        set(pkg_directory "${CMAKE_CURRENT_BINARY_DIR}/${TARGET}_R_package")
        file(MAKE_DIRECTORY ${pkg_directory} "${pkg_directory}/src" "${pkg_directory}/R")
        add_custom_command(
            OUTPUT "${pkg_directory}/DESCRIPTION"
            COMMAND ${CMAKE_COMMAND} -E copy "${DESCRIPTION}"
                                             "${pkg_directory}/DESCRIPTION"
            DEPENDS ${DESCRIPTION}
            VERBATIM)
        file(WRITE "${pkg_directory}/NAMESPACE" "\
useDynLib(${TARGET}, .registration = TRUE)\n\
exportPattern('^[[:alpha:]\\\\._]+')\n")
        if(OPT_RCPP)
            file(APPEND "${pkg_directory}/NAMESPACE" "\
import(Rcpp)\n")
        endif()

        set(rcpp_depends)
        foreach(src ${sources})
            get_filename_component(src_ext ${src} EXT)
            get_filename_component(src_name ${src} NAME)
            get_filename_component(src_path ${src} REALPATH)

            if(src_ext MATCHES "\\.((c(c|pp)?)|(h(h|pp)?))$")
                set(src_dest "${pkg_directory}/src/${src_name}")
            elseif(src_ext STREQUAL ".R")
                set(src_dest "${pkg_directory}/R/${src_name}")
            else()
                continue()
            endif()

            add_custom_command(
                OUTPUT "${src_dest}"
                COMMAND ${CMAKE_COMMAND} -E copy "${src_path}" "${src_dest}"
                DEPENDS "${src_path}"
                VERBATIM)
            list(APPEND rcpp_depends ${src_dest})
        endforeach()

        add_library(${TARGET} SHARED ${sources} "${pkg_directory}/DESCRIPTION")
        set_target_properties(${TARGET} PROPERTIES PREFIX ""
                                                   COMPILE_FLAGS "-frtti -fexceptions"
                                                   LINK_FLAGS "-static-libgcc")
        target_include_directories(${TARGET} SYSTEM PRIVATE ${R_INCLUDE_DIRS})
        target_link_libraries(${TARGET} PRIVATE ${R_LIBRARY})

        file(MAKE_DIRECTORY "${install_directory}")
        add_custom_command(
            TARGET ${TARGET} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:${TARGET}>"
                                             "${pkg_directory}/src/${TARGET}${CMAKE_SHARED_LIBRARY_SUFFIX}")

        file(WRITE "${CMAKE_BINARY_DIR}/RunRCmdInstall.cmake" "\
if(WIN32)\n\
    set(ENV{PATH} \"\${MAKE_PATH};\$ENV{PATH}\")\n\
else()\n\
    set(ENV{PATH} \"\${MAKE_PATH}:\$ENV{PATH}\")\n\
endif()\n\
execute_process(
    COMMAND \"${R_BINARY}\" CMD INSTALL -l \"\${LIB_PATH}\" .\n\
    WORKING_DIRECTORY \"\${PKG}\"\n\
    OUTPUT_QUIET)\n")
        add_custom_command(
            TARGET ${TARGET} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -DMAKE_PATH="$<TARGET_FILE_DIR:R_fake_make>"
                                     -DLIB_PATH="${install_directory}"
                                     -DPKG="${pkg_directory}"
                                     -P "${CMAKE_BINARY_DIR}/RunRCmdInstall.cmake"
            WORKING_DIRECTORY ${pkg_directory})

        if(OPT_RCPP)
            target_include_directories(${TARGET} SYSTEM PRIVATE ${R_RCPP_INCLUDE_DIRS})
            add_custom_command(
                OUTPUT "${pkg_directory}/src/RcppExports.cpp"
                COMMAND ${R_BINARY_RSCRIPT} -e "library(Rcpp);Rcpp::compileAttributes('${pkg_directory}')"
                DEPENDS ${rcpp_depends}
                VERBATIM)
            target_sources(${TARGET} PRIVATE "${pkg_directory}/src/RcppExports.cpp")
            set_source_files_properties("${pkg_directory}/src/RcppExports.cpp"
                                        PROPERTIES COMPILE_FLAGS "-Wno-conversion")
        endif()
    endfunction()

    if(WIN32)
        file(WRITE "${CMAKE_BINARY_DIR}/Rscript.bat" "\
@echo off\n\
setlocal\n\
set \"f=%1\"\n
\"${R_BINARY_RSCRIPT}\" -e \".libPaths(c('${install_directory}',.libPaths()));source('%f:\\=/%')\"\n")
    else()
        file(WRITE "${CMAKE_BINARY_DIR}/Rscript" "\
\"${R_BINARY_RSCRIPT}\" -e \".libPaths(c('${install_directory}',.libPaths()));source('$1')\"\n")
    endif()
endif()
