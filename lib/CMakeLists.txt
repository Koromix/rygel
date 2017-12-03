set(IMGUI_SRC imgui/imgui.cpp
              imgui/imgui_draw.cpp
              imgui/imgui_demo.cpp)
add_library(imgui STATIC EXCLUDE_FROM_ALL ${IMGUI_SRC})

set(MINIZ_SRC miniz/miniz.c)
add_library(miniz STATIC EXCLUDE_FROM_ALL ${MINIZ_SRC})
