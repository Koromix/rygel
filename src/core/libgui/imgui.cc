// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.

#include "src/core/libcc/libcc.hh"
#include "window.hh"
#include "src/core/libwrap/opengl.hh"
RG_PUSH_NO_WARNINGS
#define IMGUI_DISABLE_OBSOLETE_FUNCTIONS
#include "vendor/imgui/imgui.h"
#include "vendor/imgui/imgui_internal.h"
RG_POP_NO_WARNINGS

namespace RG {

extern "C" const AssetInfo RobotoMediumTtf;

static const char *const ImGuiVertexCode =
#ifdef __EMSCRIPTEN__
R"!(#version 300 es

    precision highp float;
)!"
#else
R"!(#version 330 core
)!"
#endif
R"!(uniform mat4 ProjMtx;
    in vec2 Position;
    in vec2 UV;
    in vec4 Color;
    out vec2 Frag_UV;
    out vec4 Frag_Color;

    void main()
    {
        Frag_UV = UV;
        Frag_Color = Color;
        gl_Position = ProjMtx * vec4(Position.xy, 0, 1);
    }
)!";

static const char *const ImGuiFragmentCode =
#ifdef __EMSCRIPTEN__
R"!(#version 300 es

    precision mediump float;
)!"
#else
R"!(#version 330 core
)!"
#endif
R"!(uniform sampler2D Texture;
    in vec2 Frag_UV;
    in vec4 Frag_Color;
    out vec4 Out_Color;

    void main()
    {
        Out_Color = Frag_Color * texture(Texture, Frag_UV.st);
    }
)!";

static GLuint shader_program = 0;
static GLint attrib_proj_mtx;
static GLint attrib_texture;
static GLuint attrib_position;
static GLuint attrib_uv;
static GLuint attrib_color;
static GLuint array_buffer = 0;
static GLuint elements_buffer = 0;
static GLuint vao = 0;
static GLuint font_texture = 0;

static ImFontAtlas default_atlas;

bool gui_Window::imgui_ready = false;

