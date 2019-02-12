// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef GL_FUNCTION
    #error Please define GL_FUNCTION() before including opengl_func.inc
#endif

#ifdef __EMSCRIPTEN__

// Nothing to load (yet?)

#else

// OpenGL 1.2

GL_FUNCTION(gl_version >= 12, void, glCopyTexSubImage3D, GLenum target, GLint level, GLint xoffset,
            GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height);
GL_FUNCTION(gl_version >= 12, void, glDrawRangeElements, GLenum mode, GLuint start, GLuint end,
            GLsizei count, GLenum type, const void *indices);
GL_FUNCTION(gl_version >= 12, void, glTexImage3D, GLenum target, GLint level, GLint internalformat,
            GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format,
            GLenum type, const void *pixels);
GL_FUNCTION(gl_version >= 12, void, glTexSubImage3D, GLenum target, GLint level, GLint xoffset,
            GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth,
            GLenum format, GLenum type, const void *pixels);

// OpenGL 1.3

GL_FUNCTION(gl_version >= 13, void, glActiveTexture, GLenum texture);
#ifndef GL_NO_COMPAT
GL_FUNCTION(gl_version >= 13 && gl_compat, void, glClientActiveTexture, GLenum texture);
#endif
GL_FUNCTION(gl_version >= 13, void, glCompressedTexImage1D, GLenum target, GLint level,
            GLenum internalformat, GLsizei width, GLint border, GLsizei imageSize,
            const void *data);
GL_FUNCTION(gl_version >= 13, void, glCompressedTexImage2D, GLenum target, GLint level,
            GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize,
            const void *data);
GL_FUNCTION(gl_version >= 13, void, glCompressedTexImage3D, GLenum target, GLint level,
            GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border,
            GLsizei imageSize, const void *data);
GL_FUNCTION(gl_version >= 13, void, glCompressedTexSubImage1D, GLenum target, GLint level,
            GLint xoffset, GLsizei width, GLenum format, GLsizei imageSize, const void *data);
GL_FUNCTION(gl_version >= 13, void, glCompressedTexSubImage2D, GLenum target, GLint level,
            GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format,
            GLsizei imageSize, const void *data);
GL_FUNCTION(gl_version >= 13, void, glCompressedTexSubImage3D, GLenum target, GLint level,
            GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height,
            GLsizei depth, GLenum format, GLsizei imageSize, const void *data);
GL_FUNCTION(gl_version >= 13, void, glGetCompressedTexImage, GLenum target, GLint level,
            void *img);
#ifndef GL_NO_COMPAT
GL_FUNCTION(gl_version >= 13 && gl_compat, void, glLoadTransposeMatrixd, const GLdouble *m);
GL_FUNCTION(gl_version >= 13 && gl_compat, void, glLoadTransposeMatrixf, const GLfloat *m);
GL_FUNCTION(gl_version >= 13 && gl_compat, void, glMultTransposeMatrixd, const GLdouble *m);
GL_FUNCTION(gl_version >= 13 && gl_compat, void, glMultTransposeMatrixf, const GLfloat *m);
GL_FUNCTION(gl_version >= 13 && gl_compat, void, glMultiTexCoord1d, GLenum target, GLdouble s);
GL_FUNCTION(gl_version >= 13 && gl_compat, void, glMultiTexCoord1dv, GLenum target,
            const GLdouble *v);
GL_FUNCTION(gl_version >= 13 && gl_compat, void, glMultiTexCoord1f, GLenum target, GLfloat s);
GL_FUNCTION(gl_version >= 13 && gl_compat, void, glMultiTexCoord1fv, GLenum target,
            const GLfloat *v);
GL_FUNCTION(gl_version >= 13 && gl_compat, void, glMultiTexCoord1i, GLenum target, GLint s);
GL_FUNCTION(gl_version >= 13 && gl_compat, void, glMultiTexCoord1iv, GLenum target,
            const GLint *v);
GL_FUNCTION(gl_version >= 13 && gl_compat, void, glMultiTexCoord1s, GLenum target, GLshort s);
GL_FUNCTION(gl_version >= 13 && gl_compat, void, glMultiTexCoord1sv, GLenum target,
            const GLshort *v);
GL_FUNCTION(gl_version >= 13 && gl_compat, void, glMultiTexCoord2d, GLenum target, GLdouble s,
            GLdouble t);
GL_FUNCTION(gl_version >= 13 && gl_compat, void, glMultiTexCoord2dv, GLenum target,
            const GLdouble *v);
GL_FUNCTION(gl_version >= 13 && gl_compat, void, glMultiTexCoord2f, GLenum target, GLfloat s,
            GLfloat t);
GL_FUNCTION(gl_version >= 13 && gl_compat, void, glMultiTexCoord2fv, GLenum target,
            const GLfloat *v);
GL_FUNCTION(gl_version >= 13 && gl_compat, void, glMultiTexCoord2i, GLenum target, GLint s,
            GLint t);
GL_FUNCTION(gl_version >= 13 && gl_compat, void, glMultiTexCoord2iv, GLenum target,
            const GLint *v);
GL_FUNCTION(gl_version >= 13 && gl_compat, void, glMultiTexCoord2s, GLenum target, GLshort s,
            GLshort t);
GL_FUNCTION(gl_version >= 13 && gl_compat, void, glMultiTexCoord2sv, GLenum target,
            const GLshort *v);
GL_FUNCTION(gl_version >= 13 && gl_compat, void, glMultiTexCoord3d, GLenum target, GLdouble s,
            GLdouble t, GLdouble r);
GL_FUNCTION(gl_version >= 13 && gl_compat, void, glMultiTexCoord3dv, GLenum target,
            const GLdouble *v);
GL_FUNCTION(gl_version >= 13 && gl_compat, void, glMultiTexCoord3f, GLenum target, GLfloat s,
            GLfloat t, GLfloat r);
GL_FUNCTION(gl_version >= 13 && gl_compat, void, glMultiTexCoord3fv, GLenum target,
            const GLfloat *v);
GL_FUNCTION(gl_version >= 13 && gl_compat, void, glMultiTexCoord3i, GLenum target, GLint s,
            GLint t, GLint r);
GL_FUNCTION(gl_version >= 13 && gl_compat, void, glMultiTexCoord3iv, GLenum target,
            const GLint *v);
GL_FUNCTION(gl_version >= 13 && gl_compat, void, glMultiTexCoord3s, GLenum target, GLshort s,
            GLshort t, GLshort r);
GL_FUNCTION(gl_version >= 13 && gl_compat, void, glMultiTexCoord3sv, GLenum target,
            const GLshort *v);
GL_FUNCTION(gl_version >= 13 && gl_compat, void, glMultiTexCoord4d, GLenum target, GLdouble s,
            GLdouble t, GLdouble r, GLdouble q);
GL_FUNCTION(gl_version >= 13 && gl_compat, void, glMultiTexCoord4dv, GLenum target,
            const GLdouble *v);
GL_FUNCTION(gl_version >= 13 && gl_compat, void, glMultiTexCoord4f, GLenum target,
            GLfloat s, GLfloat t, GLfloat r, GLfloat q);
GL_FUNCTION(gl_version >= 13 && gl_compat, void, glMultiTexCoord4fv, GLenum target,
            const GLfloat *v);
GL_FUNCTION(gl_version >= 13 && gl_compat, void, glMultiTexCoord4i, GLenum target, GLint s,
            GLint t, GLint r, GLint q);
GL_FUNCTION(gl_version >= 13 && gl_compat, void, glMultiTexCoord4iv, GLenum target,
            const GLint *v);
GL_FUNCTION(gl_version >= 13 && gl_compat, void, glMultiTexCoord4s, GLenum target, GLshort s,
            GLshort t, GLshort r, GLshort q);
GL_FUNCTION(gl_version >= 13 && gl_compat, void, glMultiTexCoord4sv, GLenum target,
            const GLshort *v);
#endif
GL_FUNCTION(gl_version >= 13, void, glSampleCoverage, GLfloat value, GLboolean invert);

// OpenGL 1.4

GL_FUNCTION(gl_version >= 14, void, glBlendColor, GLfloat red, GLfloat green, GLfloat blue,
            GLfloat alpha);
GL_FUNCTION(gl_version >= 14, void, glBlendEquation, GLenum mode);
GL_FUNCTION(gl_version >= 14, void, glBlendFuncSeparate, GLenum sfactorRGB, GLenum dfactorRGB,
            GLenum sfactorAlpha, GLenum dfactorAlpha);
#ifndef GL_NO_COMPAT
GL_FUNCTION(gl_version >= 14 && gl_compat, void, glFogCoordPointer, GLenum type, GLsizei stride,
            const void *pointer);
GL_FUNCTION(gl_version >= 14 && gl_compat, void, glFogCoordd, GLdouble coord);
GL_FUNCTION(gl_version >= 14 && gl_compat, void, glFogCoorddv, const GLdouble *coord);
GL_FUNCTION(gl_version >= 14 && gl_compat, void, glFogCoordf, GLfloat coord);
GL_FUNCTION(gl_version >= 14 && gl_compat, void, glFogCoordfv, const GLfloat *coord);
#endif
GL_FUNCTION(gl_version >= 14, void, glMultiDrawArrays, GLenum mode, const GLint *first,
            const GLsizei *count, GLsizei drawcount);
GL_FUNCTION(gl_version >= 14, void, glMultiDrawElements, GLenum mode, const GLsizei *count,
            GLenum type, const void *const *indices, GLsizei drawcount);
GL_FUNCTION(gl_version >= 14, void, glPointParameterf, GLenum pname, GLfloat param);
GL_FUNCTION(gl_version >= 14, void, glPointParameterfv, GLenum pname, const GLfloat *params);
GL_FUNCTION(gl_version >= 14, void, glPointParameteri, GLenum pname, GLint param);
GL_FUNCTION(gl_version >= 14, void, glPointParameteriv, GLenum pname, const GLint *params);
#ifndef GL_NO_COMPAT
GL_FUNCTION(gl_version >= 14 && gl_compat, void, glSecondaryColor3b, GLbyte red, GLbyte green,
            GLbyte blue);
GL_FUNCTION(gl_version >= 14 && gl_compat, void, glSecondaryColor3bv, const GLbyte *v);
GL_FUNCTION(gl_version >= 14 && gl_compat, void, glSecondaryColor3d, GLdouble red, GLdouble green,
            GLdouble blue);
GL_FUNCTION(gl_version >= 14 && gl_compat, void, glSecondaryColor3dv, const GLdouble *v);
GL_FUNCTION(gl_version >= 14 && gl_compat, void, glSecondaryColor3f, GLfloat red, GLfloat green,
            GLfloat blue);
GL_FUNCTION(gl_version >= 14 && gl_compat, void, glSecondaryColor3fv, const GLfloat *v);
GL_FUNCTION(gl_version >= 14 && gl_compat, void, glSecondaryColor3i, GLint red, GLint green,
            GLint blue);
GL_FUNCTION(gl_version >= 14 && gl_compat, void, glSecondaryColor3iv, const GLint *v);
GL_FUNCTION(gl_version >= 14 && gl_compat, void, glSecondaryColor3s, GLshort red, GLshort green,
            GLshort blue);
GL_FUNCTION(gl_version >= 14 && gl_compat, void, glSecondaryColor3sv, const GLshort *v);
GL_FUNCTION(gl_version >= 14 && gl_compat, void, glSecondaryColor3ub, GLubyte red, GLubyte green,
            GLubyte blue);
GL_FUNCTION(gl_version >= 14 && gl_compat, void, glSecondaryColor3ubv, const GLubyte *v);
GL_FUNCTION(gl_version >= 14 && gl_compat, void, glSecondaryColor3ui, GLuint red, GLuint green,
            GLuint blue);
GL_FUNCTION(gl_version >= 14 && gl_compat, void, glSecondaryColor3uiv, const GLuint *v);
GL_FUNCTION(gl_version >= 14 && gl_compat, void, glSecondaryColor3us, GLushort red, GLushort green,
            GLushort blue);
GL_FUNCTION(gl_version >= 14 && gl_compat, void, glSecondaryColor3usv, const GLushort *v);
GL_FUNCTION(gl_version >= 14 && gl_compat, void, glSecondaryColorPointer, GLint size, GLenum type,
            GLsizei stride, const void *pointer);
GL_FUNCTION(gl_version >= 14 && gl_compat, void, glWindowPos2d, GLdouble x, GLdouble y);
GL_FUNCTION(gl_version >= 14 && gl_compat, void, glWindowPos2dv, const GLdouble *v);
GL_FUNCTION(gl_version >= 14 && gl_compat, void, glWindowPos2f, GLfloat x, GLfloat y);
GL_FUNCTION(gl_version >= 14 && gl_compat, void, glWindowPos2fv, const GLfloat *v);
GL_FUNCTION(gl_version >= 14 && gl_compat, void, glWindowPos2i, GLint x, GLint y);
GL_FUNCTION(gl_version >= 14 && gl_compat, void, glWindowPos2iv, const GLint *v);
GL_FUNCTION(gl_version >= 14 && gl_compat, void, glWindowPos2s, GLshort x, GLshort y);
GL_FUNCTION(gl_version >= 14 && gl_compat, void, glWindowPos2sv, const GLshort *v);
GL_FUNCTION(gl_version >= 14 && gl_compat, void, glWindowPos3d, GLdouble x, GLdouble y,
            GLdouble z);
