# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# R does not support MSVC, nothing we can do about it
if(NOT R_BINARY AND (CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_CLANG) AND NOT CMAKE_CROSSCOMPILING)
    if(WIN32)
        if(CMAKE_SIZEOF_VOID_P EQUAL 8)
            find_program(R_BINARY R
                         HINTS "[HKEY_LOCAL_MACHINE\\SOFTWARE\\R-core\\R64;InstallPath]\\bin\\x64")
            find_program(R_BINARY_RSCRIPT Rscript
                         HINTS "[HKEY_LOCAL_MACHINE\\SOFTWARE\\R-core\\R64;InstallPath]\\bin\\x64")
        else()
            find_program(R_BINARY R
                         HINTS "[HKEY_LOCAL_MACHINE\\SOFTWARE\\R-core\\R;InstallPath]\\bin\\i386")
            find_program(R_BINARY_RSCRIPT Rscript
                         HINTS "[HKEY_LOCAL_MACHINE\\SOFTWARE\\R-core\\R;InstallPath]\\bin\\i386")
        endif()
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
            if(NOT R_RCPP_INCLUDE_DIRS)
                message(WARNING "Found R but Rcpp is not installed: ignoring R packages")
            endif()
        endif()
    endif()
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(R REQUIRED_VARS R_BINARY R_BINARY_RSCRIPT R_INCLUDE_DIRS R_LIBRARY
                                    VERSION_VAR R_VERSION)
mark_as_advanced(R_BINARY R_BINARY_RSCRIPT R_INCLUDE_DIRS R_LIBRARY R_RCPP_INCLUDE_DIRS)

if(R_FOUND)
    set(install_directory "${CMAKE_BINARY_DIR}/R")

    function(R_add_package TARGET DESCRIPTION NAMESPACE)
        cmake_parse_arguments("OPT" "RCPP_INCLUDE;RCPP_EXPORT" "" "" ${ARGN})
        set(sources ${OPT_UNPARSED_ARGUMENTS})

        if(NOT IS_ABSOLUTE ${DESCRIPTION})
            get_filename_component(DESCRIPTION "${CMAKE_CURRENT_SOURCE_DIR}/${DESCRIPTION}" ABSOLUTE)
        endif()
        if(NOT IS_ABSOLUTE ${NAMESPACE})
            get_filename_component(NAMESPACE "${CMAKE_CURRENT_SOURCE_DIR}/${NAMESPACE}" ABSOLUTE)
        endif()

        set(pkg_directory "${CMAKE_CURRENT_BINARY_DIR}/${TARGET}_R_package")
        file(MAKE_DIRECTORY ${pkg_directory} "${pkg_directory}/src" "${pkg_directory}/R")
        add_custom_command(
            OUTPUT "${pkg_directory}/DESCRIPTION"
            COMMAND ${CMAKE_COMMAND} -E copy "${DESCRIPTION}"
                                             "${pkg_directory}/DESCRIPTION"
            DEPENDS ${DESCRIPTION}
            VERBATIM)
        add_custom_command(
            OUTPUT "${pkg_directory}/NAMESPACE"
            COMMAND ${CMAKE_COMMAND} -E copy "${NAMESPACE}"
                                             "${pkg_directory}/NAMESPACE"
            DEPENDS ${NAMESPACE}
            VERBATIM)

        set(copy_dependencies)
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
            list(APPEND copy_dependencies ${src_dest})
        endforeach()
        add_custom_target(${TARGET}_copy DEPENDS ${copy_dependencies})

        add_library(${TARGET} MODULE ${sources}
                    "${pkg_directory}/DESCRIPTION" "${pkg_directory}/NAMESPACE")
        set_target_properties(${TARGET} PROPERTIES PREFIX ""
                                                   COMPILE_FLAGS "-frtti -fexceptions -fvisibility=default")
        if (CMAKE_COMPILER_IS_GNUCC)
            set_target_properties(${TARGET} PROPERTIES LINK_FLAGS "-fvisibility=default -static-libgcc")
        else()
            set_target_properties(${TARGET} PROPERTIES LINK_FLAGS "-fvisibility=default")
        endif()
        target_include_directories(${TARGET} SYSTEM PRIVATE ${R_INCLUDE_DIRS})
        target_link_libraries(${TARGET} PRIVATE ${R_LIBRARY})
        add_dependencies(${TARGET} ${TARGET}_copy)

        file(MAKE_DIRECTORY "${install_directory}")
        add_custom_command(
            TARGET ${TARGET} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:${TARGET}>"
                                             "${pkg_directory}/src/${TARGET}${CMAKE_SHARED_MODULE_SUFFIX}")

        file(WRITE "${CMAKE_BINARY_DIR}/RunRCmdInstall.cmake" "\
if(WIN32)
    file(WRITE make.bat \"@echo off\")
    set(ENV{PATH} \"\${PKG};\$ENV{PATH}\")
    file(WRITE src/Makefile.win \"all:\\nclean:\\n\")
else()
    file(WRITE src/Makefile \"all:\\nclean:\\n\")
endif()
execute_process(
    COMMAND \"${R_BINARY}\" CMD INSTALL -l \"\${LIB_PATH}\" --no-multiarch --no-test-load .\n\
    WORKING_DIRECTORY \"\${PKG}\"\n\
    OUTPUT_QUIET)\n")
        add_custom_command(
            TARGET ${TARGET} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -DLIB_PATH="${install_directory}"
                                     -DPKG="${pkg_directory}"
                                     -P "${CMAKE_BINARY_DIR}/RunRCmdInstall.cmake"
            WORKING_DIRECTORY ${pkg_directory})

        if(OPT_RCPP_INCLUDE)
            target_include_directories(${TARGET} SYSTEM PRIVATE ${R_RCPP_INCLUDE_DIRS})
        endif()
        if(OPT_RCPP_EXPORT)
            add_custom_command(
                OUTPUT "${pkg_directory}/src/RcppExports.cpp"
                COMMAND ${R_BINARY_RSCRIPT} -e "library(Rcpp);Rcpp::compileAttributes('${pkg_directory}')"
                DEPENDS ${TARGET}_copy
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
set \"PATH=${CMAKE_BINARY_DIR};%PATH%\"\n\
set \"f=%1\"\n\
\"${R_BINARY_RSCRIPT}\" -e \".libPaths(c('${install_directory}',.libPaths()));source('%f:\\=/%')\"\n")
    else()
        file(WRITE "${CMAKE_BINARY_DIR}/Rscript" "\
export \"LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}\"\n\
\"${R_BINARY_RSCRIPT}\" -e \".libPaths(c('${install_directory}',.libPaths()));source('$1')\"\n")
        execute_process(COMMAND chmod +x "${CMAKE_BINARY_DIR}/Rscript")
    endif()
endif()