bool gui_Window::InitImGui(ImFontAtlas *font_atlas)
{
    RG_ASSERT(!imgui_ready);

    if (!font_atlas) {
        static std::once_flag flag;

        std::call_once(flag, []() {
            const AssetInfo &font = RobotoMediumTtf;
            RG_ASSERT(font.data.len <= INT_MAX);

            ImFontConfig font_config;
            font_config.FontDataOwnedByAtlas = false;

            default_atlas.AddFontFromMemoryTTF((void *)font.data.ptr, (int)font.data.len, 16, &font_config);
        });

        font_atlas = &default_atlas;
    }

    ImGui::CreateContext(font_atlas);
    RG_DEFER_N(imgui_guard) { ReleaseImGui(); };

    ImGuiIO *io = &ImGui::GetIO();
    io->IniFilename = nullptr;

    // Build shaders
    {
        GLuint new_shader = ogl_BuildShader("imgui", ImGuiVertexCode, ImGuiFragmentCode);
        if (new_shader) {
            if (shader_program) {
                glDeleteProgram(shader_program);
            }
            shader_program = new_shader;
        } else if (!shader_program) {
            return false;
        }

        attrib_proj_mtx = glGetUniformLocation(shader_program, "ProjMtx");
        attrib_texture = glGetUniformLocation(shader_program, "Texture");
        attrib_position = (GLuint)glGetAttribLocation(shader_program, "Position");
        attrib_uv = (GLuint)glGetAttribLocation(shader_program, "UV");
        attrib_color = (GLuint)glGetAttribLocation(shader_program, "Color");
    }

    if (!array_buffer) {
        glGenBuffers(1, &array_buffer);
        glGenBuffers(1, &elements_buffer);
        glGenVertexArrays(1, &vao);
    }
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, array_buffer);
    glEnableVertexAttribArray(attrib_position);
    glEnableVertexAttribArray(attrib_uv);
    glEnableVertexAttribArray(attrib_color);
    glVertexAttribPointer(attrib_position, 2, GL_FLOAT, GL_FALSE, RG_SIZE(ImDrawVert),
                          (GLvoid *)RG_OFFSET_OF(ImDrawVert, pos));
    glVertexAttribPointer(attrib_uv, 2, GL_FLOAT, GL_FALSE, RG_SIZE(ImDrawVert),
                          (GLvoid *)RG_OFFSET_OF(ImDrawVert, uv));
    glVertexAttribPointer(attrib_color, 4, GL_UNSIGNED_BYTE, GL_TRUE, RG_SIZE(ImDrawVert),
                          (GLvoid *)RG_OFFSET_OF(ImDrawVert, col));

    if (!font_texture) {
        uint8_t *pixels;
        int width, height;
        // XXX: Switch to GetTexDataAsAlpha8() eventually
        io->Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

        glGenTextures(1, &font_texture);
        glBindTexture(GL_TEXTURE_2D, font_texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, pixels);
        io->Fonts->TexID = (void *)(intptr_t)font_texture;
    }

    io->BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;

    io->KeyMap[ImGuiKey_Tab] = (int)gui_InputKey::Tab;
    io->KeyMap[ImGuiKey_Delete] = (int)gui_InputKey::Delete;
    io->KeyMap[ImGuiKey_Backspace] = (int)gui_InputKey::Backspace;
    io->KeyMap[ImGuiKey_Enter] = (int)gui_InputKey::Enter;
    io->KeyMap[ImGuiKey_Escape] = (int)gui_InputKey::Escape;
    io->KeyMap[ImGuiKey_Home] = (int)gui_InputKey::Home;
    io->KeyMap[ImGuiKey_End] = (int)gui_InputKey::End;
    io->KeyMap[ImGuiKey_PageUp] = (int)gui_InputKey::PageUp;
    io->KeyMap[ImGuiKey_PageDown] = (int)gui_InputKey::PageDown;
    io->KeyMap[ImGuiKey_LeftArrow] = (int)gui_InputKey::Left;
    io->KeyMap[ImGuiKey_RightArrow] = (int)gui_InputKey::Right;
    io->KeyMap[ImGuiKey_UpArrow] = (int)gui_InputKey::Up;
    io->KeyMap[ImGuiKey_DownArrow] = (int)gui_InputKey::Down;
    io->KeyMap[ImGuiKey_A] = (int)gui_InputKey::A;
    io->KeyMap[ImGuiKey_C] = (int)gui_InputKey::C;
    io->KeyMap[ImGuiKey_V] = (int)gui_InputKey::V;
    io->KeyMap[ImGuiKey_X] = (int)gui_InputKey::X;
    io->KeyMap[ImGuiKey_Y] = (int)gui_InputKey::Y;
    io->KeyMap[ImGuiKey_Z] = (int)gui_InputKey::Z;

    imgui_local = true;
    imgui_ready = true;

    imgui_guard.Disable();
    return true;
}

void gui_Window::StartImGuiFrame()
{
    ImGuiIO *io = &ImGui::GetIO();

    io->DisplaySize = ImVec2((float)state.display.width, (float)state.display.height);
    io->DeltaTime = (float)state.time.monotonic_delta;

    memset_safe(io->KeysDown, 0, RG_SIZE(io->KeysDown));
    for (Size idx: state.input.keys) {
        io->KeysDown[idx] = true;
    }
    io->KeyCtrl = state.input.keys.Test((Size)gui_InputKey::Control);
    io->KeyAlt = state.input.keys.Test((Size)gui_InputKey::Alt);
    io->KeyShift = state.input.keys.Test((Size)gui_InputKey::Shift);
    io->AddInputCharactersUTF8(state.input.text.data);

    io->MousePos = ImVec2((float)state.input.x, (float)state.input.y);
    for (Size i = 0; i < RG_LEN(io->MouseDown); i++) {
        io->MouseDown[i] = state.input.buttons & (unsigned int)(1 << i);
    }
    io->MouseWheel = (float)state.input.wheel_y;

    ImGui::NewFrame();
}

