// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "opengl.hh"
#include "render.hh"
#include "runner.hh"

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

static const char *imgui_vertex_src = R"!(
    #version 330 core

    uniform mat4 ProjMtx;
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
static const char *imgui_fragment_src = R"!(
    #version 330 core

    uniform sampler2D Texture;
    in vec2 Frag_UV;
    in vec4 Frag_Color;
    out vec4 Out_Color;

    void main()
    {
        Out_Color = Frag_Color * texture(Texture, Frag_UV.st);
    }
)!";

static void ReleaseImGui();
static bool InitImGui()
{
    DEFER_N(imgui_guard) { ReleaseImGui(); };

    ImGuiIO *io = &ImGui::GetIO();

    io->IniFilename = nullptr;

    {
        GLuint new_shader = BuildGLShader("imgui", imgui_vertex_src, imgui_fragment_src);
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
    glVertexAttribPointer(attrib_position, 2, GL_FLOAT, GL_FALSE, SIZE(ImDrawVert),
                          (GLvoid *)OFFSET_OF(ImDrawVert, pos));
    glVertexAttribPointer(attrib_uv, 2, GL_FLOAT, GL_FALSE, SIZE(ImDrawVert),
                          (GLvoid *)OFFSET_OF(ImDrawVert, uv));
    glVertexAttribPointer(attrib_color, 4, GL_UNSIGNED_BYTE, GL_TRUE, SIZE(ImDrawVert),
                          (GLvoid *)OFFSET_OF(ImDrawVert, col));

    if (!font_texture) {
        uint8_t *pixels;
        int width, height;
        // TODO: Switch to GetTexDataAsAlpha8() eventually
        io->Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

        glGenTextures(1, &font_texture);
        glBindTexture(GL_TEXTURE_2D, font_texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, pixels);
        io->Fonts->TexID = (void *)(intptr_t)font_texture;
    }

    io->KeyMap[ImGuiKey_Tab] = (int)RunIO::Key::Tab;
    io->KeyMap[ImGuiKey_Delete] = (int)RunIO::Key::Delete;
    io->KeyMap[ImGuiKey_Backspace] = (int)RunIO::Key::Backspace;
    io->KeyMap[ImGuiKey_Enter] = (int)RunIO::Key::Enter;
    io->KeyMap[ImGuiKey_Escape] = (int)RunIO::Key::Escape;
    io->KeyMap[ImGuiKey_Home] = (int)RunIO::Key::Home;
    io->KeyMap[ImGuiKey_End] = (int)RunIO::Key::End;
    io->KeyMap[ImGuiKey_PageUp] = (int)RunIO::Key::PageUp;
    io->KeyMap[ImGuiKey_PageDown] = (int)RunIO::Key::PageDown;
    io->KeyMap[ImGuiKey_LeftArrow] = (int)RunIO::Key::Left;
    io->KeyMap[ImGuiKey_RightArrow] = (int)RunIO::Key::Right;
    io->KeyMap[ImGuiKey_UpArrow] = (int)RunIO::Key::Up;
    io->KeyMap[ImGuiKey_DownArrow] = (int)RunIO::Key::Down;
    io->KeyMap[ImGuiKey_A] = (int)RunIO::Key::A;
    io->KeyMap[ImGuiKey_C] = (int)RunIO::Key::C;
    io->KeyMap[ImGuiKey_V] = (int)RunIO::Key::V;
    io->KeyMap[ImGuiKey_X] = (int)RunIO::Key::X;
    io->KeyMap[ImGuiKey_Y] = (int)RunIO::Key::Y;
    io->KeyMap[ImGuiKey_Z] = (int)RunIO::Key::Z;

    imgui_guard.disable();
    return true;
}

static void ReleaseImGui()
{
    ImGui::Shutdown();

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

bool StartRender()
{
    if (!g_io->main.iteration_count) {
        if (!InitGLFunctions())
            return false;
        if (!InitImGui())
            return false;
    }

    ImGuiIO *io = &ImGui::GetIO();

    io->DisplaySize = ImVec2((float)g_io->display.width, (float)g_io->display.height);
    io->DeltaTime = g_io->time.monotonic_delta;

    memset(io->KeysDown, 0, SIZE(io->KeysDown));
    for (Size idx: g_io->input.keys) {
        io->KeysDown[idx] = true;
    }
    io->KeyCtrl = g_io->input.keys.Test((Size)RunIO::Key::Control);
    io->KeyAlt = g_io->input.keys.Test((Size)RunIO::Key::Alt);
    io->KeyShift = g_io->input.keys.Test((Size)RunIO::Key::Shift);
    io->AddInputCharactersUTF8(g_io->input.text.data);

    io->MousePos = ImVec2((float)g_io->input.x, (float)g_io->input.y);
    for (Size i = 0; i < ARRAY_SIZE(io->MouseDown); i++) {
        io->MouseDown[i] = g_io->input.buttons & (unsigned int)(1 << i);
    }
    io->MouseWheel = (float)g_io->input.wheel_y;

    ImGui::NewFrame();

    return true;
}

void Render()
{
    // Clear screen
    glViewport(0, 0, g_io->display.width, g_io->display.height);
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
            glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)cmds->VtxBuffer.Size * SIZE(ImDrawVert),
                         (const GLvoid*)cmds->VtxBuffer.Data, GL_STREAM_DRAW);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elements_buffer);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)cmds->IdxBuffer.Size * SIZE(ImDrawIdx),
                         (const GLvoid*)cmds->IdxBuffer.Data, GL_STREAM_DRAW);

            for (const ImDrawCmd &cmd: cmds->CmdBuffer) {
                if (cmd.UserCallback) {
                    cmd.UserCallback(cmds, &cmd);
                } else {
                    glBindTexture(GL_TEXTURE_2D, (GLuint)(intptr_t)cmd.TextureId);
                    glScissor((int)cmd.ClipRect.x, g_io->display.height - (int)cmd.ClipRect.w,
                              (int)(cmd.ClipRect.z - cmd.ClipRect.x),
                              (int)(cmd.ClipRect.w - cmd.ClipRect.y));
                    glDrawElements(GL_TRIANGLES, (GLsizei)cmd.ElemCount,
                                   SIZE(ImDrawIdx) == 2 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT,
                                   idx_buffer_offset);
                }
                idx_buffer_offset += cmd.ElemCount;
            }
        }
    }
}

void ReleaseRender()
{
    ReleaseImGui();
}