GL_FUNCTION(gl_version >= 14 && gl_compat, void, glWindowPos3dv, const GLdouble *v);
GL_FUNCTION(gl_version >= 14 && gl_compat, void, glWindowPos3f, GLfloat x, GLfloat y, GLfloat z);
GL_FUNCTION(gl_version >= 14 && gl_compat, void, glWindowPos3fv, const GLfloat *v);
GL_FUNCTION(gl_version >= 14 && gl_compat, void, glWindowPos3i, GLint x, GLint y, GLint z);
GL_FUNCTION(gl_version >= 14 && gl_compat, void, glWindowPos3iv, const GLint *v);
GL_FUNCTION(gl_version >= 14 && gl_compat, void, glWindowPos3s, GLshort x, GLshort y, GLshort z);
GL_FUNCTION(gl_version >= 14 && gl_compat, void, glWindowPos3sv, const GLshort *v);
#endif

// OpenGL 1.5

GL_FUNCTION(gl_version >= 15, void, glBeginQuery, GLenum target, GLuint id);
GL_FUNCTION(gl_version >= 15, void, glBindBuffer, GLenum target, GLuint buffer);
GL_FUNCTION(gl_version >= 15, void, glBufferData, GLenum target, GLsizeiptr size, const void *data,
            GLenum usage);
GL_FUNCTION(gl_version >= 15, void, glBufferSubData, GLenum target, GLintptr offset,
            GLsizeiptr size, const void *data);
GL_FUNCTION(gl_version >= 15, void, glDeleteBuffers, GLsizei n, const GLuint *buffers);
GL_FUNCTION(gl_version >= 15, void, glDeleteQueries, GLsizei n, const GLuint *ids);
GL_FUNCTION(gl_version >= 15, void, glEndQuery, GLenum target);
GL_FUNCTION(gl_version >= 15, void, glGenBuffers, GLsizei n, GLuint *buffers);
GL_FUNCTION(gl_version >= 15, void, glGenQueries, GLsizei n, GLuint *ids);
GL_FUNCTION(gl_version >= 15, void, glGetBufferParameteriv, GLenum target, GLenum pname,
            GLint *params);
GL_FUNCTION(gl_version >= 15, void, glGetBufferPointerv, GLenum target, GLenum pname,
            void **params);
GL_FUNCTION(gl_version >= 15, void, glGetBufferSubData, GLenum target, GLintptr offset,
            GLsizeiptr size, void *data);
GL_FUNCTION(gl_version >= 15, void, glGetQueryObjectiv, GLuint id, GLenum pname, GLint *params);
GL_FUNCTION(gl_version >= 15, void, glGetQueryObjectuiv, GLuint id, GLenum pname, GLuint *params);
GL_FUNCTION(gl_version >= 15, void, glGetQueryiv, GLenum target, GLenum pname, GLint *params);
GL_FUNCTION(gl_version >= 15, GLboolean, glIsBuffer, GLuint buffer);
GL_FUNCTION(gl_version >= 15, GLboolean, glIsQuery, GLuint id);
GL_FUNCTION(gl_version >= 15, void *, glMapBuffer, GLenum target, GLenum access);
GL_FUNCTION(gl_version >= 15, GLboolean, glUnmapBuffer, GLenum target);

// OpenGL 2.0

GL_FUNCTION(gl_version >= 20, void, glAttachShader, GLuint program, GLuint shader);
GL_FUNCTION(gl_version >= 20, void, glBindAttribLocation, GLuint program, GLuint index,
            const GLchar *name);
GL_FUNCTION(gl_version >= 20, void, glBlendEquationSeparate, GLenum modeRGB, GLenum modeAlpha);
GL_FUNCTION(gl_version >= 20, void, glCompileShader, GLuint shader);
GL_FUNCTION(gl_version >= 20, GLuint, glCreateProgram, void);
GL_FUNCTION(gl_version >= 20, GLuint, glCreateShader, GLenum type);
GL_FUNCTION(gl_version >= 20, void, glDeleteProgram, GLuint program);
GL_FUNCTION(gl_version >= 20, void, glDeleteShader, GLuint shader);
GL_FUNCTION(gl_version >= 20, void, glDetachShader, GLuint program, GLuint shader);
GL_FUNCTION(gl_version >= 20, void, glDisableVertexAttribArray, GLuint index);
GL_FUNCTION(gl_version >= 20, void, glDrawBuffers, GLsizei n, const GLenum *bufs);
GL_FUNCTION(gl_version >= 20, void, glEnableVertexAttribArray, GLuint index);
GL_FUNCTION(gl_version >= 20, void, glGetActiveAttrib, GLuint program, GLuint index,
            GLsizei bufSize, GLsizei *length, GLint *size, GLenum *type, GLchar *name);
GL_FUNCTION(gl_version >= 20, void, glGetActiveUniform, GLuint program, GLuint index,
            GLsizei bufSize, GLsizei *length, GLint *size, GLenum *type, GLchar *name);
GL_FUNCTION(gl_version >= 20, void, glGetAttachedShaders, GLuint program, GLsizei maxCount,
            GLsizei *count, GLuint *shaders);
GL_FUNCTION(gl_version >= 20, GLint, glGetAttribLocation, GLuint program, const GLchar *name);
GL_FUNCTION(gl_version >= 20, void, glGetProgramInfoLog, GLuint program, GLsizei bufSize,
            GLsizei *length, GLchar *infoLog);
GL_FUNCTION(gl_version >= 20, void, glGetProgramiv, GLuint program, GLenum pname, GLint *params);
GL_FUNCTION(gl_version >= 20, void, glGetShaderInfoLog, GLuint shader, GLsizei bufSize,
            GLsizei *length, GLchar *infoLog);
GL_FUNCTION(gl_version >= 20, void, glGetShaderSource, GLuint shader, GLsizei bufSize,
            GLsizei *length, GLchar *source);
GL_FUNCTION(gl_version >= 20, void, glGetShaderiv, GLuint shader, GLenum pname, GLint *params);
GL_FUNCTION(gl_version >= 20, GLint, glGetUniformLocation, GLuint program, const GLchar *name);
GL_FUNCTION(gl_version >= 20, void, glGetUniformfv, GLuint program, GLint location,
            GLfloat *params);
GL_FUNCTION(gl_version >= 20, void, glGetUniformiv, GLuint program, GLint location, GLint *params);
GL_FUNCTION(gl_version >= 20, void, glGetVertexAttribPointerv, GLuint index, GLenum pname,
            void **pointer);
GL_FUNCTION(gl_version >= 20, void, glGetVertexAttribdv, GLuint index, GLenum pname,
            GLdouble *params);
GL_FUNCTION(gl_version >= 20, void, glGetVertexAttribfv, GLuint index, GLenum pname,
            GLfloat *params);
GL_FUNCTION(gl_version >= 20, void, glGetVertexAttribiv, GLuint index, GLenum pname,
            GLint *params);
GL_FUNCTION(gl_version >= 20, GLboolean, glIsProgram, GLuint program);
GL_FUNCTION(gl_version >= 20, GLboolean, glIsShader, GLuint shader);
GL_FUNCTION(gl_version >= 20, void, glLinkProgram, GLuint program);
GL_FUNCTION(gl_version >= 20, void, glShaderSource, GLuint shader, GLsizei count,
            const GLchar *const *string, const GLint *length);
GL_FUNCTION(gl_version >= 20, void, glStencilFuncSeparate, GLenum face, GLenum func, GLint ref,
            GLuint mask);
GL_FUNCTION(gl_version >= 20, void, glStencilMaskSeparate, GLenum face, GLuint mask);
GL_FUNCTION(gl_version >= 20, void, glStencilOpSeparate, GLenum face, GLenum sfail, GLenum dpfail,
            GLenum dppass);
GL_FUNCTION(gl_version >= 20, void, glUniform1f, GLint location, GLfloat v0);
GL_FUNCTION(gl_version >= 20, void, glUniform1fv, GLint location, GLsizei count,
            const GLfloat *value);
GL_FUNCTION(gl_version >= 20, void, glUniform1i, GLint location, GLint v0);
GL_FUNCTION(gl_version >= 20, void, glUniform1iv, GLint location, GLsizei count,
            const GLint *value);
GL_FUNCTION(gl_version >= 20, void, glUniform2f, GLint location, GLfloat v0, GLfloat v1);
GL_FUNCTION(gl_version >= 20, void, glUniform2fv, GLint location, GLsizei count,
            const GLfloat *value);
GL_FUNCTION(gl_version >= 20, void, glUniform2i, GLint location, GLint v0, GLint v1);
GL_FUNCTION(gl_version >= 20, void, glUniform2iv, GLint location, GLsizei count,
            const GLint *value);
GL_FUNCTION(gl_version >= 20, void, glUniform3f, GLint location, GLfloat v0, GLfloat v1,
            GLfloat v2);
GL_FUNCTION(gl_version >= 20, void, glUniform3fv, GLint location, GLsizei count,
            const GLfloat *value);
GL_FUNCTION(gl_version >= 20, void, glUniform3i, GLint location, GLint v0, GLint v1, GLint v2);
GL_FUNCTION(gl_version >= 20, void, glUniform3iv, GLint location, GLsizei count,
            const GLint *value);
GL_FUNCTION(gl_version >= 20, void, glUniform4f, GLint location, GLfloat v0, GLfloat v1,
            GLfloat v2, GLfloat v3);
GL_FUNCTION(gl_version >= 20, void, glUniform4fv, GLint location, GLsizei count,
            const GLfloat *value);
GL_FUNCTION(gl_version >= 20, void, glUniform4i, GLint location, GLint v0, GLint v1, GLint v2,
            GLint v3);
GL_FUNCTION(gl_version >= 20, void, glUniform4iv, GLint location, GLsizei count,
            const GLint *value);
GL_FUNCTION(gl_version >= 20, void, glUniformMatrix2fv, GLint location, GLsizei count,
            GLboolean transpose, const GLfloat *value);
GL_FUNCTION(gl_version >= 20, void, glUniformMatrix3fv, GLint location, GLsizei count,
            GLboolean transpose, const GLfloat *value);
GL_FUNCTION(gl_version >= 20, void, glUniformMatrix4fv, GLint location, GLsizei count,
            GLboolean transpose, const GLfloat *value);
GL_FUNCTION(gl_version >= 20, void, glUseProgram, GLuint program);
GL_FUNCTION(gl_version >= 20, void, glValidateProgram, GLuint program);
GL_FUNCTION(gl_version >= 20, void, glVertexAttrib1d, GLuint index, GLdouble x);
GL_FUNCTION(gl_version >= 20, void, glVertexAttrib1dv, GLuint index, const GLdouble *v);
GL_FUNCTION(gl_version >= 20, void, glVertexAttrib1f, GLuint index, GLfloat x);
GL_FUNCTION(gl_version >= 20, void, glVertexAttrib1fv, GLuint index, const GLfloat *v);
GL_FUNCTION(gl_version >= 20, void, glVertexAttrib1s, GLuint index, GLshort x);
GL_FUNCTION(gl_version >= 20, void, glVertexAttrib1sv, GLuint index, const GLshort *v);
GL_FUNCTION(gl_version >= 20, void, glVertexAttrib2d, GLuint index, GLdouble x, GLdouble y);
GL_FUNCTION(gl_version >= 20, void, glVertexAttrib2dv, GLuint index, const GLdouble *v);
GL_FUNCTION(gl_version >= 20, void, glVertexAttrib2f, GLuint index, GLfloat x, GLfloat y);
GL_FUNCTION(gl_version >= 20, void, glVertexAttrib2fv, GLuint index, const GLfloat *v);
GL_FUNCTION(gl_version >= 20, void, glVertexAttrib2s, GLuint index, GLshort x, GLshort y);
GL_FUNCTION(gl_version >= 20, void, glVertexAttrib2sv, GLuint index, const GLshort *v);
GL_FUNCTION(gl_version >= 20, void, glVertexAttrib3d, GLuint index, GLdouble x, GLdouble y,
            GLdouble z);
GL_FUNCTION(gl_version >= 20, void, glVertexAttrib3dv, GLuint index, const GLdouble *v);
GL_FUNCTION(gl_version >= 20, void, glVertexAttrib3f, GLuint index, GLfloat x, GLfloat y,
            GLfloat z);
