# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_CLANG)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wconversion -Wno-strict-overflow")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wconversion -Wno-strict-overflow \
-Wno-class-memaccess -Wno-missing-field-initializers -Wno-old-style-cast -Wno-cast-function-type")
endif()

# ------------------------------------------------------------------------
# libcc
# ------------------------------------------------------------------------

set(LIBCC_SRC
    libcc/libcc.cc
    libcc/libcc.hh
)

add_library(libcc ${LIBCC_SRC})
set_target_properties(libcc PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_link_libraries(libcc PUBLIC miniz)
if(NOT EMSCRIPTEN)
    target_link_libraries(libcc PUBLIC Threads::Threads)
endif()
if(WIN32)
    target_link_libraries(libcc PUBLIC shlwapi)
endif()

# ------------------------------------------------------------------------
# felix
# ------------------------------------------------------------------------

set(FELIX_SRC
    felix/felix/build_command.cc
    felix/felix/build_command.hh
    felix/felix/build_compiler.cc
    felix/felix/build_compiler.hh
    felix/felix/build_target.cc
    felix/felix/build_target.hh
    felix/felix/felix.cc
    felix/felix/felix_build.cc
    felix/felix/felix_pack.cc
    felix/felix/pack_generator_C.cc
    felix/felix/pack_generator_files.cc
    felix/felix/pack_generator.hh
    felix/felix/pack_output.cc
    felix/felix/pack_output.hh
)

add_executable(felix ${FELIX_SRC})
target_link_libraries(felix PRIVATE libcc)

function(target_assets TARGET)
    cmake_parse_arguments("OPT" "" "MODE;MERGE_RULES" "OPTIONS;SOURCES" ${ARGN})

    set(output_src "${CMAKE_CURRENT_BINARY_DIR}/${TARGET}_assets.c")
    set(output_target "${TARGET}_assets")
    set(output_dependencies felix ${OPT_SOURCES})

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

    add_custom_command(
        OUTPUT ${output_src}
        COMMAND $<TARGET_FILE:felix>
        ARGS pack -gC ${OPT_OPTIONS} "-O${output_src}" ${OPT_SOURCES}
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

# ------------------------------------------------------------------------
# libpacker
# ------------------------------------------------------------------------

set(LIBPACKER_SRC
    felix/libpacker/libpacker.cc
    felix/libpacker/libpacker.hh
)

add_library(libpacker STATIC ${LIBPACKER_SRC})
if(UNIX)
    target_link_libraries(libpacker PUBLIC dl)
endif()

# ------------------------------------------------------------------------
# libgui
# ------------------------------------------------------------------------

if(WIN32 OR EMSCRIPTEN)
    set(LIBGUI_SRC
        wrappers/opengl.cc
        wrappers/opengl.hh
        wrappers/opengl_func.inc
        libgui/imgui.cc
        libgui/window_win32.cc
        libgui/window_emsdk.cc
        libgui/window.hh
    )

    add_library(libgui ${LIBGUI_SRC})
    set_target_properties(libgui PROPERTIES POSITION_INDEPENDENT_CODE ON)
    target_link_libraries(libgui PUBLIC libcc imgui)
    if(WIN32)
        target_link_libraries(libgui PUBLIC gdi32 opengl32 imm32)
    endif()
    add_amalgamated_file(libgui "${CMAKE_BINARY_DIR}/libgui.cc" libgui/libgui.cc)
    add_amalgamated_file(libgui "${CMAKE_BINARY_DIR}/libgui.hh" libgui/libgui.hh)
endif()

# ------------------------------------------------------------------------
# libdrd
# ------------------------------------------------------------------------

set(LIBDRD_SRC
    drd/libdrd/common.hh
    drd/libdrd/mco_authorization.cc
    drd/libdrd/mco_authorization.hh
    drd/libdrd/mco_classifier.cc
    drd/libdrd/mco_classifier.hh
    drd/libdrd/mco_common.hh
    drd/libdrd/mco_dump.cc
    drd/libdrd/mco_dump.hh
    drd/libdrd/mco_filter.cc
    drd/libdrd/mco_filter.hh
    drd/libdrd/mco_mapper.cc
    drd/libdrd/mco_mapper.hh
    drd/libdrd/mco_pricing.cc
    drd/libdrd/mco_pricing.hh
    drd/libdrd/mco_stay.cc
    drd/libdrd/mco_stay.hh
    drd/libdrd/mco_tables.cc
    drd/libdrd/mco_tables.hh
)

add_library(libdrd ${LIBDRD_SRC})
set_target_properties(libdrd PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_link_libraries(libdrd PUBLIC libcc wren)
add_amalgamated_file(libdrd "${CMAKE_BINARY_DIR}/libdrd.cc" drd/libdrd/libdrd.cc)
add_amalgamated_file(libdrd "${CMAKE_BINARY_DIR}/libdrd.hh" drd/libdrd/libdrd.hh)

# ------------------------------------------------------------------------
# drdc
# ------------------------------------------------------------------------

set(DRDC_SRC
    drd/drdc/config.cc
    drd/drdc/config.hh
    drd/drdc/drdc.cc
    drd/drdc/drdc_mco.cc
)

add_executable(drdc ${DRDC_SRC})
target_link_libraries(drdc PRIVATE libcc libdrd)

# ------------------------------------------------------------------------
# drdR
# ------------------------------------------------------------------------

find_package(R)
if(R_FOUND)
    set(DRDR_SRC
        wrappers/Rcc.cc
        wrappers/Rcc.hh
        drd/drdR/drdR_mco.cc
        drd/drdR/drdR_mco.R
    )

    R_add_package(drdR drd/drdR/DESCRIPTION drd/drdR/NAMESPACE
                  RCPP_INCLUDE ${DRDR_SRC})
    target_link_libraries(drdR PRIVATE libcc libdrd miniz)
endif()

# ------------------------------------------------------------------------
# THOP
# ------------------------------------------------------------------------

if(NOT EMSCRIPTEN)
    set(THOP_SRC
        wrappers/http.cc
        wrappers/http.hh
        wrappers/json.hh
        thop/server/config.cc
        thop/server/config.hh
        thop/server/mco.cc
        thop/server/mco.hh
        thop/server/mco_casemix.cc
        thop/server/mco_casemix.hh
        thop/server/mco_info.cc
        thop/server/mco_info.hh
        thop/server/structure.cc
        thop/server/structure.hh
        thop/server/thop.cc
        thop/server/thop.hh
        thop/server/user.cc
        thop/server/user.hh
    )
    set(THOP_ASSETS
        thop/client/thop.html
        libweb/compat.js
        libweb/util.js
        thop/client/common.js
        thop/client/thop.js
        thop/client/user.js
        thop/client/mco_casemix.js
        thop/client/mco_list.js
        thop/client/mco_pricing.js
        thop/client/mco_tree.js
        libweb/data_table.js
        libweb/pager.js
        libweb/period_picker.js
        libweb/tree_selector.js
        libweb/version_line.js
        ../lib/chartjs/chart.min.js
        ../lib/sheetjs/xlsx.core.min.js
        thop/client/thop.css
        thop/client/mco_casemix.css
        thop/client/mco_list.css
        thop/client/mco_pricing.css
        thop/client/mco_tree.css
        libweb/data_table.css
        libweb/pager.css
        libweb/period_picker.css
        libweb/tree_selector.css
        libweb/version_line.css
        thop/client/images/thop_logo.svg
        thop/client/images/favicon.png
    )

    set(THOP_HASH_PASSWORD_SRC
        thop/server/thop_hash_password.c
    )

    add_executable(thop ${THOP_SRC})
    target_include_directories(thop PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
    target_link_libraries(thop PRIVATE libcc libdrd libpacker microhttpd sodium)
    target_assets(thop
        MODE ModuleIfDebug
        MERGE_RULES thop/client/packer.ini
        OPTIONS -cGzip $<$<CONFIG:Debug>:--source_map>
        SOURCES ${THOP_ASSETS}
    )

    add_executable(thop_hash_password ${THOP_HASH_PASSWORD_SRC})
    target_link_libraries(thop_hash_password PRIVATE sodium)
endif()

# ------------------------------------------------------------------------
# libheimdall
# ------------------------------------------------------------------------

if(TARGET libgui)
    set(LIBHEIMDALL_SRC
        wrappers/opengl.cc
        wrappers/opengl.hh
        wrappers/opengl_func.inc
        heimdall/libheimdall/animation.hh
        heimdall/libheimdall/core.cc
        heimdall/libheimdall/core.hh
        heimdall/libheimdall/data.hh
    )

    add_library(libheimdall ${LIBHEIMDALL_SRC})
    set_target_properties(libheimdall PROPERTIES POSITION_INDEPENDENT_CODE ON)
    target_link_libraries(libheimdall PUBLIC libcc libgui)
    add_amalgamated_file(libheimdall "${CMAKE_BINARY_DIR}/libheimdall.cc" heimdall/libheimdall/libheimdall.cc)
    add_amalgamated_file(libheimdall "${CMAKE_BINARY_DIR}/libheimdall.hh" heimdall/libheimdall/libheimdall.hh)
endif()

# ------------------------------------------------------------------------
# heimdallR
# ------------------------------------------------------------------------

find_package(R)
if(TARGET libheimdall AND R_FOUND)
    set(HEIMDALLR_SRC
        wrappers/Rcc.cc
        wrappers/Rcc.hh
        heimdall/heimdallR/heimdallR.cc
        heimdall/heimdallR/heimdallR.R
    )

    R_add_package(heimdallR heimdall/heimdallR/DESCRIPTION heimdall/heimdallR/NAMESPACE
                  RCPP_EXPORT RCPP_INCLUDE ${HEIMDALLR_SRC})
    target_link_libraries(heimdallR PRIVATE libcc libheimdall miniz)
endif()

# ------------------------------------------------------------------------
# heimdallw
# ------------------------------------------------------------------------

if(TARGET libheimdall)
    if(EMSCRIPTEN)
        set(CMAKE_EXECUTABLE_SUFFIX .html)
    endif()

    set(HEIMDALLW_SRC
        heimdall/heimdallw/heimdallw.cc
    )

    add_executable(heimdallw ${HEIMDALLW_SRC})
    target_link_libraries(heimdallw PRIVATE libcc libheimdall)
endif()

# ------------------------------------------------------------------------
# goupil
# ------------------------------------------------------------------------

set(GOUPIL_SRC
    wrappers/http.cc
    wrappers/http.hh
    goupil/server/config.cc
    goupil/server/config.hh
    goupil/server/goupil.cc
)
set(GOUPIL_ASSETS
    goupil/client/goupil.html
    libweb/compat.js
    libweb/util.js
    goupil/client/store.js
    goupil/client/widgets.js
    goupil/client/widgets.css
)

set(GOUPIL_ADMIN_SRC
    goupil/admin/goupil_admin.cc
    goupil/admin/profile.cc
    goupil/admin/profile.hh
    goupil/server/config.cc
    goupil/server/config.hh
    goupil/server/data_sqlite.cc
    goupil/server/data.hh
)
# These are the files used by create_profile, only put in files
# under goupil/admin/profile and avoid '..' ("dotdot") components!
set(GOUPIL_ADMIN_ASSETS
    # Nothing yet
)

add_executable(goupil ${GOUPIL_SRC})
target_link_libraries(goupil PRIVATE libcc libpacker microhttpd sqlite)
target_assets(goupil
    MODE ModuleIfDebug
    MERGE_RULES goupil/client/packer.ini
    OPTIONS -cGzip $<$<CONFIG:Debug>:--source_map>
    SOURCES ${GOUPIL_ASSETS}
)

add_executable(goupil_admin ${GOUPIL_ADMIN_SRC})
target_link_libraries(goupil_admin PRIVATE libcc sqlite)
target_assets(goupil_admin
    OPTIONS -cGzip -s3
    SOURCES ${GOUPIL_ADMIN_ASSETS}
)

# ------------------------------------------------------------------------
# inPL
# ------------------------------------------------------------------------

set(INPL_ASSETS
    inPL/inPL.html
    inPL/inPL_logo.png
    ../lib/papaparse/papaparse.min.js
    ../lib/docxtemplater/jszip.min.js
    ../lib/docxtemplater/docxtemplater.min.js
    libweb/compat.js
    libweb/util.js
    inPL/common.js
    inPL/inPL.js
    inPL/mod_ems.js
    inPL/mod_neuropsy.js
    inPL/mod_nutrition.js
    inPL/bridge.js
    inPL/report.js
    inPL/tables.js
    inPL/inPL.css
)

add_custom_target(inPL ALL
    DEPENDS "${CMAKE_BINARY_DIR}/inPL/inPL.html"
)
add_custom_command(
    OUTPUT "${CMAKE_BINARY_DIR}/inPL/inPL.html"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/inPL"
    COMMAND $<TARGET_FILE:felix> pack -M inPL/packer.ini -gFiles "-O${CMAKE_BINARY_DIR}/inPL" ${INPL_ASSETS}
    COMMAND $<TARGET_FILE:felix> pack -M inPL/packer.ini -gFiles -cGzip "-O${CMAKE_BINARY_DIR}/inPL" ${INPL_ASSETS}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    DEPENDS felix ${INPL_ASSETS}
)

# ------------------------------------------------------------------------
# simPL
# ------------------------------------------------------------------------

if(TARGET libgui)
    set(SIMPL_SRC
        simPL/simPL.cc
        simPL/view.cc
        simPL/view.hh
    )
    set(SIMPL_ASSETS
        ../lib/imgui/misc/fonts/Roboto-Medium.ttf
    )

    set(SIMPL_MODULE_SRC
        wrappers/pcg.hh
        simPL/economics.cc
        simPL/economics.hh
        simPL/predict_cancer.cc
        simPL/predict_cv.cc
        simPL/predict_cv_qrisk3.cc
        simPL/predict.hh
        simPL/simulate.cc
        simPL/simulate.hh
        simPL/tables_cepidc.cc
        simPL/tables_smoking.cc
        simPL/tables.hh
    )

    add_executable(simPL ${SIMPL_SRC})
    target_link_libraries(simPL PRIVATE libcc libgui)
    target_assets(simPL
        SOURCES ${SIMPL_ASSETS}
    )

    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        # The copy is mostly for Windows, where incomplete DLLs can generate
        # annoying MessageBox errors.
        add_library(simPL_module MODULE ${SIMPL_MODULE_SRC})
        target_link_libraries(simPL_module PRIVATE libcc pcg)
        add_custom_command(
            TARGET simPL_module POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:simPL_module>
                                             "$<TARGET_FILE_DIR:simPL_module>/simPL${CMAKE_SHARED_LIBRARY_SUFFIX}"
        )

        target_compile_definitions(simPL PRIVATE SIMPL_ENABLE_HOT_RELOAD)
        target_compile_definitions(simPL_module PRIVATE SIMPL_ENABLE_HOT_RELOAD)
    else()
        target_sources(simPL PRIVATE ${SIMPL_MODULE_SRC})
        target_link_libraries(simPL PRIVATE pcg)
    endif()
endif()
