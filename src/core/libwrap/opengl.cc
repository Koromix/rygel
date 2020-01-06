// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../libcc/libcc.hh"
#include "opengl.hh"

namespace RG {

bool ogl_InitFunctions(void *(*get_proc_address)(const char *name))
{
    int gl_version;
    {
        GLint major, minor;
        glGetIntegerv(GL_MAJOR_VERSION, &major);
        glGetIntegerv(GL_MINOR_VERSION, &minor);
        RG_ASSERT(major < 10 && minor < 10);
        gl_version = major * 10 + minor;
        if (gl_version > 33) {
            gl_version = 33;
        }
    }

#ifndef OGL_NO_COMPAT
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
    msg_func(id, RG_SIZE(buf), nullptr, buf);
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
    RG_DEFER { glDeleteShader(vertex_shader); };
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
    RG_DEFER { glDeleteShader(fragment_shader); };
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
    RG_DEFER_N(program_guard) { glDeleteProgram(shader_program); };
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

#ifndef __EMSCRIPTEN__
    #define OGL_FUNCTION(Cond, ...) \
        RG_FORCE_EXPAND(OGL_FUNCTION_PTR(__VA_ARGS__))
    #include "opengl_func.inc"
#endif