GL_FUNCTION(gl_version >= 20, void, glVertexAttrib3fv, GLuint index, const GLfloat *v);
GL_FUNCTION(gl_version >= 20, void, glVertexAttrib3s, GLuint index, GLshort x, GLshort y,
            GLshort z);
GL_FUNCTION(gl_version >= 20, void, glVertexAttrib3sv, GLuint index, const GLshort *v);
GL_FUNCTION(gl_version >= 20, void, glVertexAttrib4Nbv, GLuint index, const GLbyte *v);
GL_FUNCTION(gl_version >= 20, void, glVertexAttrib4Niv, GLuint index, const GLint *v);
GL_FUNCTION(gl_version >= 20, void, glVertexAttrib4Nsv, GLuint index, const GLshort *v);
GL_FUNCTION(gl_version >= 20, void, glVertexAttrib4Nub, GLuint index, GLubyte x, GLubyte y,
            GLubyte z, GLubyte w);
GL_FUNCTION(gl_version >= 20, void, glVertexAttrib4Nubv, GLuint index, const GLubyte *v);
GL_FUNCTION(gl_version >= 20, void, glVertexAttrib4Nuiv, GLuint index, const GLuint *v);
GL_FUNCTION(gl_version >= 20, void, glVertexAttrib4Nusv, GLuint index, const GLushort *v);
GL_FUNCTION(gl_version >= 20, void, glVertexAttrib4bv, GLuint index, const GLbyte *v);
GL_FUNCTION(gl_version >= 20, void, glVertexAttrib4d, GLuint index, GLdouble x, GLdouble y,
            GLdouble z, GLdouble w);
GL_FUNCTION(gl_version >= 20, void, glVertexAttrib4dv, GLuint index, const GLdouble *v);
GL_FUNCTION(gl_version >= 20, void, glVertexAttrib4f, GLuint index, GLfloat x, GLfloat y,
            GLfloat z, GLfloat w);
GL_FUNCTION(gl_version >= 20, void, glVertexAttrib4fv, GLuint index, const GLfloat *v);
GL_FUNCTION(gl_version >= 20, void, glVertexAttrib4iv, GLuint index, const GLint *v);
GL_FUNCTION(gl_version >= 20, void, glVertexAttrib4s, GLuint index, GLshort x, GLshort y,
            GLshort z, GLshort w);
GL_FUNCTION(gl_version >= 20, void, glVertexAttrib4sv, GLuint index, const GLshort *v);
GL_FUNCTION(gl_version >= 20, void, glVertexAttrib4ubv, GLuint index, const GLubyte *v);
GL_FUNCTION(gl_version >= 20, void, glVertexAttrib4uiv, GLuint index, const GLuint *v);
GL_FUNCTION(gl_version >= 20, void, glVertexAttrib4usv, GLuint index, const GLushort *v);
GL_FUNCTION(gl_version >= 20, void, glVertexAttribPointer, GLuint index, GLint size, GLenum type,
            GLboolean normalized, GLsizei stride, const void *pointer);

// OpenGL 2.1

GL_FUNCTION(gl_version >= 21, void, glUniformMatrix2x3fv, GLint location, GLsizei count,
            GLboolean transpose, const GLfloat *value);
GL_FUNCTION(gl_version >= 21, void, glUniformMatrix2x4fv, GLint location, GLsizei count,
            GLboolean transpose, const GLfloat *value);
GL_FUNCTION(gl_version >= 21, void, glUniformMatrix3x2fv, GLint location, GLsizei count,
            GLboolean transpose, const GLfloat *value);
GL_FUNCTION(gl_version >= 21, void, glUniformMatrix3x4fv, GLint location, GLsizei count,
            GLboolean transpose, const GLfloat *value);
GL_FUNCTION(gl_version >= 21, void, glUniformMatrix4x2fv, GLint location, GLsizei count,
            GLboolean transpose, const GLfloat *value);
GL_FUNCTION(gl_version >= 21, void, glUniformMatrix4x3fv, GLint location, GLsizei count,
            GLboolean transpose, const GLfloat *value);

// OpenGL 3.0

GL_FUNCTION(gl_version >= 30, void, glBeginConditionalRender, GLuint id, GLenum mode);
GL_FUNCTION(gl_version >= 30, void, glBeginTransformFeedback, GLenum primitiveMode);
GL_FUNCTION(gl_version >= 30, void, glBindBufferBase, GLenum target, GLuint index, GLuint buffer);
GL_FUNCTION(gl_version >= 30, void, glBindBufferRange, GLenum target, GLuint index, GLuint buffer,
            GLintptr offset, GLsizeiptr size);
GL_FUNCTION(gl_version >= 30, void, glBindFragDataLocation, GLuint program, GLuint color,
            const GLchar *name);
GL_FUNCTION(gl_version >= 30, void, glBindFramebuffer, GLenum target, GLuint framebuffer);
GL_FUNCTION(gl_version >= 30, void, glBindRenderbuffer, GLenum target, GLuint renderbuffer);
GL_FUNCTION(gl_version >= 30, void, glBindVertexArray, GLuint array);
GL_FUNCTION(gl_version >= 30, void, glBlitFramebuffer, GLint srcX0, GLint srcY0, GLint srcX1,
            GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask,
            GLenum filter);
GL_FUNCTION(gl_version >= 30, GLenum, glCheckFramebufferStatus, GLenum target);
GL_FUNCTION(gl_version >= 30, void, glClampColor, GLenum target, GLenum clamp);
GL_FUNCTION(gl_version >= 30, void, glClearBufferfi, GLenum buffer, GLint drawbuffer,
            GLfloat depth, GLint stencil);
GL_FUNCTION(gl_version >= 30, void, glClearBufferfv, GLenum buffer, GLint drawbuffer,
            const GLfloat *value);
GL_FUNCTION(gl_version >= 30, void, glClearBufferiv, GLenum buffer, GLint drawbuffer,
            const GLint *value);
GL_FUNCTION(gl_version >= 30, void, glClearBufferuiv, GLenum buffer, GLint drawbuffer,
            const GLuint *value);
GL_FUNCTION(gl_version >= 30, void, glColorMaski, GLuint index, GLboolean r, GLboolean g,
            GLboolean b, GLboolean a);
GL_FUNCTION(gl_version >= 30, void, glDeleteFramebuffers, GLsizei n, const GLuint *framebuffers);
GL_FUNCTION(gl_version >= 30, void, glDeleteRenderbuffers, GLsizei n, const GLuint *renderbuffers);
GL_FUNCTION(gl_version >= 30, void, glDeleteVertexArrays, GLsizei n, const GLuint *arrays);
GL_FUNCTION(gl_version >= 30, void, glDisablei, GLenum target, GLuint index);
GL_FUNCTION(gl_version >= 30, void, glEnablei, GLenum target, GLuint index);
GL_FUNCTION(gl_version >= 30, void, glEndConditionalRender, void);
GL_FUNCTION(gl_version >= 30, void, glEndTransformFeedback, void);
GL_FUNCTION(gl_version >= 30, void, glFlushMappedBufferRange, GLenum target, GLintptr offset,
            GLsizeiptr length);
GL_FUNCTION(gl_version >= 30, void, glFramebufferRenderbuffer, GLenum target, GLenum attachment,
            GLenum renderbuffertarget, GLuint renderbuffer);
GL_FUNCTION(gl_version >= 30, void, glFramebufferTexture1D, GLenum target, GLenum attachment,
            GLenum textarget, GLuint texture, GLint level);
GL_FUNCTION(gl_version >= 30, void, glFramebufferTexture2D, GLenum target, GLenum attachment,
            GLenum textarget, GLuint texture, GLint level);
GL_FUNCTION(gl_version >= 30, void, glFramebufferTexture3D, GLenum target, GLenum attachment,
            GLenum textarget, GLuint texture, GLint level, GLint zoffset);
GL_FUNCTION(gl_version >= 30, void, glFramebufferTextureLayer, GLenum target, GLenum attachment,
            GLuint texture, GLint level, GLint layer);
GL_FUNCTION(gl_version >= 30, void, glGenFramebuffers, GLsizei n, GLuint *framebuffers);
GL_FUNCTION(gl_version >= 30, void, glGenRenderbuffers, GLsizei n, GLuint *renderbuffers);
GL_FUNCTION(gl_version >= 30, void, glGenVertexArrays, GLsizei n, GLuint *arrays);
GL_FUNCTION(gl_version >= 30, void, glGenerateMipmap, GLenum target);
GL_FUNCTION(gl_version >= 30, void, glGetBooleani_v, GLenum target, GLuint index, GLboolean *data);
GL_FUNCTION(gl_version >= 30, GLint, glGetFragDataLocation, GLuint program, const GLchar *name);
GL_FUNCTION(gl_version >= 30, void, glGetFramebufferAttachmentParameteriv, GLenum target,
            GLenum attachment, GLenum pname, GLint *params);
GL_FUNCTION(gl_version >= 30, void, glGetIntegeri_v, GLenum target, GLuint index, GLint *data);
GL_FUNCTION(gl_version >= 30, void, glGetRenderbufferParameteriv, GLenum target, GLenum pname,
            GLint *params);
GL_FUNCTION(gl_version >= 30, const GLubyte *, glGetStringi, GLenum name, GLuint index);
GL_FUNCTION(gl_version >= 30, void, glGetTexParameterIiv, GLenum target, GLenum pname,
            GLint *params);
GL_FUNCTION(gl_version >= 30, void, glGetTexParameterIuiv, GLenum target, GLenum pname,
            GLuint *params);
GL_FUNCTION(gl_version >= 30, void, glGetTransformFeedbackVarying, GLuint program, GLuint index,
            GLsizei bufSize, GLsizei *length, GLsizei *size, GLenum *type, GLchar *name);
GL_FUNCTION(gl_version >= 30, void, glGetUniformuiv, GLuint program, GLint location,
            GLuint *params);
GL_FUNCTION(gl_version >= 30, void, glGetVertexAttribIiv, GLuint index, GLenum pname,
            GLint *params);
GL_FUNCTION(gl_version >= 30, void, glGetVertexAttribIuiv, GLuint index, GLenum pname,
            GLuint *params);
GL_FUNCTION(gl_version >= 30, GLboolean, glIsEnabledi, GLenum target, GLuint index);
GL_FUNCTION(gl_version >= 30, GLboolean, glIsFramebuffer, GLuint framebuffer);
GL_FUNCTION(gl_version >= 30, GLboolean, glIsRenderbuffer, GLuint renderbuffer);
GL_FUNCTION(gl_version >= 30, GLboolean, glIsVertexArray, GLuint array);
GL_FUNCTION(gl_version >= 30, void *, glMapBufferRange, GLenum target, GLintptr offset,
            GLsizeiptr length, GLbitfield access);
GL_FUNCTION(gl_version >= 30, void, glRenderbufferStorage, GLenum target, GLenum internalformat,
            GLsizei width, GLsizei height);
GL_FUNCTION(gl_version >= 30, void, glRenderbufferStorageMultisample, GLenum target,
            GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height);
GL_FUNCTION(gl_version >= 30, void, glTexParameterIiv, GLenum target, GLenum pname,
            const GLint *params);
GL_FUNCTION(gl_version >= 30, void, glTexParameterIuiv, GLenum target, GLenum pname,
            const GLuint *params);
GL_FUNCTION(gl_version >= 30, void, glTransformFeedbackVaryings, GLuint program, GLsizei count,
            const GLchar *const *varyings, GLenum bufferMode);
GL_FUNCTION(gl_version >= 30, void, glUniform1ui, GLint location, GLuint v0);
GL_FUNCTION(gl_version >= 30, void, glUniform1uiv, GLint location, GLsizei count,
            const GLuint *value);
GL_FUNCTION(gl_version >= 30, void, glUniform2ui, GLint location, GLuint v0, GLuint v1);
GL_FUNCTION(gl_version >= 30, void, glUniform2uiv, GLint location, GLsizei count,
            const GLuint *value);
GL_FUNCTION(gl_version >= 30, void, glUniform3ui, GLint location, GLuint v0, GLuint v1, GLuint v2);
GL_FUNCTION(gl_version >= 30, void, glUniform3uiv, GLint location, GLsizei count,
            const GLuint *value);
GL_FUNCTION(gl_version >= 30, void, glUniform4ui, GLint location, GLuint v0, GLuint v1,
            GLuint v2, GLuint v3);
GL_FUNCTION(gl_version >= 30, void, glUniform4uiv, GLint location, GLsizei count,
            const GLuint *value);
