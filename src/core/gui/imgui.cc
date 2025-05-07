// Copyright (C) 2025  Niels Martignène <niels.martignene@protonmail.com>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of
// this software and associated documentation files (the “Software”), to deal in
// the Software without restriction, including without limitation the rights to use,
// copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the
// Software, and to permit persons to whom the Software is furnished to do so,
// subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
// OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
// HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
// WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
// OTHER DEALINGS IN THE SOFTWARE.

#include "src/core/base/base.hh"
#include "window.hh"
#include "src/core/wrap/opengl.hh"
RG_PUSH_NO_WARNINGS
#define IMGUI_DISABLE_OBSOLETE_FUNCTIONS
#include "vendor/imgui/imgui.h"
#include "vendor/imgui/imgui_internal.h"
RG_POP_NO_WARNINGS

namespace RG {

extern "C" const AssetInfo RobotoMediumTtf;

static const char *const ImGuiVertexCode =
#if defined(__EMSCRIPTEN__)
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
#if defined(__EMSCRIPTEN__)
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

static RG_CONSTINIT ConstMap<128, uint8_t, ImGuiKey> KeyMap {
    { (uint8_t)gui_InputKey::Control, ImGuiMod_Ctrl },
    { (uint8_t)gui_InputKey::Alt, ImGuiMod_Alt },
    { (uint8_t)gui_InputKey::Shift, ImGuiMod_Shift },

    { (uint8_t)gui_InputKey::Tab, ImGuiKey_Tab },
    { (uint8_t)gui_InputKey::Delete, ImGuiKey_Delete },
    { (uint8_t)gui_InputKey::Backspace, ImGuiKey_Backspace },
    { (uint8_t)gui_InputKey::Enter, ImGuiKey_Enter },
    { (uint8_t)gui_InputKey::Escape, ImGuiKey_Escape },
    { (uint8_t)gui_InputKey::Home, ImGuiKey_Home },
    { (uint8_t)gui_InputKey::End, ImGuiKey_End },
    { (uint8_t)gui_InputKey::PageUp, ImGuiKey_PageUp },
    { (uint8_t)gui_InputKey::PageDown, ImGuiKey_PageDown },
    { (uint8_t)gui_InputKey::Left, ImGuiKey_LeftArrow },
    { (uint8_t)gui_InputKey::Right, ImGuiKey_RightArrow },
    { (uint8_t)gui_InputKey::Up, ImGuiKey_UpArrow },
    { (uint8_t)gui_InputKey::Down, ImGuiKey_DownArrow },
    { (uint8_t)gui_InputKey::A, ImGuiKey_A },
    { (uint8_t)gui_InputKey::B, ImGuiKey_B },
    { (uint8_t)gui_InputKey::C, ImGuiKey_C },
    { (uint8_t)gui_InputKey::D, ImGuiKey_D },
    { (uint8_t)gui_InputKey::E, ImGuiKey_E },
    { (uint8_t)gui_InputKey::F, ImGuiKey_F },
    { (uint8_t)gui_InputKey::G, ImGuiKey_G },
    { (uint8_t)gui_InputKey::H, ImGuiKey_H },
    { (uint8_t)gui_InputKey::I, ImGuiKey_I },
    { (uint8_t)gui_InputKey::J, ImGuiKey_J },
    { (uint8_t)gui_InputKey::K, ImGuiKey_K },
    { (uint8_t)gui_InputKey::L, ImGuiKey_L },
    { (uint8_t)gui_InputKey::M, ImGuiKey_M },
    { (uint8_t)gui_InputKey::N, ImGuiKey_N },
    { (uint8_t)gui_InputKey::O, ImGuiKey_O },
    { (uint8_t)gui_InputKey::P, ImGuiKey_P },
    { (uint8_t)gui_InputKey::Q, ImGuiKey_Q },
    { (uint8_t)gui_InputKey::R, ImGuiKey_R },
    { (uint8_t)gui_InputKey::S, ImGuiKey_S },
    { (uint8_t)gui_InputKey::T, ImGuiKey_T },
    { (uint8_t)gui_InputKey::U, ImGuiKey_U },
    { (uint8_t)gui_InputKey::V, ImGuiKey_V },
    { (uint8_t)gui_InputKey::W, ImGuiKey_W },
    { (uint8_t)gui_InputKey::X, ImGuiKey_Y },
    { (uint8_t)gui_InputKey::Z, ImGuiKey_Z }
};

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
                          (GLvoid *)offsetof(ImDrawVert, pos));
    glVertexAttribPointer(attrib_uv, 2, GL_FLOAT, GL_FALSE, RG_SIZE(ImDrawVert),
                          (GLvoid *)offsetof(ImDrawVert, uv));
    glVertexAttribPointer(attrib_color, 4, GL_UNSIGNED_BYTE, GL_TRUE, RG_SIZE(ImDrawVert),
                          (GLvoid *)offsetof(ImDrawVert, col));

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
        io->Fonts->SetTexID((ImTextureID)font_texture);
    }

    io->BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;

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

    for (gui_KeyEvent evt: state.input.events) {
        ImGuiKey key = KeyMap.FindValue(evt.key, ImGuiKey_None);
        if (key == ImGuiKey_None)
            continue;
        io->AddKeyEvent(key, evt.down);
    }
    io->AddInputCharactersUTF8(state.input.text.data);

    io->AddMousePosEvent((float)state.input.x, (float)state.input.y);
    for (int i = 0; i < RG_LEN(io->MouseDown); i++) {
        bool down = (state.input.buttons & (unsigned int)(1 << i));
        io->AddMouseButtonEvent(i, down);
    }
    io->AddMouseWheelEvent((float)state.input.wheel_x, (float)state.input.wheel_y);

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