void gui_Window::ReleaseImGui()
{
    if (imgui_local) {
        ImGui::DestroyContext();

        if (font_texture) {
            glDeleteTextures(1, &font_texture);
            font_texture = 0;
        }
        if (vao) {
            glDeleteVertexArrays(1, &vao);
            vao = 0;
        }
        if (elements_buffer) {
            glDeleteBuffers(1, &elements_buffer);
            elements_buffer = 0;
        }
        if (array_buffer) {
            glDeleteBuffers(1, &array_buffer);
            array_buffer = 0;
        }
        if(shader_program) {
            glDeleteProgram(shader_program);
            shader_program = 0;
        }
    }

    imgui_local = false;
    imgui_ready = false;
}

void gui_Window::RenderImGui()
{
    RG_ASSERT(imgui_local);

    // Clear screen
    glViewport(0, 0, state.display.width, state.display.height);
    glDisable(GL_SCISSOR_TEST);
    glClearColor(0.14f, 0.14f, 0.14f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Configure OpenGL
    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_SCISSOR_TEST);
    glUseProgram(shader_program);
    glBindVertexArray(vao);
    glActiveTexture(GL_TEXTURE0);
    glUniform1i(attrib_texture, 0);

    // Set up orthographic projection matrix
    {
        ImGuiIO *io = &ImGui::GetIO();

        float width = io->DisplaySize.x, height = io->DisplaySize.y;
        const float proj_mtx[4][4] = {
            { 2.0f / width, 0.0f,          0.0f, 0.0f },
            { 0.0f,        -2.0f / height, 0.0f, 0.0f },
            { 0.0f,         0.0f,         -1.0f, 0.0f },
            {-1.0f,         1.0f,          0.0f, 1.0f }
        };
        glUniformMatrix4fv(attrib_proj_mtx, 1, GL_FALSE, &proj_mtx[0][0]);
    }

    // Render ImGui
    {
        ImGui::Render();

        ImDrawData *draw_data = ImGui::GetDrawData();

        // ImGui draw calls
        for (int i = 0; i < draw_data->CmdListsCount; i++) {
            const ImDrawList *cmds = draw_data->CmdLists[i];
            const ImDrawIdx *idx_buffer_offset = nullptr;

            glBindBuffer(GL_ARRAY_BUFFER, array_buffer);
            glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)cmds->VtxBuffer.Size * RG_SIZE(ImDrawVert),
                         (const GLvoid*)cmds->VtxBuffer.Data, GL_STREAM_DRAW);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elements_buffer);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)cmds->IdxBuffer.Size * RG_SIZE(ImDrawIdx),
                         (const GLvoid*)cmds->IdxBuffer.Data, GL_STREAM_DRAW);

            for (const ImDrawCmd &cmd: cmds->CmdBuffer) {
                if (cmd.UserCallback) {
                    cmd.UserCallback(cmds, &cmd);
                } else {
                    glBindTexture(GL_TEXTURE_2D, (GLuint)(intptr_t)cmd.GetTexID());
                    glScissor((int)cmd.ClipRect.x, state.display.height - (int)cmd.ClipRect.w,
                              (int)(cmd.ClipRect.z - cmd.ClipRect.x),
                              (int)(cmd.ClipRect.w - cmd.ClipRect.y));

                    glDrawElementsBaseVertex(GL_TRIANGLES, (GLsizei)cmd.ElemCount,
                                             RG_SIZE(ImDrawIdx) == 2 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT,
                                             idx_buffer_offset, (GLint)cmd.VtxOffset);
                }
                idx_buffer_offset += cmd.ElemCount;
            }
        }
    }
}

}