GL_FUNCTION(gl_version >= 30, void, glVertexAttribI1i, GLuint index, GLint x);
GL_FUNCTION(gl_version >= 30, void, glVertexAttribI1iv, GLuint index, const GLint *v);
GL_FUNCTION(gl_version >= 30, void, glVertexAttribI1ui, GLuint index, GLuint x);
GL_FUNCTION(gl_version >= 30, void, glVertexAttribI1uiv, GLuint index, const GLuint *v);
GL_FUNCTION(gl_version >= 30, void, glVertexAttribI2i, GLuint index, GLint x, GLint y);
GL_FUNCTION(gl_version >= 30, void, glVertexAttribI2iv, GLuint index, const GLint *v);
GL_FUNCTION(gl_version >= 30, void, glVertexAttribI2ui, GLuint index, GLuint x, GLuint y);
GL_FUNCTION(gl_version >= 30, void, glVertexAttribI2uiv, GLuint index, const GLuint *v);
GL_FUNCTION(gl_version >= 30, void, glVertexAttribI3i, GLuint index, GLint x, GLint y, GLint z);
GL_FUNCTION(gl_version >= 30, void, glVertexAttribI3iv, GLuint index, const GLint *v);
GL_FUNCTION(gl_version >= 30, void, glVertexAttribI3ui, GLuint index, GLuint x, GLuint y,
            GLuint z);
GL_FUNCTION(gl_version >= 30, void, glVertexAttribI3uiv, GLuint index, const GLuint *v);
GL_FUNCTION(gl_version >= 30, void, glVertexAttribI4bv, GLuint index, const GLbyte *v);
GL_FUNCTION(gl_version >= 30, void, glVertexAttribI4i, GLuint index, GLint x, GLint y,
            GLint z, GLint w);
GL_FUNCTION(gl_version >= 30, void, glVertexAttribI4iv, GLuint index, const GLint *v);
GL_FUNCTION(gl_version >= 30, void, glVertexAttribI4sv, GLuint index, const GLshort *v);
GL_FUNCTION(gl_version >= 30, void, glVertexAttribI4ubv, GLuint index, const GLubyte *v);
GL_FUNCTION(gl_version >= 30, void, glVertexAttribI4ui, GLuint index, GLuint x, GLuint y,
            GLuint z, GLuint w);
GL_FUNCTION(gl_version >= 30, void, glVertexAttribI4uiv, GLuint index, const GLuint *v);
GL_FUNCTION(gl_version >= 30, void, glVertexAttribI4usv, GLuint index, const GLushort *v);
GL_FUNCTION(gl_version >= 30, void, glVertexAttribIPointer, GLuint index, GLint size, GLenum type,
            GLsizei stride, const void *pointer);

// OpenGL 3.1

GL_FUNCTION(gl_version >= 31, void, glCopyBufferSubData, GLenum readTarget, GLenum writeTarget,
            GLintptr readOffset, GLintptr writeOffset, GLsizeiptr size);
GL_FUNCTION(gl_version >= 31, void, glDrawArraysInstanced, GLenum mode, GLint first, GLsizei count,
            GLsizei instancecount);
GL_FUNCTION(gl_version >= 31, void, glDrawElementsInstanced, GLenum mode, GLsizei count,
            GLenum type, const void *indices, GLsizei instancecount);
GL_FUNCTION(gl_version >= 31, void, glGetActiveUniformBlockName, GLuint program,
            GLuint uniformBlockIndex, GLsizei bufSize, GLsizei *length, GLchar *uniformBlockName);
GL_FUNCTION(gl_version >= 31, void, glGetActiveUniformBlockiv, GLuint program,
            GLuint uniformBlockIndex, GLenum pname, GLint *params);
GL_FUNCTION(gl_version >= 31, void, glGetActiveUniformName, GLuint program, GLuint uniformIndex,
            GLsizei bufSize, GLsizei *length, GLchar *uniformName);
GL_FUNCTION(gl_version >= 31, void, glGetActiveUniformsiv, GLuint program, GLsizei uniformCount,
            const GLuint *uniformIndices, GLenum pname, GLint *params);
GL_FUNCTION(gl_version >= 31, GLuint, glGetUniformBlockIndex, GLuint program,
            const GLchar *uniformBlockName);
GL_FUNCTION(gl_version >= 31, void, glGetUniformIndices, GLuint program, GLsizei uniformCount,
            const GLchar *const *uniformNames, GLuint *uniformIndices);
GL_FUNCTION(gl_version >= 31, void, glPrimitiveRestartIndex, GLuint index);
GL_FUNCTION(gl_version >= 31, void, glTexBuffer, GLenum target, GLenum internalformat,
            GLuint buffer);
GL_FUNCTION(gl_version >= 31, void, glUniformBlockBinding, GLuint program,
            GLuint uniformBlockIndex, GLuint uniformBlockBinding);

// OpenGL 3.2

GL_FUNCTION(gl_version >= 32, GLenum, glClientWaitSync, GLsync sync, GLbitfield flags,
            GLuint64 timeout);
GL_FUNCTION(gl_version >= 32, void, glDeleteSync, GLsync sync);
GL_FUNCTION(gl_version >= 32, void, glDrawElementsBaseVertex, GLenum mode, GLsizei count,
            GLenum type, const void *indices, GLint basevertex);
GL_FUNCTION(gl_version >= 32, void, glDrawElementsInstancedBaseVertex, GLenum mode, GLsizei count,
            GLenum type, const void *indices, GLsizei instancecount, GLint basevertex);
GL_FUNCTION(gl_version >= 32, void, glDrawRangeElementsBaseVertex, GLenum mode, GLuint start,
            GLuint end, GLsizei count, GLenum type, const void *indices, GLint basevertex);
GL_FUNCTION(gl_version >= 32, GLsync, glFenceSync, GLenum condition, GLbitfield flags);
GL_FUNCTION(gl_version >= 32, void, glFramebufferTexture, GLenum target, GLenum attachment,
            GLuint texture, GLint level);
GL_FUNCTION(gl_version >= 32, void, glGetBufferParameteri64v, GLenum target, GLenum pname,
            GLint64 *params);
GL_FUNCTION(gl_version >= 32, void, glGetInteger64i_v, GLenum target, GLuint index, GLint64 *data);
GL_FUNCTION(gl_version >= 32, void, glGetInteger64v, GLenum pname, GLint64 *data);
GL_FUNCTION(gl_version >= 32, void, glGetMultisamplefv, GLenum pname, GLuint index, GLfloat *val);
GL_FUNCTION(gl_version >= 32, void, glGetSynciv, GLsync sync, GLenum pname, GLsizei bufSize,
            GLsizei *length, GLint *values);
GL_FUNCTION(gl_version >= 32, GLboolean, glIsSync, GLsync sync);
GL_FUNCTION(gl_version >= 32, void, glMultiDrawElementsBaseVertex, GLenum mode,
            const GLsizei *count, GLenum type, const void *const *indices, GLsizei drawcount,
            const GLint *basevertex);
GL_FUNCTION(gl_version >= 32, void, glProvokingVertex, GLenum mode);
GL_FUNCTION(gl_version >= 32, void, glSampleMaski, GLuint maskNumber, GLbitfield mask);
GL_FUNCTION(gl_version >= 32, void, glTexImage2DMultisample, GLenum target, GLsizei samples,
            GLenum internalformat, GLsizei width, GLsizei height,
            GLboolean fixedsamplelocations);
GL_FUNCTION(gl_version >= 32, void, glTexImage3DMultisample, GLenum target, GLsizei samples,
            GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth,
            GLboolean fixedsamplelocations);
GL_FUNCTION(gl_version >= 32, void, glWaitSync, GLsync sync, GLbitfield flags, GLuint64 timeout);

// OpenGL 3.3

GL_FUNCTION(gl_version >= 33, void, glBindFragDataLocationIndexed, GLuint program,
            GLuint colorNumber, GLuint index, const GLchar *name);
GL_FUNCTION(gl_version >= 33, void, glBindSampler, GLuint unit, GLuint sampler);
#ifndef GL_NO_COMPAT
GL_FUNCTION(gl_version >= 33 && gl_compat, void, glColorP3ui, GLenum type, GLuint color);
GL_FUNCTION(gl_version >= 33 && gl_compat, void, glColorP3uiv, GLenum type, const GLuint *color);
GL_FUNCTION(gl_version >= 33 && gl_compat, void, glColorP4ui, GLenum type, GLuint color);
GL_FUNCTION(gl_version >= 33 && gl_compat, void, glColorP4uiv, GLenum type, const GLuint *color);
#endif
GL_FUNCTION(gl_version >= 33, void, glDeleteSamplers, GLsizei count, const GLuint *samplers);
GL_FUNCTION(gl_version >= 33, void, glGenSamplers, GLsizei count, GLuint *samplers);
GL_FUNCTION(gl_version >= 33, GLint, glGetFragDataIndex, GLuint program, const GLchar *name);
GL_FUNCTION(gl_version >= 33, void, glGetQueryObjecti64v, GLuint id, GLenum pname,
            GLint64 *params);
GL_FUNCTION(gl_version >= 33, void, glGetQueryObjectui64v, GLuint id, GLenum pname,
            GLuint64 *params);
GL_FUNCTION(gl_version >= 33, void, glGetSamplerParameterIiv, GLuint sampler, GLenum pname,
            GLint *params);
GL_FUNCTION(gl_version >= 33, void, glGetSamplerParameterIuiv, GLuint sampler, GLenum pname,
            GLuint *params);
GL_FUNCTION(gl_version >= 33, void, glGetSamplerParameterfv, GLuint sampler, GLenum pname,
            GLfloat *params);
GL_FUNCTION(gl_version >= 33, void, glGetSamplerParameteriv, GLuint sampler, GLenum pname,
            GLint *params);
GL_FUNCTION(gl_version >= 33, GLboolean, glIsSampler, GLuint sampler);
#ifndef GL_NO_COMPAT
GL_FUNCTION(gl_version >= 33 && gl_compat, void, glMultiTexCoordP1ui, GLenum texture, GLenum type,
            GLuint coords);
GL_FUNCTION(gl_version >= 33 && gl_compat, void, glMultiTexCoordP1uiv, GLenum texture, GLenum type,
            const GLuint *coords);
GL_FUNCTION(gl_version >= 33 && gl_compat, void, glMultiTexCoordP2ui, GLenum texture, GLenum type,
            GLuint coords);
GL_FUNCTION(gl_version >= 33 && gl_compat, void, glMultiTexCoordP2uiv, GLenum texture, GLenum type,
            const GLuint *coords);
GL_FUNCTION(gl_version >= 33 && gl_compat, void, glMultiTexCoordP3ui, GLenum texture, GLenum type,
            GLuint coords);
GL_FUNCTION(gl_version >= 33 && gl_compat, void, glMultiTexCoordP3uiv, GLenum texture, GLenum type,
            const GLuint *coords);
GL_FUNCTION(gl_version >= 33 && gl_compat, void, glMultiTexCoordP4ui, GLenum texture, GLenum type,
            GLuint coords);
GL_FUNCTION(gl_version >= 33 && gl_compat, void, glMultiTexCoordP4uiv, GLenum texture, GLenum type,
            const GLuint *coords);
GL_FUNCTION(gl_version >= 33 && gl_compat, void, glNormalP3ui, GLenum type, GLuint coords);
GL_FUNCTION(gl_version >= 33 && gl_compat, void, glNormalP3uiv, GLenum type, const GLuint *coords);
#endif
GL_FUNCTION(gl_version >= 33, void, glQueryCounter, GLuint id, GLenum target);
GL_FUNCTION(gl_version >= 33, void, glSamplerParameterIiv, GLuint sampler, GLenum pname,
            const GLint *param);
GL_FUNCTION(gl_version >= 33, void, glSamplerParameterIuiv, GLuint sampler, GLenum pname,
            const GLuint *param);
GL_FUNCTION(gl_version >= 33, void, glSamplerParameterf, GLuint sampler, GLenum pname,
            GLfloat param);
GL_FUNCTION(gl_version >= 33, void, glSamplerParameterfv, GLuint sampler, GLenum pname,
            const GLfloat *param);
GL_FUNCTION(gl_version >= 33, void, glSamplerParameteri, GLuint sampler, GLenum pname,
            GLint param);
GL_FUNCTION(gl_version >= 33, void, glSamplerParameteriv, GLuint sampler, GLenum pname,
            const GLint *param);
#ifndef GL_NO_COMPAT
GL_FUNCTION(gl_version >= 33 && gl_compat, void, glSecondaryColorP3ui, GLenum type, GLuint color);
GL_FUNCTION(gl_version >= 33 && gl_compat, void, glSecondaryColorP3uiv, GLenum type,
            const GLuint *color);
GL_FUNCTION(gl_version >= 33 && gl_compat, void, glTexCoordP1ui, GLenum type, GLuint coords);
GL_FUNCTION(gl_version >= 33 && gl_compat, void, glTexCoordP1uiv, GLenum type,
            const GLuint *coords);
GL_FUNCTION(gl_version >= 33 && gl_compat, void, glTexCoordP2ui, GLenum type, GLuint coords);
GL_FUNCTION(gl_version >= 33 && gl_compat, void, glTexCoordP2uiv, GLenum type,
            const GLuint *coords);
