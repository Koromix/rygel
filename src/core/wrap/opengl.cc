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
#include "opengl.hh"

namespace K {

bool ogl_InitFunctions(void *(*get_proc_address)(const char *name))
{
    int gl_version;
    {
        GLint major, minor;
        glGetIntegerv(GL_MAJOR_VERSION, &major);
        glGetIntegerv(GL_MINOR_VERSION, &minor);
        K_ASSERT(major < 10 && minor < 10);
        gl_version = major * 10 + minor;
        if (gl_version > 33) {
            gl_version = 33;
        }
    }

#if !defined(OGL_NO_COMPAT)
    bool gl_compat;
    if (gl_version >= 32) {
        GLint profile;
        glGetIntegerv(GL_CONTEXT_PROFILE_MASK, &profile);
        gl_compat = (profile & GL_CONTEXT_COMPATIBILITY_PROFILE_BIT);
    } else {
        gl_compat = true;
    }
#endif

#define OGL_FUNCTION(Cond, Type, Name, ...) \
        do { \
            if (Cond) { \
                Name = (decltype(Name))get_proc_address(#Name); \
                if (!Name) { \
                    LogError("Required OpenGL function '%1' is not available",  #Name); \
                    return false; \
                } \
            } else { \
                Name = nullptr; \
            } \
        } while (false)
#include "opengl_func.inc"

    return true;
}

static void LogShaderError(GLuint id, void (GL_API *msg_func)(GLuint, GLsizei, GLsizei *,
                                                              GLchar *),
                           const char *type, const char *name = nullptr)
{
    if (!name) {
        name = "?";
    }

    char buf[512];
    msg_func(id, K_SIZE(buf), nullptr, buf);
    Size len = (Size)strlen(buf);
    while (len && strchr(" \t\r\n", buf[len - 1])) {
        len--;
    }
    buf[len] = 0;

    LogError("Failed to build %1 '%2':\n%3", type, name, buf);
}

GLuint ogl_BuildShader(const char *name, const char *vertex_src, const char *fragment_src)
{
    GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    K_DEFER { glDeleteShader(vertex_shader); };
    {
        glShaderSource(vertex_shader, 1, &vertex_src, nullptr);
        glCompileShader(vertex_shader);

        GLint success;
        glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            LogShaderError(vertex_shader, glGetShaderInfoLog, "vertex shader", name);
            return 0;
        }
    }

    GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    K_DEFER { glDeleteShader(fragment_shader); };
    {
        glShaderSource(fragment_shader, 1, &fragment_src, nullptr);
        glCompileShader(fragment_shader);

        GLint success;
        glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            LogShaderError(fragment_shader, glGetShaderInfoLog, "fragment shader", name);
            return 0;
        }
    }

    GLuint shader_program = glCreateProgram();
    K_DEFER_N(program_guard) { glDeleteProgram(shader_program); };
    {
        glAttachShader(shader_program, vertex_shader);
        glAttachShader(shader_program, fragment_shader);
        glLinkProgram(shader_program);

        GLint success;
        glGetProgramiv(shader_program, GL_LINK_STATUS, &success);
        if (!success) {
            LogShaderError(shader_program, glGetProgramInfoLog, "shader program", name);
            return 0;
        }
    }

    program_guard.Disable();
    return shader_program;
}

}

#if !defined(__EMSCRIPTEN__)
    #define OGL_FUNCTION(Cond, ...) \
        K_FORCE_EXPAND(OGL_FUNCTION_PTR(__VA_ARGS__))
    #include "opengl_func.inc"
#endif
