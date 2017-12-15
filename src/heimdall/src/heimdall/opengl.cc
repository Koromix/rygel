#include "kutil.hh"
#include "opengl.hh"

bool InitGLFunctions()
{
    int gl_version;
    {
        GLint major, minor;
        glGetIntegerv(GL_MAJOR_VERSION, &major);
        glGetIntegerv(GL_MINOR_VERSION, &minor);
        Assert(major < 10 && minor < 10);
        gl_version = major * 10 + minor;
        if (gl_version > 33) {
            gl_version = 33;
        }
    }

#ifndef GL_NO_COMPAT
    bool gl_compat;
    if (gl_version >= 32) {
        GLint profile;
        glGetIntegerv(GL_CONTEXT_PROFILE_MASK, &profile);
        gl_compat = (profile & GL_CONTEXT_COMPATIBILITY_PROFILE_BIT);
    } else {
        gl_compat = true;
    }
#endif

#define GL_FUNCTION(Cond, Type, Name, ...) \
        do { \
            if (Cond) { \
                Name = (decltype(Name))GetGLProcAddress(#Name); \
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
    msg_func(id, SIZE(buf), nullptr, buf);
    Size len = (Size)strlen(buf);
    while (len && strchr(" \t\r\n", buf[len - 1])) {
        len--;
    }
    buf[len] = 0;

    LogError("Failed to build %1 '%2':\n%3", type, name, buf);
}

GLuint BuildGLShader(const char *name, const char *vertex_src, const char *fragment_src)
{
    GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    DEFER { glDeleteShader(vertex_shader); };
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
    DEFER { glDeleteShader(fragment_shader); };
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
    DEFER_N(program_guard) { glDeleteProgram(shader_program); };
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

    program_guard.disable();
    return shader_program;
}

#define GL_FUNCTION(Cond, ...) \
    FORCE_EXPAND(GL_FUNCTION_PTR(__VA_ARGS__))
#include "opengl_func.inc"