GL_FUNCTION(gl_version >= 33 && gl_compat, void, glTexCoordP3ui, GLenum type, GLuint coords);
GL_FUNCTION(gl_version >= 33 && gl_compat, void, glTexCoordP3uiv, GLenum type,
            const GLuint *coords);
GL_FUNCTION(gl_version >= 33 && gl_compat, void, glTexCoordP4ui, GLenum type, GLuint coords);
GL_FUNCTION(gl_version >= 33 && gl_compat, void, glTexCoordP4uiv, GLenum type,
            const GLuint *coords);
#endif
GL_FUNCTION(gl_version >= 33, void, glVertexAttribDivisor, GLuint index, GLuint divisor);
GL_FUNCTION(gl_version >= 33, void, glVertexAttribP1ui, GLuint index, GLenum type,
            GLboolean normalized, GLuint value);
GL_FUNCTION(gl_version >= 33, void, glVertexAttribP1uiv, GLuint index, GLenum type,
            GLboolean normalized, const GLuint *value);
GL_FUNCTION(gl_version >= 33, void, glVertexAttribP2ui, GLuint index, GLenum type,
            GLboolean normalized, GLuint value);
GL_FUNCTION(gl_version >= 33, void, glVertexAttribP2uiv, GLuint index, GLenum type,
            GLboolean normalized, const GLuint *value);
GL_FUNCTION(gl_version >= 33, void, glVertexAttribP3ui, GLuint index, GLenum type,
            GLboolean normalized, GLuint value);
GL_FUNCTION(gl_version >= 33, void, glVertexAttribP3uiv, GLuint index, GLenum type,
            GLboolean normalized, const GLuint *value);
GL_FUNCTION(gl_version >= 33, void, glVertexAttribP4ui, GLuint index, GLenum type,
            GLboolean normalized, GLuint value);
GL_FUNCTION(gl_version >= 33, void, glVertexAttribP4uiv, GLuint index, GLenum type,
            GLboolean normalized, const GLuint *value);
#ifndef GL_NO_COMPAT
GL_FUNCTION(gl_version >= 33 && gl_compat, void, glVertexP2ui, GLenum type, GLuint value);
GL_FUNCTION(gl_version >= 33 && gl_compat, void, glVertexP2uiv, GLenum type, const GLuint *value);
GL_FUNCTION(gl_version >= 33 && gl_compat, void, glVertexP3ui, GLenum type, GLuint value);
GL_FUNCTION(gl_version >= 33 && gl_compat, void, glVertexP3uiv, GLenum type, const GLuint *value);
GL_FUNCTION(gl_version >= 33 && gl_compat, void, glVertexP4ui, GLenum type, GLuint value);
GL_FUNCTION(gl_version >= 33 && gl_compat, void, glVertexP4uiv, GLenum type, const GLuint *value);
#endif

// OpenGL 4.0

GL_FUNCTION(gl_version >= 40, void, glBeginQueryIndexed, GLenum target, GLuint index, GLuint id);
GL_FUNCTION(gl_version >= 40, void, glBindTransformFeedback, GLenum target, GLuint id);
GL_FUNCTION(gl_version >= 40, void, glBlendEquationSeparatei, GLuint buf, GLenum modeRGB,
            GLenum modeAlpha);
GL_FUNCTION(gl_version >= 40, void, glBlendEquationi, GLuint buf, GLenum mode);
GL_FUNCTION(gl_version >= 40, void, glBlendFuncSeparatei, GLuint buf, GLenum srcRGB, GLenum dstRGB,
            GLenum srcAlpha, GLenum dstAlpha);
GL_FUNCTION(gl_version >= 40, void, glBlendFunci, GLuint buf, GLenum src, GLenum dst);
GL_FUNCTION(gl_version >= 40, void, glDeleteTransformFeedbacks, GLsizei n, const GLuint *ids);
GL_FUNCTION(gl_version >= 40, void, glDrawArraysIndirect, GLenum mode, const void *indirect);
GL_FUNCTION(gl_version >= 40, void, glDrawElementsIndirect, GLenum mode, GLenum type,
            const void *indirect);
GL_FUNCTION(gl_version >= 40, void, glDrawTransformFeedback, GLenum mode, GLuint id);
GL_FUNCTION(gl_version >= 40, void, glDrawTransformFeedbackStream, GLenum mode, GLuint id,
            GLuint stream);
GL_FUNCTION(gl_version >= 40, void, glEndQueryIndexed, GLenum target, GLuint index);
GL_FUNCTION(gl_version >= 40, void, glGenTransformFeedbacks, GLsizei n, GLuint *ids);
GL_FUNCTION(gl_version >= 40, void, glGetActiveSubroutineName, GLuint program, GLenum shadertype,
            GLuint index, GLsizei bufsize, GLsizei *length, GLchar *name);
GL_FUNCTION(gl_version >= 40, void, glGetActiveSubroutineUniformName, GLuint program,
            GLenum shadertype, GLuint index, GLsizei bufsize, GLsizei *length, GLchar *name);
GL_FUNCTION(gl_version >= 40, void, glGetActiveSubroutineUniformiv, GLuint program,
            GLenum shadertype, GLuint index, GLenum pname, GLint *values);
GL_FUNCTION(gl_version >= 40, void, glGetProgramStageiv, GLuint program, GLenum shadertype,
            GLenum pname, GLint *values);
GL_FUNCTION(gl_version >= 40, void, glGetQueryIndexediv, GLenum target, GLuint index, GLenum pname,
            GLint *params);
GL_FUNCTION(gl_version >= 40, GLuint, glGetSubroutineIndex, GLuint program, GLenum shadertype,
            const GLchar *name);
GL_FUNCTION(gl_version >= 40, GLint, glGetSubroutineUniformLocation, GLuint program,
            GLenum shadertype, const GLchar *name);
GL_FUNCTION(gl_version >= 40, void, glGetUniformSubroutineuiv, GLenum shadertype, GLint location,
            GLuint *params);
GL_FUNCTION(gl_version >= 40, void, glGetUniformdv, GLuint program, GLint location,
            GLdouble *params);
GL_FUNCTION(gl_version >= 40, GLboolean, glIsTransformFeedback, GLuint id);
GL_FUNCTION(gl_version >= 40, void, glMinSampleShading, GLfloat value);
GL_FUNCTION(gl_version >= 40, void, glPatchParameterfv, GLenum pname, const GLfloat *values);
GL_FUNCTION(gl_version >= 40, void, glPatchParameteri, GLenum pname, GLint value);
GL_FUNCTION(gl_version >= 40, void, glPauseTransformFeedback, void);
GL_FUNCTION(gl_version >= 40, void, glResumeTransformFeedback, void);
GL_FUNCTION(gl_version >= 40, void, glUniform1d, GLint location, GLdouble x);
GL_FUNCTION(gl_version >= 40, void, glUniform1dv, GLint location, GLsizei count,
            const GLdouble *value);
GL_FUNCTION(gl_version >= 40, void, glUniform2d, GLint location, GLdouble x, GLdouble y);
GL_FUNCTION(gl_version >= 40, void, glUniform2dv, GLint location, GLsizei count,
            const GLdouble *value);
GL_FUNCTION(gl_version >= 40, void, glUniform3d, GLint location, GLdouble x, GLdouble y,
            GLdouble z);
GL_FUNCTION(gl_version >= 40, void, glUniform3dv, GLint location, GLsizei count,
            const GLdouble *value);
GL_FUNCTION(gl_version >= 40, void, glUniform4d, GLint location, GLdouble x, GLdouble y,
            GLdouble z, GLdouble w);
GL_FUNCTION(gl_version >= 40, void, glUniform4dv, GLint location, GLsizei count,
            const GLdouble *value);
GL_FUNCTION(gl_version >= 40, void, glUniformMatrix2dv, GLint location, GLsizei count,
            GLboolean transpose, const GLdouble *value);
GL_FUNCTION(gl_version >= 40, void, glUniformMatrix2x3dv, GLint location, GLsizei count,
            GLboolean transpose, const GLdouble *value);
GL_FUNCTION(gl_version >= 40, void, glUniformMatrix2x4dv, GLint location, GLsizei count,
            GLboolean transpose, const GLdouble *value);
GL_FUNCTION(gl_version >= 40, void, glUniformMatrix3dv, GLint location, GLsizei count,
            GLboolean transpose, const GLdouble *value);
GL_FUNCTION(gl_version >= 40, void, glUniformMatrix3x2dv, GLint location, GLsizei count,
            GLboolean transpose, const GLdouble *value);
GL_FUNCTION(gl_version >= 40, void, glUniformMatrix3x4dv, GLint location, GLsizei count,
            GLboolean transpose, const GLdouble *value);
GL_FUNCTION(gl_version >= 40, void, glUniformMatrix4dv, GLint location, GLsizei count,
            GLboolean transpose, const GLdouble *value);
GL_FUNCTION(gl_version >= 40, void, glUniformMatrix4x2dv, GLint location, GLsizei count,
            GLboolean transpose, const GLdouble *value);
GL_FUNCTION(gl_version >= 40, void, glUniformMatrix4x3dv, GLint location, GLsizei count,
            GLboolean transpose, const GLdouble *value);
GL_FUNCTION(gl_version >= 40, void, glUniformSubroutinesuiv, GLenum shadertype, GLsizei count,
            const GLuint *indices);

// OpenGL 4.1

GL_FUNCTION(gl_version >= 41, void, glActiveShaderProgram, GLuint pipeline, GLuint program);
GL_FUNCTION(gl_version >= 41, void, glBindProgramPipeline, GLuint pipeline);
GL_FUNCTION(gl_version >= 41, void, glClearDepthf, GLfloat d);
GL_FUNCTION(gl_version >= 41, GLuint, glCreateShaderProgramv, GLenum type, GLsizei count,
            const GLchar *const *strings);
GL_FUNCTION(gl_version >= 41, void, glDeleteProgramPipelines, GLsizei n, const GLuint *pipelines);
GL_FUNCTION(gl_version >= 41, void, glDepthRangeArrayv, GLuint first, GLsizei count,
            const GLdouble *v);
GL_FUNCTION(gl_version >= 41, void, glDepthRangeIndexed, GLuint index, GLdouble n, GLdouble f);
GL_FUNCTION(gl_version >= 41, void, glDepthRangef, GLfloat n, GLfloat f);
GL_FUNCTION(gl_version >= 41, void, glGenProgramPipelines, GLsizei n, GLuint *pipelines);
GL_FUNCTION(gl_version >= 41, void, glGetDoublei_v, GLenum target, GLuint index, GLdouble *data);
GL_FUNCTION(gl_version >= 41, void, glGetFloati_v, GLenum target, GLuint index, GLfloat *data);
GL_FUNCTION(gl_version >= 41, void, glGetProgramBinary, GLuint program, GLsizei bufSize,
            GLsizei *length, GLenum *binaryFormat, void *binary);
GL_FUNCTION(gl_version >= 41, void, glGetProgramPipelineInfoLog, GLuint pipeline, GLsizei bufSize,
            GLsizei *length, GLchar *infoLog);
GL_FUNCTION(gl_version >= 41, void, glGetProgramPipelineiv, GLuint pipeline, GLenum pname,
            GLint *params);
GL_FUNCTION(gl_version >= 41, void, glGetShaderPrecisionFormat, GLenum shadertype,
            GLenum precisiontype, GLint *range, GLint *precision);
GL_FUNCTION(gl_version >= 41, void, glGetVertexAttribLdv, GLuint index, GLenum pname,
            GLdouble *params);
GL_FUNCTION(gl_version >= 41, GLboolean, glIsProgramPipeline, GLuint pipeline);
GL_FUNCTION(gl_version >= 41, void, glProgramBinary, GLuint program, GLenum binaryFormat,
            const void *binary, GLsizei length);
GL_FUNCTION(gl_version >= 41, void, glProgramParameteri, GLuint program, GLenum pname,
            GLint value);
GL_FUNCTION(gl_version >= 41, void, glProgramUniform1d, GLuint program, GLint location,
            GLdouble v0);
GL_FUNCTION(gl_version >= 41, void, glProgramUniform1dv, GLuint program, GLint location,
            GLsizei count, const GLdouble *value);
GL_FUNCTION(gl_version >= 41, void, glProgramUniform1f, GLuint program, GLint location,
            GLfloat v0);
GL_FUNCTION(gl_version >= 41, void, glProgramUniform1fv, GLuint program, GLint location,
            GLsizei count, const GLfloat *value);
GL_FUNCTION(gl_version >= 41, void, glProgramUniform1i, GLuint program, GLint location, GLint v0);
GL_FUNCTION(gl_version >= 41, void, glProgramUniform1iv, GLuint program, GLint location,
            GLsizei count, const GLint *value);
GL_FUNCTION(gl_version >= 41, void, glProgramUniform1ui, GLuint program, GLint location,
            GLuint v0);
