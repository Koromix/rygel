if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_CLANG)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wsign-conversion -Wconversion -Wsign-promo \
                                            -Wno-missing-field-initializers -Wno-old-style-cast -Wno-stringop-overflow")
endif()

set(HEIMDALL_SRC heimdall/kutil.cc
                 heimdall/kutil.hh
                 heimdall/main.cc
                 heimdall/main.hh
                 heimdall/opengl.cc
                 heimdall/opengl.hh
                 heimdall/opengl_func.inc
                 heimdall/render.cc
                 heimdall/render.hh)
add_library(heimdall STATIC ${HEIMDALL_SRC})
target_link_libraries(heimdall PUBLIC miniz imgui gdi32 opengl32 imm32)

set(RUNNER_SRC runner/runner_win32.cc
               runner/runner.hh)
add_executable(runner ${RUNNER_SRC})
set_target_properties(runner PROPERTIES OUTPUT_NAME heimdall)
target_link_libraries(runner PUBLIC heimdall)