GL_FUNCTION(gl_version >= 41, void, glProgramUniform1uiv, GLuint program, GLint location,
            GLsizei count, const GLuint *value);
GL_FUNCTION(gl_version >= 41, void, glProgramUniform2d, GLuint program, GLint location,
            GLdouble v0, GLdouble v1);
GL_FUNCTION(gl_version >= 41, void, glProgramUniform2dv, GLuint program, GLint location,
            GLsizei count, const GLdouble *value);
GL_FUNCTION(gl_version >= 41, void, glProgramUniform2f, GLuint program, GLint location,
            GLfloat v0, GLfloat v1);
GL_FUNCTION(gl_version >= 41, void, glProgramUniform2fv, GLuint program, GLint location,
            GLsizei count, const GLfloat *value);
GL_FUNCTION(gl_version >= 41, void, glProgramUniform2i, GLuint program, GLint location,
            GLint v0, GLint v1);
GL_FUNCTION(gl_version >= 41, void, glProgramUniform2iv, GLuint program, GLint location,
            GLsizei count, const GLint *value);
GL_FUNCTION(gl_version >= 41, void, glProgramUniform2ui, GLuint program, GLint location,
            GLuint v0, GLuint v1);
GL_FUNCTION(gl_version >= 41, void, glProgramUniform2uiv, GLuint program, GLint location,
            GLsizei count, const GLuint *value);
GL_FUNCTION(gl_version >= 41, void, glProgramUniform3d, GLuint program, GLint location,
            GLdouble v0, GLdouble v1, GLdouble v2);
GL_FUNCTION(gl_version >= 41, void, glProgramUniform3dv, GLuint program, GLint location,
            GLsizei count, const GLdouble *value);
GL_FUNCTION(gl_version >= 41, void, glProgramUniform3f, GLuint program, GLint location,
            GLfloat v0, GLfloat v1, GLfloat v2);
GL_FUNCTION(gl_version >= 41, void, glProgramUniform3fv, GLuint program, GLint location,
            GLsizei count, const GLfloat *value);
GL_FUNCTION(gl_version >= 41, void, glProgramUniform3i, GLuint program, GLint location,
            GLint v0, GLint v1, GLint v2);
GL_FUNCTION(gl_version >= 41, void, glProgramUniform3iv, GLuint program, GLint location,
            GLsizei count, const GLint *value);
GL_FUNCTION(gl_version >= 41, void, glProgramUniform3ui, GLuint program, GLint location,
            GLuint v0, GLuint v1, GLuint v2);
GL_FUNCTION(gl_version >= 41, void, glProgramUniform3uiv, GLuint program, GLint location,
            GLsizei count, const GLuint *value);
GL_FUNCTION(gl_version >= 41, void, glProgramUniform4d, GLuint program, GLint location,
            GLdouble v0, GLdouble v1, GLdouble v2, GLdouble v3);
GL_FUNCTION(gl_version >= 41, void, glProgramUniform4dv, GLuint program, GLint location,
            GLsizei count, const GLdouble *value);
GL_FUNCTION(gl_version >= 41, void, glProgramUniform4f, GLuint program, GLint location,
            GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
GL_FUNCTION(gl_version >= 41, void, glProgramUniform4fv, GLuint program, GLint location,
            GLsizei count, const GLfloat *value);
GL_FUNCTION(gl_version >= 41, void, glProgramUniform4i, GLuint program, GLint location,
            GLint v0, GLint v1, GLint v2, GLint v3);
GL_FUNCTION(gl_version >= 41, void, glProgramUniform4iv, GLuint program, GLint location,
            GLsizei count, const GLint *value);
GL_FUNCTION(gl_version >= 41, void, glProgramUniform4ui, GLuint program, GLint location,
            GLuint v0, GLuint v1, GLuint v2, GLuint v3);
GL_FUNCTION(gl_version >= 41, void, glProgramUniform4uiv, GLuint program, GLint location,
            GLsizei count, const GLuint *value);
GL_FUNCTION(gl_version >= 41, void, glProgramUniformMatrix2dv, GLuint program, GLint location,
            GLsizei count, GLboolean transpose, const GLdouble *value);
GL_FUNCTION(gl_version >= 41, void, glProgramUniformMatrix2fv, GLuint program, GLint location,
            GLsizei count, GLboolean transpose, const GLfloat *value);
GL_FUNCTION(gl_version >= 41, void, glProgramUniformMatrix2x3dv, GLuint program, GLint location,
            GLsizei count, GLboolean transpose, const GLdouble *value);
GL_FUNCTION(gl_version >= 41, void, glProgramUniformMatrix2x3fv, GLuint program, GLint location,
            GLsizei count, GLboolean transpose, const GLfloat *value);
GL_FUNCTION(gl_version >= 41, void, glProgramUniformMatrix2x4dv, GLuint program, GLint location,
            GLsizei count, GLboolean transpose, const GLdouble *value);
GL_FUNCTION(gl_version >= 41, void, glProgramUniformMatrix2x4fv, GLuint program, GLint location,
            GLsizei count, GLboolean transpose, const GLfloat *value);
GL_FUNCTION(gl_version >= 41, void, glProgramUniformMatrix3dv, GLuint program, GLint location,
            GLsizei count, GLboolean transpose, const GLdouble *value);
GL_FUNCTION(gl_version >= 41, void, glProgramUniformMatrix3fv, GLuint program, GLint location,
            GLsizei count, GLboolean transpose, const GLfloat *value);
GL_FUNCTION(gl_version >= 41, void, glProgramUniformMatrix3x2dv, GLuint program, GLint location,
            GLsizei count, GLboolean transpose, const GLdouble *value);
GL_FUNCTION(gl_version >= 41, void, glProgramUniformMatrix3x2fv, GLuint program, GLint location,
            GLsizei count, GLboolean transpose, const GLfloat *value);
GL_FUNCTION(gl_version >= 41, void, glProgramUniformMatrix3x4dv, GLuint program, GLint location,
            GLsizei count, GLboolean transpose, const GLdouble *value);
GL_FUNCTION(gl_version >= 41, void, glProgramUniformMatrix3x4fv, GLuint program, GLint location,
            GLsizei count, GLboolean transpose, const GLfloat *value);
GL_FUNCTION(gl_version >= 41, void, glProgramUniformMatrix4dv, GLuint program, GLint location,
            GLsizei count, GLboolean transpose, const GLdouble *value);
GL_FUNCTION(gl_version >= 41, void, glProgramUniformMatrix4fv, GLuint program, GLint location,
            GLsizei count, GLboolean transpose, const GLfloat *value);
GL_FUNCTION(gl_version >= 41, void, glProgramUniformMatrix4x2dv, GLuint program, GLint location,
            GLsizei count, GLboolean transpose, const GLdouble *value);
GL_FUNCTION(gl_version >= 41, void, glProgramUniformMatrix4x2fv, GLuint program, GLint location,
            GLsizei count, GLboolean transpose, const GLfloat *value);
GL_FUNCTION(gl_version >= 41, void, glProgramUniformMatrix4x3dv, GLuint program, GLint location,
            GLsizei count, GLboolean transpose, const GLdouble *value);
GL_FUNCTION(gl_version >= 41, void, glProgramUniformMatrix4x3fv, GLuint program, GLint location,
            GLsizei count, GLboolean transpose, const GLfloat *value);
GL_FUNCTION(gl_version >= 41, void, glReleaseShaderCompiler, void);
GL_FUNCTION(gl_version >= 41, void, glScissorArrayv, GLuint first, GLsizei count, const GLint *v);
GL_FUNCTION(gl_version >= 41, void, glScissorIndexed, GLuint index, GLint left, GLint bottom,
            GLsizei width, GLsizei height);
GL_FUNCTION(gl_version >= 41, void, glScissorIndexedv, GLuint index, const GLint *v);
GL_FUNCTION(gl_version >= 41, void, glShaderBinary, GLsizei count, const GLuint *shaders,
            GLenum binaryformat, const void *binary, GLsizei length);
GL_FUNCTION(gl_version >= 41, void, glUseProgramStages, GLuint pipeline, GLbitfield stages,
            GLuint program);
GL_FUNCTION(gl_version >= 41, void, glValidateProgramPipeline, GLuint pipeline);
GL_FUNCTION(gl_version >= 41, void, glVertexAttribL1d, GLuint index, GLdouble x);
GL_FUNCTION(gl_version >= 41, void, glVertexAttribL1dv, GLuint index, const GLdouble *v);
GL_FUNCTION(gl_version >= 41, void, glVertexAttribL2d, GLuint index, GLdouble x, GLdouble y);
GL_FUNCTION(gl_version >= 41, void, glVertexAttribL2dv, GLuint index, const GLdouble *v);
GL_FUNCTION(gl_version >= 41, void, glVertexAttribL3d, GLuint index, GLdouble x, GLdouble y,
            GLdouble z);
GL_FUNCTION(gl_version >= 41, void, glVertexAttribL3dv, GLuint index, const GLdouble *v);
GL_FUNCTION(gl_version >= 41, void, glVertexAttribL4d, GLuint index, GLdouble x, GLdouble y,
            GLdouble z, GLdouble w);
GL_FUNCTION(gl_version >= 41, void, glVertexAttribL4dv, GLuint index, const GLdouble *v);
GL_FUNCTION(gl_version >= 41, void, glVertexAttribLPointer, GLuint index, GLint size, GLenum type,
            GLsizei stride, const void *pointer);
GL_FUNCTION(gl_version >= 41, void, glViewportArrayv, GLuint first, GLsizei count,
            const GLfloat *v);
GL_FUNCTION(gl_version >= 41, void, glViewportIndexedf, GLuint index, GLfloat x, GLfloat y,
            GLfloat w, GLfloat h);
GL_FUNCTION(gl_version >= 41, void, glViewportIndexedfv, GLuint index, const GLfloat *v);

// OpenGL 4.2

GL_FUNCTION(gl_version >= 42, void, glBindImageTexture, GLuint unit, GLuint texture, GLint level,
            GLboolean layered, GLint layer, GLenum access, GLenum format);
GL_FUNCTION(gl_version >= 42, void, glDrawArraysInstancedBaseInstance, GLenum mode, GLint first,
            GLsizei count, GLsizei instancecount, GLuint baseinstance);
GL_FUNCTION(gl_version >= 42, void, glDrawElementsInstancedBaseInstance, GLenum mode,
            GLsizei count, GLenum type, const void *indices, GLsizei instancecount,
            GLuint baseinstance);
GL_FUNCTION(gl_version >= 42, void, glDrawElementsInstancedBaseVertexBaseInstance, GLenum mode,
            GLsizei count, GLenum type, const void *indices, GLsizei instancecount,
            GLint basevertex, GLuint baseinstance);
GL_FUNCTION(gl_version >= 42, void, glDrawTransformFeedbackInstanced, GLenum mode, GLuint id,
            GLsizei instancecount);
GL_FUNCTION(gl_version >= 42, void, glDrawTransformFeedbackStreamInstanced, GLenum mode, GLuint id,
            GLuint stream, GLsizei instancecount);
GL_FUNCTION(gl_version >= 42, void, glGetActiveAtomicCounterBufferiv, GLuint program,
            GLuint bufferIndex, GLenum pname, GLint *params);
GL_FUNCTION(gl_version >= 42, void, glGetInternalformativ, GLenum target, GLenum internalformat,
            GLenum pname, GLsizei bufSize, GLint *params);
GL_FUNCTION(gl_version >= 42, void, glMemoryBarrier, GLbitfield barriers);
GL_FUNCTION(gl_version >= 42, void, glTexStorage1D, GLenum target, GLsizei levels,
            GLenum internalformat, GLsizei width);
GL_FUNCTION(gl_version >= 42, void, glTexStorage2D, GLenum target, GLsizei levels,
            GLenum internalformat, GLsizei width, GLsizei height);
GL_FUNCTION(gl_version >= 42, void, glTexStorage3D, GLenum target, GLsizei levels,
            GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth);

// OpenGL 4.3

GL_FUNCTION(gl_version >= 43, void, glBindVertexBuffer, GLuint bindingindex, GLuint buffer,
            GLintptr offset, GLsizei stride);
GL_FUNCTION(gl_version >= 43, void, glClearBufferData, GLenum target, GLenum internalformat,
            GLenum format, GLenum type, const void *data);
GL_FUNCTION(gl_version >= 43, void, glClearBufferSubData, GLenum target, GLenum internalformat,
            GLintptr offset, GLsizeiptr size, GLenum format, GLenum type, const void *data);
GL_FUNCTION(gl_version >= 43, void, glCopyImageSubData, GLuint srcName, GLenum srcTarget,
            GLint srcLevel, GLint srcX, GLint srcY, GLint srcZ, GLuint dstName, GLenum dstTarget,
            GLint dstLevel, GLint dstX, GLint dstY, GLint dstZ, GLsizei srcWidth,
            GLsizei srcHeight, GLsizei srcDepth);
GL_FUNCTION(gl_version >= 43, void, glDebugMessageCallback, GLDEBUGPROC callback,
            const void *userParam);
GL_FUNCTION(gl_version >= 43, void, glDebugMessageControl, GLenum source, GLenum type,
            GLenum severity, GLsizei count, const GLuint *ids, GLboolean enabled);
GL_FUNCTION(gl_version >= 43, void, glDebugMessageInsert, GLenum source, GLenum type, GLuint id,
            GLenum severity, GLsizei length, const GLchar *buf);
GL_FUNCTION(gl_version >= 43, void, glDispatchCompute, GLuint num_groups_x, GLuint num_groups_y,
            GLuint num_groups_z);
GL_FUNCTION(gl_version >= 43, void, glDispatchComputeIndirect, GLintptr indirect);
GL_FUNCTION(gl_version >= 43, void, glFramebufferParameteri, GLenum target, GLenum pname,
            GLint param);
GL_FUNCTION(gl_version >= 43, GLuint, glGetDebugMessageLog, GLuint count, GLsizei bufSize,
            GLenum *sources, GLenum *types, GLuint *ids, GLenum *severities, GLsizei *lengths,
            GLchar *messageLog);
GL_FUNCTION(gl_version >= 43, void, glGetFramebufferParameteriv, GLenum target, GLenum pname,
            GLint *params);
GL_FUNCTION(gl_version >= 43, void, glGetInternalformati64v, GLenum target, GLenum internalformat,
            GLenum pname, GLsizei bufSize, GLint64 *params);
GL_FUNCTION(gl_version >= 43, void, glGetObjectLabel, GLenum identifier, GLuint name,
            GLsizei bufSize, GLsizei *length, GLchar *label);
GL_FUNCTION(gl_version >= 43, void, glGetObjectPtrLabel, const void *ptr, GLsizei bufSize,
            GLsizei *length, GLchar *label);
GL_FUNCTION(gl_version >= 43, void, glGetProgramInterfaceiv, GLuint program,
            GLenum programInterface, GLenum pname, GLint *params);
GL_FUNCTION(gl_version >= 43, GLuint, glGetProgramResourceIndex, GLuint program,
            GLenum programInterface, const GLchar *name);
GL_FUNCTION(gl_version >= 43, GLint, glGetProgramResourceLocation, GLuint program,
            GLenum programInterface, const GLchar *name);
GL_FUNCTION(gl_version >= 43, GLint, glGetProgramResourceLocationIndex, GLuint program,
            GLenum programInterface, const GLchar *name);
GL_FUNCTION(gl_version >= 43, void, glGetProgramResourceName, GLuint program,
            GLenum programInterface, GLuint index, GLsizei bufSize, GLsizei *length, GLchar *name);
GL_FUNCTION(gl_version >= 43, void, glGetProgramResourceiv, GLuint program,
            GLenum programInterface, GLuint index, GLsizei propCount, const GLenum *props,
            GLsizei bufSize, GLsizei *length, GLint *params);
GL_FUNCTION(gl_version >= 43, void, glInvalidateBufferData, GLuint buffer);
GL_FUNCTION(gl_version >= 43, void, glInvalidateBufferSubData, GLuint buffer, GLintptr offset,
            GLsizeiptr length);
GL_FUNCTION(gl_version >= 43, void, glInvalidateFramebuffer, GLenum target, GLsizei numAttachments,
            const GLenum *attachments);
GL_FUNCTION(gl_version >= 43, void, glInvalidateSubFramebuffer, GLenum target,
            GLsizei numAttachments, const GLenum *attachments, GLint x, GLint y, GLsizei width,
            GLsizei height);
GL_FUNCTION(gl_version >= 43, void, glInvalidateTexImage, GLuint texture, GLint level);
GL_FUNCTION(gl_version >= 43, void, glInvalidateTexSubImage, GLuint texture, GLint level,
            GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height,
            GLsizei depth);
GL_FUNCTION(gl_version >= 43, void, glMultiDrawArraysIndirect, GLenum mode, const void *indirect,
            GLsizei drawcount, GLsizei stride);
GL_FUNCTION(gl_version >= 43, void, glMultiDrawElementsIndirect, GLenum mode, GLenum type,
            const void *indirect, GLsizei drawcount, GLsizei stride);
GL_FUNCTION(gl_version >= 43, void, glObjectLabel, GLenum identifier, GLuint name, GLsizei length,
            const GLchar *label);
GL_FUNCTION(gl_version >= 43, void, glObjectPtrLabel, const void *ptr, GLsizei length,
            const GLchar *label);
GL_FUNCTION(gl_version >= 43, void, glPopDebugGroup, void);
GL_FUNCTION(gl_version >= 43, void, glPushDebugGroup, GLenum source, GLuint id, GLsizei length,
            const GLchar *message);
GL_FUNCTION(gl_version >= 43, void, glShaderStorageBlockBinding, GLuint program,
            GLuint storageBlockIndex, GLuint storageBlockBinding);
GL_FUNCTION(gl_version >= 43, void, glTexBufferRange, GLenum target, GLenum internalformat,
            GLuint buffer, GLintptr offset, GLsizeiptr size);
GL_FUNCTION(gl_version >= 43, void, glTexStorage2DMultisample, GLenum target, GLsizei samples,
            GLenum internalformat, GLsizei width, GLsizei height, GLboolean fixedsamplelocations);
GL_FUNCTION(gl_version >= 43, void, glTexStorage3DMultisample, GLenum target, GLsizei samples,
            GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth,
            GLboolean fixedsamplelocations);
GL_FUNCTION(gl_version >= 43, void, glTextureView, GLuint texture, GLenum target,
            GLuint origtexture, GLenum internalformat, GLuint minlevel, GLuint numlevels,
            GLuint minlayer, GLuint numlayers);
GL_FUNCTION(gl_version >= 43, void, glVertexAttribBinding, GLuint attribindex,
            GLuint bindingindex);
GL_FUNCTION(gl_version >= 43, void, glVertexAttribFormat, GLuint attribindex, GLint size,
            GLenum type, GLboolean normalized, GLuint relativeoffset);
GL_FUNCTION(gl_version >= 43, void, glVertexAttribIFormat, GLuint attribindex, GLint size,
            GLenum type, GLuint relativeoffset);
GL_FUNCTION(gl_version >= 43, void, glVertexAttribLFormat, GLuint attribindex, GLint size,
            GLenum type, GLuint relativeoffset);
GL_FUNCTION(gl_version >= 43, void, glVertexBindingDivisor, GLuint bindingindex, GLuint divisor);

// OpenGL 4.4

GL_FUNCTION(gl_version >= 44, void, glBindBuffersBase, GLenum target, GLuint first, GLsizei count,
            const GLuint *buffers);
GL_FUNCTION(gl_version >= 44, void, glBindBuffersRange, GLenum target, GLuint first, GLsizei count,
            const GLuint *buffers, const GLintptr *offsets, const GLsizeiptr *sizes);
GL_FUNCTION(gl_version >= 44, void, glBindImageTextures, GLuint first, GLsizei count,
            const GLuint *textures);
GL_FUNCTION(gl_version >= 44, void, glBindSamplers, GLuint first, GLsizei count,
            const GLuint *samplers);
GL_FUNCTION(gl_version >= 44, void, glBindTextures, GLuint first, GLsizei count,
            const GLuint *textures);
GL_FUNCTION(gl_version >= 44, void, glBindVertexBuffers, GLuint first, GLsizei count,
            const GLuint *buffers, const GLintptr *offsets, const GLsizei *strides);
GL_FUNCTION(gl_version >= 44, void, glBufferStorage, GLenum target, GLsizeiptr size,
            const void *data, GLbitfield flags);
GL_FUNCTION(gl_version >= 44, void, glClearTexImage, GLuint texture, GLint level, GLenum format,
            GLenum type, const void *data);
GL_FUNCTION(gl_version >= 44, void, glClearTexSubImage, GLuint texture, GLint level, GLint xoffset,
            GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth,
            GLenum format, GLenum type, const void *data);

// OpenGL 4.5

GL_FUNCTION(gl_version >= 45, void, glBindTextureUnit, GLuint unit, GLuint texture);
GL_FUNCTION(gl_version >= 45, void, glBlitNamedFramebuffer, GLuint readFramebuffer,
            GLuint drawFramebuffer, GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1,
            GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter);
GL_FUNCTION(gl_version >= 45, GLenum, glCheckNamedFramebufferStatus, GLuint framebuffer,
            GLenum target);
GL_FUNCTION(gl_version >= 45, void, glClearNamedBufferData, GLuint buffer, GLenum internalformat,
            GLenum format, GLenum type, const void *data);
GL_FUNCTION(gl_version >= 45, void, glClearNamedBufferSubData, GLuint buffer,
            GLenum internalformat, GLintptr offset, GLsizeiptr size, GLenum format, GLenum type,
            const void *data);
GL_FUNCTION(gl_version >= 45, void, glClearNamedFramebufferfi, GLuint framebuffer, GLenum buffer,
            GLint drawbuffer, GLfloat depth, GLint stencil);
GL_FUNCTION(gl_version >= 45, void, glClearNamedFramebufferfv, GLuint framebuffer, GLenum buffer,
            GLint drawbuffer, const GLfloat *value);
GL_FUNCTION(gl_version >= 45, void, glClearNamedFramebufferiv, GLuint framebuffer, GLenum buffer,
            GLint drawbuffer, const GLint *value);
GL_FUNCTION(gl_version >= 45, void, glClearNamedFramebufferuiv, GLuint framebuffer, GLenum buffer,
            GLint drawbuffer, const GLuint *value);
GL_FUNCTION(gl_version >= 45, void, glClipControl, GLenum origin, GLenum depth);
GL_FUNCTION(gl_version >= 45, void, glCompressedTextureSubImage1D, GLuint texture, GLint level,
            GLint xoffset, GLsizei width, GLenum format, GLsizei imageSize, const void *data);
GL_FUNCTION(gl_version >= 45, void, glCompressedTextureSubImage2D, GLuint texture, GLint level,
            GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format,
            GLsizei imageSize, const void *data);
GL_FUNCTION(gl_version >= 45, void, glCompressedTextureSubImage3D, GLuint texture, GLint level,
            GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height,
            GLsizei depth, GLenum format, GLsizei imageSize, const void *data);
GL_FUNCTION(gl_version >= 45, void, glCopyNamedBufferSubData, GLuint readBuffer,
            GLuint writeBuffer, GLintptr readOffset, GLintptr writeOffset, GLsizeiptr size);
GL_FUNCTION(gl_version >= 45, void, glCopyTextureSubImage1D, GLuint texture, GLint level,
            GLint xoffset, GLint x, GLint y, GLsizei width);
GL_FUNCTION(gl_version >= 45, void, glCopyTextureSubImage2D, GLuint texture, GLint level,
            GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height);
GL_FUNCTION(gl_version >= 45, void, glCopyTextureSubImage3D, GLuint texture, GLint level,
            GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width,
            GLsizei height);
GL_FUNCTION(gl_version >= 45, void, glCreateBuffers, GLsizei n, GLuint *buffers);
GL_FUNCTION(gl_version >= 45, void, glCreateFramebuffers, GLsizei n, GLuint *framebuffers);
GL_FUNCTION(gl_version >= 45, void, glCreateProgramPipelines, GLsizei n, GLuint *pipelines);
GL_FUNCTION(gl_version >= 45, void, glCreateQueries, GLenum target, GLsizei n, GLuint *ids);
GL_FUNCTION(gl_version >= 45, void, glCreateRenderbuffers, GLsizei n, GLuint *renderbuffers);
GL_FUNCTION(gl_version >= 45, void, glCreateSamplers, GLsizei n, GLuint *samplers);
GL_FUNCTION(gl_version >= 45, void, glCreateTextures, GLenum target, GLsizei n, GLuint *textures);
GL_FUNCTION(gl_version >= 45, void, glCreateTransformFeedbacks, GLsizei n, GLuint *ids);
GL_FUNCTION(gl_version >= 45, void, glCreateVertexArrays, GLsizei n, GLuint *arrays);
GL_FUNCTION(gl_version >= 45, void, glDisableVertexArrayAttrib, GLuint vaobj, GLuint index);
GL_FUNCTION(gl_version >= 45, void, glEnableVertexArrayAttrib, GLuint vaobj, GLuint index);
GL_FUNCTION(gl_version >= 45, void, glFlushMappedNamedBufferRange, GLuint buffer, GLintptr offset,
            GLsizeiptr length);
GL_FUNCTION(gl_version >= 45, void, glGenerateTextureMipmap, GLuint texture);
GL_FUNCTION(gl_version >= 45, void, glGetCompressedTextureImage, GLuint texture, GLint level,
            GLsizei bufSize, void *pixels);
GL_FUNCTION(gl_version >= 45, void, glGetCompressedTextureSubImage, GLuint texture, GLint level,
            GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height,
            GLsizei depth, GLsizei bufSize, void *pixels);
GL_FUNCTION(gl_version >= 45, GLenum, glGetGraphicsResetStatus, void);
GL_FUNCTION(gl_version >= 45, void, glGetNamedBufferParameteri64v, GLuint buffer, GLenum pname,
            GLint64 *params);
GL_FUNCTION(gl_version >= 45, void, glGetNamedBufferParameteriv, GLuint buffer, GLenum pname,
            GLint *params);
GL_FUNCTION(gl_version >= 45, void, glGetNamedBufferPointerv, GLuint buffer, GLenum pname,
            void **params);
GL_FUNCTION(gl_version >= 45, void, glGetNamedBufferSubData, GLuint buffer, GLintptr offset,
            GLsizeiptr size, void *data);
GL_FUNCTION(gl_version >= 45, void, glGetNamedFramebufferAttachmentParameteriv, GLuint framebuffer,
            GLenum attachment, GLenum pname, GLint *params);
GL_FUNCTION(gl_version >= 45, void, glGetNamedFramebufferParameteriv, GLuint framebuffer,
            GLenum pname, GLint *param);
GL_FUNCTION(gl_version >= 45, void, glGetNamedRenderbufferParameteriv, GLuint renderbuffer,
            GLenum pname, GLint *params);
GL_FUNCTION(gl_version >= 45, void, glGetQueryBufferObjecti64v, GLuint id, GLuint buffer,
            GLenum pname, GLintptr offset);
GL_FUNCTION(gl_version >= 45, void, glGetQueryBufferObjectiv, GLuint id, GLuint buffer,
            GLenum pname, GLintptr offset);
GL_FUNCTION(gl_version >= 45, void, glGetQueryBufferObjectui64v, GLuint id, GLuint buffer,
            GLenum pname, GLintptr offset);
GL_FUNCTION(gl_version >= 45, void, glGetQueryBufferObjectuiv, GLuint id, GLuint buffer,
            GLenum pname, GLintptr offset);
GL_FUNCTION(gl_version >= 45, void, glGetTextureImage, GLuint texture, GLint level, GLenum format,
            GLenum type, GLsizei bufSize, void *pixels);
GL_FUNCTION(gl_version >= 45, void, glGetTextureLevelParameterfv, GLuint texture, GLint level,
            GLenum pname, GLfloat *params);
GL_FUNCTION(gl_version >= 45, void, glGetTextureLevelParameteriv, GLuint texture, GLint level,
            GLenum pname, GLint *params);
GL_FUNCTION(gl_version >= 45, void, glGetTextureParameterIiv, GLuint texture, GLenum pname,
            GLint *params);
GL_FUNCTION(gl_version >= 45, void, glGetTextureParameterIuiv, GLuint texture, GLenum pname,
            GLuint *params);
GL_FUNCTION(gl_version >= 45, void, glGetTextureParameterfv, GLuint texture, GLenum pname,
            GLfloat *params);
GL_FUNCTION(gl_version >= 45, void, glGetTextureParameteriv, GLuint texture, GLenum pname,
            GLint *params);
GL_FUNCTION(gl_version >= 45, void, glGetTextureSubImage, GLuint texture, GLint level,
            GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height,
            GLsizei depth, GLenum format, GLenum type, GLsizei bufSize, void *pixels);
GL_FUNCTION(gl_version >= 45, void, glGetTransformFeedbacki64_v, GLuint xfb, GLenum pname,
            GLuint index, GLint64 *param);
GL_FUNCTION(gl_version >= 45, void, glGetTransformFeedbacki_v, GLuint xfb, GLenum pname,
            GLuint index, GLint *param);
GL_FUNCTION(gl_version >= 45, void, glGetTransformFeedbackiv, GLuint xfb, GLenum pname,
            GLint *param);
GL_FUNCTION(gl_version >= 45, void, glGetVertexArrayIndexed64iv, GLuint vaobj, GLuint index,
            GLenum pname, GLint64 *param);
GL_FUNCTION(gl_version >= 45, void, glGetVertexArrayIndexediv, GLuint vaobj, GLuint index,
            GLenum pname, GLint *param);
GL_FUNCTION(gl_version >= 45, void, glGetVertexArrayiv, GLuint vaobj, GLenum pname, GLint *param);
GL_FUNCTION(gl_version >= 45, void, glGetnCompressedTexImage, GLenum target, GLint lod,
            GLsizei bufSize, void *pixels);
GL_FUNCTION(gl_version >= 45, void, glGetnTexImage, GLenum target, GLint level, GLenum format,
            GLenum type, GLsizei bufSize, void *pixels);
GL_FUNCTION(gl_version >= 45, void, glGetnUniformdv, GLuint program, GLint location,
            GLsizei bufSize, GLdouble *params);
GL_FUNCTION(gl_version >= 45, void, glGetnUniformfv, GLuint program, GLint location,
            GLsizei bufSize, GLfloat *params);
GL_FUNCTION(gl_version >= 45, void, glGetnUniformiv, GLuint program, GLint location,
            GLsizei bufSize, GLint *params);
GL_FUNCTION(gl_version >= 45, void, glGetnUniformuiv, GLuint program, GLint location,
            GLsizei bufSize, GLuint *params);
GL_FUNCTION(gl_version >= 45, void, glInvalidateNamedFramebufferData, GLuint framebuffer,
            GLsizei numAttachments, const GLenum *attachments);
GL_FUNCTION(gl_version >= 45, void, glInvalidateNamedFramebufferSubData, GLuint framebuffer,
            GLsizei numAttachments, const GLenum *attachments, GLint x, GLint y, GLsizei width,
            GLsizei height);
GL_FUNCTION(gl_version >= 45, void *, glMapNamedBuffer, GLuint buffer, GLenum access);
GL_FUNCTION(gl_version >= 45, void *, glMapNamedBufferRange, GLuint buffer, GLintptr offset,
            GLsizeiptr length, GLbitfield access);
GL_FUNCTION(gl_version >= 45, void, glMemoryBarrierByRegion, GLbitfield barriers);
GL_FUNCTION(gl_version >= 45, void, glNamedBufferData, GLuint buffer, GLsizeiptr size,
            const void *data, GLenum usage);
GL_FUNCTION(gl_version >= 45, void, glNamedBufferStorage, GLuint buffer, GLsizeiptr size,
            const void *data, GLbitfield flags);
GL_FUNCTION(gl_version >= 45, void, glNamedBufferSubData, GLuint buffer, GLintptr offset,
            GLsizeiptr size, const void *data);
GL_FUNCTION(gl_version >= 45, void, glNamedFramebufferDrawBuffer, GLuint framebuffer, GLenum buf);
GL_FUNCTION(gl_version >= 45, void, glNamedFramebufferDrawBuffers, GLuint framebuffer, GLsizei n,
            const GLenum *bufs);
GL_FUNCTION(gl_version >= 45, void, glNamedFramebufferParameteri, GLuint framebuffer, GLenum pname,
            GLint param);
GL_FUNCTION(gl_version >= 45, void, glNamedFramebufferReadBuffer, GLuint framebuffer, GLenum src);
GL_FUNCTION(gl_version >= 45, void, glNamedFramebufferRenderbuffer, GLuint framebuffer,
            GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer);
GL_FUNCTION(gl_version >= 45, void, glNamedFramebufferTexture, GLuint framebuffer,
            GLenum attachment, GLuint texture, GLint level);
GL_FUNCTION(gl_version >= 45, void, glNamedFramebufferTextureLayer, GLuint framebuffer,
            GLenum attachment, GLuint texture, GLint level, GLint layer);
GL_FUNCTION(gl_version >= 45, void, glNamedRenderbufferStorage, GLuint renderbuffer,
            GLenum internalformat, GLsizei width, GLsizei height);
GL_FUNCTION(gl_version >= 45, void, glNamedRenderbufferStorageMultisample, GLuint renderbuffer,
            GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height);
GL_FUNCTION(gl_version >= 45, void, glReadnPixels, GLint x, GLint y, GLsizei width, GLsizei height,
            GLenum format, GLenum type, GLsizei bufSize, void *data);
GL_FUNCTION(gl_version >= 45, void, glTextureBarrier, void);
GL_FUNCTION(gl_version >= 45, void, glTextureBuffer, GLuint texture, GLenum internalformat,
            GLuint buffer);
GL_FUNCTION(gl_version >= 45, void, glTextureBufferRange, GLuint texture, GLenum internalformat,
            GLuint buffer, GLintptr offset, GLsizeiptr size);
GL_FUNCTION(gl_version >= 45, void, glTextureParameterIiv, GLuint texture, GLenum pname,
            const GLint *params);
GL_FUNCTION(gl_version >= 45, void, glTextureParameterIuiv, GLuint texture, GLenum pname,
            const GLuint *params);
GL_FUNCTION(gl_version >= 45, void, glTextureParameterf, GLuint texture, GLenum pname,
            GLfloat param);
GL_FUNCTION(gl_version >= 45, void, glTextureParameterfv, GLuint texture, GLenum pname,
            const GLfloat *param);
GL_FUNCTION(gl_version >= 45, void, glTextureParameteri, GLuint texture, GLenum pname,
            GLint param);
GL_FUNCTION(gl_version >= 45, void, glTextureParameteriv, GLuint texture, GLenum pname,
            const GLint *param);
GL_FUNCTION(gl_version >= 45, void, glTextureStorage1D, GLuint texture, GLsizei levels,
            GLenum internalformat, GLsizei width);
GL_FUNCTION(gl_version >= 45, void, glTextureStorage2D, GLuint texture, GLsizei levels,
            GLenum internalformat, GLsizei width, GLsizei height);
GL_FUNCTION(gl_version >= 45, void, glTextureStorage2DMultisample, GLuint texture, GLsizei samples,
            GLenum internalformat, GLsizei width, GLsizei height,
            GLboolean fixedsamplelocations);
GL_FUNCTION(gl_version >= 45, void, glTextureStorage3D, GLuint texture, GLsizei levels,
            GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth);
GL_FUNCTION(gl_version >= 45, void, glTextureStorage3DMultisample, GLuint texture, GLsizei samples,
            GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth,
            GLboolean fixedsamplelocations);
GL_FUNCTION(gl_version >= 45, void, glTextureSubImage1D, GLuint texture, GLint level,
            GLint xoffset, GLsizei width, GLenum format, GLenum type, const void *pixels);
GL_FUNCTION(gl_version >= 45, void, glTextureSubImage2D, GLuint texture, GLint level,
            GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format,
            GLenum type, const void *pixels);
GL_FUNCTION(gl_version >= 45, void, glTextureSubImage3D, GLuint texture, GLint level,
            GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height,
            GLsizei depth, GLenum format, GLenum type, const void *pixels);
GL_FUNCTION(gl_version >= 45, void, glTransformFeedbackBufferBase, GLuint xfb, GLuint index,
            GLuint buffer);
GL_FUNCTION(gl_version >= 45, void, glTransformFeedbackBufferRange, GLuint xfb, GLuint index,
            GLuint buffer, GLintptr offset, GLsizeiptr size);
GL_FUNCTION(gl_version >= 45, GLboolean, glUnmapNamedBuffer, GLuint buffer);
GL_FUNCTION(gl_version >= 45, void, glVertexArrayAttribBinding, GLuint vaobj, GLuint attribindex,
            GLuint bindingindex);
GL_FUNCTION(gl_version >= 45, void, glVertexArrayAttribFormat, GLuint vaobj, GLuint attribindex,
            GLint size, GLenum type, GLboolean normalized, GLuint relativeoffset);
GL_FUNCTION(gl_version >= 45, void, glVertexArrayAttribIFormat, GLuint vaobj, GLuint attribindex,
            GLint size, GLenum type, GLuint relativeoffset);
GL_FUNCTION(gl_version >= 45, void, glVertexArrayAttribLFormat, GLuint vaobj, GLuint attribindex,
            GLint size, GLenum type, GLuint relativeoffset);
GL_FUNCTION(gl_version >= 45, void, glVertexArrayBindingDivisor, GLuint vaobj, GLuint bindingindex,
            GLuint divisor);
GL_FUNCTION(gl_version >= 45, void, glVertexArrayElementBuffer, GLuint vaobj, GLuint buffer);
GL_FUNCTION(gl_version >= 45, void, glVertexArrayVertexBuffer, GLuint vaobj, GLuint bindingindex,
            GLuint buffer, GLintptr offset, GLsizei stride);
GL_FUNCTION(gl_version >= 45, void, glVertexArrayVertexBuffers, GLuint vaobj, GLuint first,
            GLsizei count, const GLuint *buffers, const GLintptr *offsets, const GLsizei *strides);

#endif

#undef GL_FUNCTION
