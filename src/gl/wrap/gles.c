#include "gles.h"
#include "../gl4es.h"
#include "../loader.h"
#include "skips.h"

// OPTIMIZATION: Branch prediction hints for ARMv8
#ifndef likely
#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)
#endif

// emulation of 'glTexParameteri' (for internal use only)
void gles_glTexParameteri(glTexParameteri_ARG_EXPAND)
{
    LOAD_GLES(glTexParameteri); 
    if (likely(gles_glTexParameteri)) return gles_glTexParameteri(target, pname, param);

    LOAD_GLES(glTexParameterx);
    // Fallback usually unlikely
    if (likely(gles_glTexParameterx)) return gles_glTexParameterx(target, pname, param);
}

// emulation of 'glGetBooleanv' (for internal use only)
void gles_glGetBooleanv(glGetBooleanv_ARG_EXPAND)
{
    LOAD_GLES(glGetBooleanv); 
    if (likely(gles_glGetBooleanv)) return gles_glGetBooleanv(pname, params);

    LOAD_GLES(glGetIntegerv);
    GLint result = GL_FALSE;
    if (likely(gles_glGetIntegerv)) {
        gles_glGetIntegerv(pname, &result);
        if (params) *params = (result == GL_TRUE ? GL_TRUE : GL_FALSE);
    }
}

// emulation of 'glIsEnabled' (for internal use only)
GLboolean gles_glIsEnabled(glIsEnabled_ARG_EXPAND)
{
    LOAD_GLES(glIsEnabled); 
    if (likely(gles_glIsEnabled)) return gles_glIsEnabled(cap);

    GLboolean result = GL_FALSE;
    gles_glGetBooleanv(cap, &result);
    return result;
}

#ifndef skip_glActiveTexture
void APIENTRY_GL4ES gl4es_glActiveTexture(GLenum texture) {
    LOAD_GLES(glActiveTexture);
#ifndef direct_glActiveTexture
    PUSH_IF_COMPILING(glActiveTexture)
#endif
    if(likely(gles_glActiveTexture)) gles_glActiveTexture(texture);
}
AliasExport(void,glActiveTexture,,(GLenum texture));
#endif

#ifndef skip_glAlphaFunc
void APIENTRY_GL4ES gl4es_glAlphaFunc(GLenum func, GLclampf ref) {
    LOAD_GLES(glAlphaFunc);
#ifndef direct_glAlphaFunc
    PUSH_IF_COMPILING(glAlphaFunc)
#endif
    if(likely(gles_glAlphaFunc)) gles_glAlphaFunc(func, ref);
}
AliasExport(void,glAlphaFunc,,(GLenum func, GLclampf ref));
#endif

#ifndef skip_glAlphaFuncx
void APIENTRY_GL4ES gl4es_glAlphaFuncx(GLenum func, GLclampx ref) {
    LOAD_GLES(glAlphaFuncx);
#ifndef direct_glAlphaFuncx
    PUSH_IF_COMPILING(glAlphaFuncx)
#endif
    if(likely(gles_glAlphaFuncx)) gles_glAlphaFuncx(func, ref);
}
AliasExport(void,glAlphaFuncx,,(GLenum func, GLclampx ref));
#endif

#ifndef skip_glAttachShader
void APIENTRY_GL4ES gl4es_glAttachShader(GLuint program, GLuint shader) {
    LOAD_GLES(glAttachShader);
#ifndef direct_glAttachShader
    PUSH_IF_COMPILING(glAttachShader)
#endif
    if(likely(gles_glAttachShader)) gles_glAttachShader(program, shader);
}
AliasExport(void,glAttachShader,,(GLuint program, GLuint shader));
#endif

#ifndef skip_glBindAttribLocation
void APIENTRY_GL4ES gl4es_glBindAttribLocation(GLuint program, GLuint index, const GLchar * name) {
    LOAD_GLES(glBindAttribLocation);
#ifndef direct_glBindAttribLocation
    PUSH_IF_COMPILING(glBindAttribLocation)
#endif
    if(likely(gles_glBindAttribLocation)) gles_glBindAttribLocation(program, index, name);
}
AliasExport(void,glBindAttribLocation,,(GLuint program, GLuint index, const GLchar * name));
#endif

#ifndef skip_glBindBuffer
void APIENTRY_GL4ES gl4es_glBindBuffer(GLenum target, GLuint buffer) {
    LOAD_GLES(glBindBuffer);
#ifndef direct_glBindBuffer
    PUSH_IF_COMPILING(glBindBuffer)
#endif
    if(likely(gles_glBindBuffer)) gles_glBindBuffer(target, buffer);
}
AliasExport(void,glBindBuffer,,(GLenum target, GLuint buffer));
#endif

#ifndef skip_glBindFramebuffer
void APIENTRY_GL4ES gl4es_glBindFramebuffer(GLenum target, GLuint framebuffer) {
    LOAD_GLES_OES(glBindFramebuffer);
#ifndef direct_glBindFramebuffer
    PUSH_IF_COMPILING(glBindFramebuffer)
#endif
    if(likely(gles_glBindFramebuffer)) gles_glBindFramebuffer(target, framebuffer);
}
AliasExport(void,glBindFramebuffer,,(GLenum target, GLuint framebuffer));
#endif

#ifndef skip_glBindRenderbuffer
void APIENTRY_GL4ES gl4es_glBindRenderbuffer(GLenum target, GLuint renderbuffer) {
    LOAD_GLES_OES(glBindRenderbuffer);
#ifndef direct_glBindRenderbuffer
    PUSH_IF_COMPILING(glBindRenderbuffer)
#endif
    if(likely(gles_glBindRenderbuffer)) gles_glBindRenderbuffer(target, renderbuffer);
}
AliasExport(void,glBindRenderbuffer,,(GLenum target, GLuint renderbuffer));
#endif

#ifndef skip_glBindTexture
void APIENTRY_GL4ES gl4es_glBindTexture(GLenum target, GLuint texture) {
    LOAD_GLES(glBindTexture);
#ifndef direct_glBindTexture
    PUSH_IF_COMPILING(glBindTexture)
#endif
    if(likely(gles_glBindTexture)) gles_glBindTexture(target, texture);
}
AliasExport(void,glBindTexture,,(GLenum target, GLuint texture));
#endif
#ifndef skip_glBlendColor
void APIENTRY_GL4ES gl4es_glBlendColor(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha) {
    LOAD_GLES_OES(glBlendColor);
#ifndef direct_glBlendColor
    PUSH_IF_COMPILING(glBlendColor)
#endif
    if(likely(gles_glBlendColor)) gles_glBlendColor(red, green, blue, alpha);
}
AliasExport(void,glBlendColor,,(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha));
#endif

#ifndef skip_glBlendEquation
void APIENTRY_GL4ES gl4es_glBlendEquation(GLenum mode) {
    LOAD_GLES_OES(glBlendEquation);
#ifndef direct_glBlendEquation
    PUSH_IF_COMPILING(glBlendEquation)
#endif
    if(likely(gles_glBlendEquation)) gles_glBlendEquation(mode);
}
AliasExport(void,glBlendEquation,,(GLenum mode));
#endif

#ifndef skip_glBlendEquationSeparate
void APIENTRY_GL4ES gl4es_glBlendEquationSeparate(GLenum modeRGB, GLenum modeA) {
    LOAD_GLES_OES(glBlendEquationSeparate);
#ifndef direct_glBlendEquationSeparate
    PUSH_IF_COMPILING(glBlendEquationSeparate)
#endif
    if(likely(gles_glBlendEquationSeparate)) gles_glBlendEquationSeparate(modeRGB, modeA);
}
AliasExport(void,glBlendEquationSeparate,,(GLenum modeRGB, GLenum modeA));
#endif

#ifndef skip_glBlendFunc
void APIENTRY_GL4ES gl4es_glBlendFunc(GLenum sfactor, GLenum dfactor) {
    LOAD_GLES(glBlendFunc);
#ifndef direct_glBlendFunc
    PUSH_IF_COMPILING(glBlendFunc)
#endif
    if(likely(gles_glBlendFunc)) gles_glBlendFunc(sfactor, dfactor);
}
AliasExport(void,glBlendFunc,,(GLenum sfactor, GLenum dfactor));
#endif

#ifndef skip_glBlendFuncSeparate
void APIENTRY_GL4ES gl4es_glBlendFuncSeparate(GLenum sfactorRGB, GLenum dfactorRGB, GLenum sfactorAlpha, GLenum dfactorAlpha) {
    LOAD_GLES_OES(glBlendFuncSeparate);
#ifndef direct_glBlendFuncSeparate
    PUSH_IF_COMPILING(glBlendFuncSeparate)
#endif
    if(likely(gles_glBlendFuncSeparate)) gles_glBlendFuncSeparate(sfactorRGB, dfactorRGB, sfactorAlpha, dfactorAlpha);
}
AliasExport(void,glBlendFuncSeparate,,(GLenum sfactorRGB, GLenum dfactorRGB, GLenum sfactorAlpha, GLenum dfactorAlpha));
#endif

#ifndef skip_glBufferData
void APIENTRY_GL4ES gl4es_glBufferData(GLenum target, GLsizeiptr size, const GLvoid * data, GLenum usage) {
    LOAD_GLES(glBufferData);
#ifndef direct_glBufferData
    PUSH_IF_COMPILING(glBufferData)
#endif
    if(likely(gles_glBufferData)) gles_glBufferData(target, size, data, usage);
}
AliasExport(void,glBufferData,,(GLenum target, GLsizeiptr size, const GLvoid * data, GLenum usage));
#endif

#ifndef skip_glBufferSubData
void APIENTRY_GL4ES gl4es_glBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, const GLvoid * data) {
    LOAD_GLES(glBufferSubData);
#ifndef direct_glBufferSubData
    PUSH_IF_COMPILING(glBufferSubData)
#endif
    if(likely(gles_glBufferSubData)) gles_glBufferSubData(target, offset, size, data);
}
AliasExport(void,glBufferSubData,,(GLenum target, GLintptr offset, GLsizeiptr size, const GLvoid * data));
#endif

#ifndef skip_glCheckFramebufferStatus
GLenum APIENTRY_GL4ES gl4es_glCheckFramebufferStatus(GLenum target) {
    LOAD_GLES_OES(glCheckFramebufferStatus);
#ifndef direct_glCheckFramebufferStatus
    PUSH_IF_COMPILING(glCheckFramebufferStatus)
#endif
    if(likely(gles_glCheckFramebufferStatus)) return gles_glCheckFramebufferStatus(target);
    return 0;
}
AliasExport(GLenum,glCheckFramebufferStatus,,(GLenum target));
#endif

#ifndef skip_glClear
void APIENTRY_GL4ES gl4es_glClear(GLbitfield mask) {
    LOAD_GLES(glClear);
#ifndef direct_glClear
    PUSH_IF_COMPILING(glClear)
#endif
    if(likely(gles_glClear)) gles_glClear(mask);
}
AliasExport(void,glClear,,(GLbitfield mask));
#endif

#ifndef skip_glClearColor
void APIENTRY_GL4ES gl4es_glClearColor(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha) {
    LOAD_GLES(glClearColor);
#ifndef direct_glClearColor
    PUSH_IF_COMPILING(glClearColor)
#endif
    if(likely(gles_glClearColor)) gles_glClearColor(red, green, blue, alpha);
}
AliasExport(void,glClearColor,,(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha));
#endif

#ifndef skip_glClearColorx
void APIENTRY_GL4ES gl4es_glClearColorx(GLclampx red, GLclampx green, GLclampx blue, GLclampx alpha) {
    LOAD_GLES(glClearColorx);
#ifndef direct_glClearColorx
    PUSH_IF_COMPILING(glClearColorx)
#endif
    if(likely(gles_glClearColorx)) gles_glClearColorx(red, green, blue, alpha);
}
AliasExport(void,glClearColorx,,(GLclampx red, GLclampx green, GLclampx blue, GLclampx alpha));
#endif

#ifndef skip_glClearDepthf
void APIENTRY_GL4ES gl4es_glClearDepthf(GLclampf depth) {
    LOAD_GLES(glClearDepthf);
#ifndef direct_glClearDepthf
    PUSH_IF_COMPILING(glClearDepthf)
#endif
    if(likely(gles_glClearDepthf)) gles_glClearDepthf(depth);
}
AliasExport(void,glClearDepthf,,(GLclampf depth));
#endif

#ifndef skip_glClearDepthx
void APIENTRY_GL4ES gl4es_glClearDepthx(GLclampx depth) {
    LOAD_GLES(glClearDepthx);
#ifndef direct_glClearDepthx
    PUSH_IF_COMPILING(glClearDepthx)
#endif
    if(likely(gles_glClearDepthx)) gles_glClearDepthx(depth);
}
AliasExport(void,glClearDepthx,,(GLclampx depth));
#endif

#ifndef skip_glClearStencil
void APIENTRY_GL4ES gl4es_glClearStencil(GLint s) {
    LOAD_GLES(glClearStencil);
#ifndef direct_glClearStencil
    PUSH_IF_COMPILING(glClearStencil)
#endif
    if(likely(gles_glClearStencil)) gles_glClearStencil(s);
}
AliasExport(void,glClearStencil,,(GLint s));
#endif

#ifndef skip_glClientActiveTexture
void APIENTRY_GL4ES gl4es_glClientActiveTexture(GLenum texture) {
    LOAD_GLES(glClientActiveTexture);
#ifndef direct_glClientActiveTexture
    PUSH_IF_COMPILING(glClientActiveTexture)
#endif
    if(likely(gles_glClientActiveTexture)) gles_glClientActiveTexture(texture);
}
AliasExport(void,glClientActiveTexture,,(GLenum texture));
#endif

#ifndef skip_glClipPlanef
void APIENTRY_GL4ES gl4es_glClipPlanef(GLenum plane, const GLfloat * equation) {
    LOAD_GLES(glClipPlanef);
#ifndef direct_glClipPlanef
    PUSH_IF_COMPILING(glClipPlanef)
#endif
    if(likely(gles_glClipPlanef)) gles_glClipPlanef(plane, equation);
}
AliasExport(void,glClipPlanef,,(GLenum plane, const GLfloat * equation));
#endif

#ifndef skip_glClipPlanex
void APIENTRY_GL4ES gl4es_glClipPlanex(GLenum plane, const GLfixed * equation) {
    LOAD_GLES(glClipPlanex);
#ifndef direct_glClipPlanex
    PUSH_IF_COMPILING(glClipPlanex)
#endif
    if(likely(gles_glClipPlanex)) gles_glClipPlanex(plane, equation);
}
AliasExport(void,glClipPlanex,,(GLenum plane, const GLfixed * equation));
#endif

#ifndef skip_glColor4f
void APIENTRY_GL4ES gl4es_glColor4f(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha) {
    LOAD_GLES(glColor4f);
#ifndef direct_glColor4f
    PUSH_IF_COMPILING(glColor4f)
#endif
    if(likely(gles_glColor4f)) gles_glColor4f(red, green, blue, alpha);
}
AliasExport(void,glColor4f,,(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha));
#endif

#ifndef skip_glColor4ub
void APIENTRY_GL4ES gl4es_glColor4ub(GLubyte red, GLubyte green, GLubyte blue, GLubyte alpha) {
    LOAD_GLES(glColor4ub);
#ifndef direct_glColor4ub
    PUSH_IF_COMPILING(glColor4ub)
#endif
    if(likely(gles_glColor4ub)) gles_glColor4ub(red, green, blue, alpha);
}
AliasExport(void,glColor4ub,,(GLubyte red, GLubyte green, GLubyte blue, GLubyte alpha));
#endif

#ifndef skip_glColor4x
void APIENTRY_GL4ES gl4es_glColor4x(GLfixed red, GLfixed green, GLfixed blue, GLfixed alpha) {
    LOAD_GLES(glColor4x);
#ifndef direct_glColor4x
    PUSH_IF_COMPILING(glColor4x)
#endif
    if(likely(gles_glColor4x)) gles_glColor4x(red, green, blue, alpha);
}
AliasExport(void,glColor4x,,(GLfixed red, GLfixed green, GLfixed blue, GLfixed alpha));
#endif

#ifndef skip_glColorMask
void APIENTRY_GL4ES gl4es_glColorMask(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha) {
    LOAD_GLES(glColorMask);
#ifndef direct_glColorMask
    PUSH_IF_COMPILING(glColorMask)
#endif
    if(likely(gles_glColorMask)) gles_glColorMask(red, green, blue, alpha);
}
AliasExport(void,glColorMask,,(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha));
#endif

#ifndef skip_glColorPointer
void APIENTRY_GL4ES gl4es_glColorPointer(GLint size, GLenum type, GLsizei stride, const GLvoid * pointer) {
    LOAD_GLES(glColorPointer);
#ifndef direct_glColorPointer
    PUSH_IF_COMPILING(glColorPointer)
#endif
    if(likely(gles_glColorPointer)) gles_glColorPointer(size, type, stride, pointer);
}
AliasExport(void,glColorPointer,,(GLint size, GLenum type, GLsizei stride, const GLvoid * pointer));
#endif
#ifndef skip_glCompileShader
void APIENTRY_GL4ES gl4es_glCompileShader(GLuint shader) {
    LOAD_GLES(glCompileShader);
#ifndef direct_glCompileShader
    PUSH_IF_COMPILING(glCompileShader)
#endif
    if(likely(gles_glCompileShader)) gles_glCompileShader(shader);
}
AliasExport(void,glCompileShader,,(GLuint shader));
#endif

#ifndef skip_glCompressedTexImage2D
void APIENTRY_GL4ES gl4es_glCompressedTexImage2D(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const GLvoid * data) {
    LOAD_GLES(glCompressedTexImage2D);
#ifndef direct_glCompressedTexImage2D
    PUSH_IF_COMPILING(glCompressedTexImage2D)
#endif
    if(likely(gles_glCompressedTexImage2D)) gles_glCompressedTexImage2D(target, level, internalformat, width, height, border, imageSize, data);
}
AliasExport(void,glCompressedTexImage2D,,(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const GLvoid * data));
#endif

#ifndef skip_glCompressedTexSubImage2D
void APIENTRY_GL4ES gl4es_glCompressedTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const GLvoid * data) {
    LOAD_GLES(glCompressedTexSubImage2D);
#ifndef direct_glCompressedTexSubImage2D
    PUSH_IF_COMPILING(glCompressedTexSubImage2D)
#endif
    if(likely(gles_glCompressedTexSubImage2D)) gles_glCompressedTexSubImage2D(target, level, xoffset, yoffset, width, height, format, imageSize, data);
}
AliasExport(void,glCompressedTexSubImage2D,,(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const GLvoid * data));
#endif

#ifndef skip_glCopyTexImage2D
void APIENTRY_GL4ES gl4es_glCopyTexImage2D(GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border) {
    LOAD_GLES(glCopyTexImage2D);
#ifndef direct_glCopyTexImage2D
    PUSH_IF_COMPILING(glCopyTexImage2D)
#endif
    if(likely(gles_glCopyTexImage2D)) gles_glCopyTexImage2D(target, level, internalformat, x, y, width, height, border);
}
AliasExport(void,glCopyTexImage2D,,(GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border));
#endif

#ifndef skip_glCopyTexSubImage2D
void APIENTRY_GL4ES gl4es_glCopyTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height) {
    LOAD_GLES(glCopyTexSubImage2D);
#ifndef direct_glCopyTexSubImage2D
    PUSH_IF_COMPILING(glCopyTexSubImage2D)
#endif
    if(likely(gles_glCopyTexSubImage2D)) gles_glCopyTexSubImage2D(target, level, xoffset, yoffset, x, y, width, height);
}
AliasExport(void,glCopyTexSubImage2D,,(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height));
#endif

#ifndef skip_glCreateProgram
GLuint APIENTRY_GL4ES gl4es_glCreateProgram() {
    LOAD_GLES(glCreateProgram);
#ifndef direct_glCreateProgram
    PUSH_IF_COMPILING(glCreateProgram)
#endif
    if(likely(gles_glCreateProgram)) return gles_glCreateProgram();
    return 0;
}
AliasExport(GLuint,glCreateProgram,,());
#endif

#ifndef skip_glCreateShader
GLuint APIENTRY_GL4ES gl4es_glCreateShader(GLenum type) {
    LOAD_GLES(glCreateShader);
#ifndef direct_glCreateShader
    PUSH_IF_COMPILING(glCreateShader)
#endif
    if(likely(gles_glCreateShader)) return gles_glCreateShader(type);
    return 0;
}
AliasExport(GLuint,glCreateShader,,(GLenum type));
#endif

#ifndef skip_glCullFace
void APIENTRY_GL4ES gl4es_glCullFace(GLenum mode) {
    LOAD_GLES(glCullFace);
#ifndef direct_glCullFace
    PUSH_IF_COMPILING(glCullFace)
#endif
    if(likely(gles_glCullFace)) gles_glCullFace(mode);
}
AliasExport(void,glCullFace,,(GLenum mode));
#endif

#ifndef skip_glDeleteBuffers
void APIENTRY_GL4ES gl4es_glDeleteBuffers(GLsizei n, const GLuint * buffer) {
    LOAD_GLES(glDeleteBuffers);
#ifndef direct_glDeleteBuffers
    PUSH_IF_COMPILING(glDeleteBuffers)
#endif
    if(likely(gles_glDeleteBuffers)) gles_glDeleteBuffers(n, buffer);
}
AliasExport(void,glDeleteBuffers,,(GLsizei n, const GLuint * buffer));
#endif

#ifndef skip_glDeleteFramebuffers
void APIENTRY_GL4ES gl4es_glDeleteFramebuffers(GLsizei n, GLuint * framebuffers) {
    LOAD_GLES_OES(glDeleteFramebuffers);
#ifndef direct_glDeleteFramebuffers
    PUSH_IF_COMPILING(glDeleteFramebuffers)
#endif
    if(likely(gles_glDeleteFramebuffers)) gles_glDeleteFramebuffers(n, framebuffers);
}
AliasExport(void,glDeleteFramebuffers,,(GLsizei n, GLuint * framebuffers));
#endif

#ifndef skip_glDeleteProgram
void APIENTRY_GL4ES gl4es_glDeleteProgram(GLuint program) {
    LOAD_GLES(glDeleteProgram);
#ifndef direct_glDeleteProgram
    PUSH_IF_COMPILING(glDeleteProgram)
#endif
    if(likely(gles_glDeleteProgram)) gles_glDeleteProgram(program);
}
AliasExport(void,glDeleteProgram,,(GLuint program));
#endif

#ifndef skip_glDeleteRenderbuffers
void APIENTRY_GL4ES gl4es_glDeleteRenderbuffers(GLsizei n, GLuint * renderbuffers) {
    LOAD_GLES_OES(glDeleteRenderbuffers);
#ifndef direct_glDeleteRenderbuffers
    PUSH_IF_COMPILING(glDeleteRenderbuffers)
#endif
    if(likely(gles_glDeleteRenderbuffers)) gles_glDeleteRenderbuffers(n, renderbuffers);
}
AliasExport(void,glDeleteRenderbuffers,,(GLsizei n, GLuint * renderbuffers));
#endif

#ifndef skip_glDeleteShader
void APIENTRY_GL4ES gl4es_glDeleteShader(GLuint shader) {
    LOAD_GLES(glDeleteShader);
#ifndef direct_glDeleteShader
    PUSH_IF_COMPILING(glDeleteShader)
#endif
    if(likely(gles_glDeleteShader)) gles_glDeleteShader(shader);
}
AliasExport(void,glDeleteShader,,(GLuint shader));
#endif

#ifndef skip_glDeleteTextures
void APIENTRY_GL4ES gl4es_glDeleteTextures(GLsizei n, const GLuint * textures) {
    LOAD_GLES(glDeleteTextures);
#ifndef direct_glDeleteTextures
    PUSH_IF_COMPILING(glDeleteTextures)
#endif
    if(likely(gles_glDeleteTextures)) gles_glDeleteTextures(n, textures);
}
AliasExport(void,glDeleteTextures,,(GLsizei n, const GLuint * textures));
#endif

#ifndef skip_glDepthFunc
void APIENTRY_GL4ES gl4es_glDepthFunc(GLenum func) {
    LOAD_GLES(glDepthFunc);
#ifndef direct_glDepthFunc
    PUSH_IF_COMPILING(glDepthFunc)
#endif
    if(likely(gles_glDepthFunc)) gles_glDepthFunc(func);
}
AliasExport(void,glDepthFunc,,(GLenum func));
#endif

#ifndef skip_glDepthMask
void APIENTRY_GL4ES gl4es_glDepthMask(GLboolean flag) {
    LOAD_GLES(glDepthMask);
#ifndef direct_glDepthMask
    PUSH_IF_COMPILING(glDepthMask)
#endif
    if(likely(gles_glDepthMask)) gles_glDepthMask(flag);
}
AliasExport(void,glDepthMask,,(GLboolean flag));
#endif

#ifndef skip_glDepthRangef
void APIENTRY_GL4ES gl4es_glDepthRangef(GLclampf Near, GLclampf Far) {
    LOAD_GLES(glDepthRangef);
#ifndef direct_glDepthRangef
    PUSH_IF_COMPILING(glDepthRangef)
#endif
    if(likely(gles_glDepthRangef)) gles_glDepthRangef(Near, Far);
}
AliasExport(void,glDepthRangef,,(GLclampf Near, GLclampf Far));
#endif

#ifndef skip_glDepthRangex
void APIENTRY_GL4ES gl4es_glDepthRangex(GLclampx Near, GLclampx Far) {
    LOAD_GLES(glDepthRangex);
#ifndef direct_glDepthRangex
    PUSH_IF_COMPILING(glDepthRangex)
#endif
    if(likely(gles_glDepthRangex)) gles_glDepthRangex(Near, Far);
}
AliasExport(void,glDepthRangex,,(GLclampx Near, GLclampx Far));
#endif

#ifndef skip_glDetachShader
void APIENTRY_GL4ES gl4es_glDetachShader(GLuint program, GLuint shader) {
    LOAD_GLES(glDetachShader);
#ifndef direct_glDetachShader
    PUSH_IF_COMPILING(glDetachShader)
#endif
    if(likely(gles_glDetachShader)) gles_glDetachShader(program, shader);
}
AliasExport(void,glDetachShader,,(GLuint program, GLuint shader));
#endif

#ifndef skip_glDisable
void APIENTRY_GL4ES gl4es_glDisable(GLenum cap) {
    LOAD_GLES(glDisable);
#ifndef direct_glDisable
    PUSH_IF_COMPILING(glDisable)
#endif
    if(likely(gles_glDisable)) gles_glDisable(cap);
}
AliasExport(void,glDisable,,(GLenum cap));
#endif

#ifndef skip_glDisableClientState
void APIENTRY_GL4ES gl4es_glDisableClientState(GLenum array) {
    LOAD_GLES(glDisableClientState);
#ifndef direct_glDisableClientState
    PUSH_IF_COMPILING(glDisableClientState)
#endif
    if(likely(gles_glDisableClientState)) gles_glDisableClientState(array);
}
AliasExport(void,glDisableClientState,,(GLenum array));
#endif

#ifndef skip_glDisableVertexAttribArray
void APIENTRY_GL4ES gl4es_glDisableVertexAttribArray(GLuint index) {
    LOAD_GLES(glDisableVertexAttribArray);
#ifndef direct_glDisableVertexAttribArray
    PUSH_IF_COMPILING(glDisableVertexAttribArray)
#endif
    if(likely(gles_glDisableVertexAttribArray)) gles_glDisableVertexAttribArray(index);
}
AliasExport(void,glDisableVertexAttribArray,,(GLuint index));
#endif

#ifndef skip_glDrawArrays
void APIENTRY_GL4ES gl4es_glDrawArrays(GLenum mode, GLint first, GLsizei count) {
    LOAD_GLES(glDrawArrays);
#ifndef direct_glDrawArrays
    PUSH_IF_COMPILING(glDrawArrays)
#endif
    if(likely(gles_glDrawArrays)) gles_glDrawArrays(mode, first, count);
}
AliasExport(void,glDrawArrays,,(GLenum mode, GLint first, GLsizei count));
#endif

#ifndef skip_glDrawBuffers
void APIENTRY_GL4ES gl4es_glDrawBuffers(GLsizei n, const GLenum * bufs) {
    LOAD_GLES_EXT(glDrawBuffers);
#ifndef direct_glDrawBuffers
    PUSH_IF_COMPILING(glDrawBuffers)
#endif
    if(likely(gles_glDrawBuffers)) gles_glDrawBuffers(n, bufs);
}
AliasExport(void,glDrawBuffers(,,GLsizei n, const GLenum * bufs));
#endif

#ifndef skip_glDrawElements
void APIENTRY_GL4ES gl4es_glDrawElements(GLenum mode, GLsizei count, GLenum type, const GLvoid * indices) {
    LOAD_GLES(glDrawElements);
#ifndef direct_glDrawElements
    PUSH_IF_COMPILING(glDrawElements)
#endif
    if(likely(gles_glDrawElements)) gles_glDrawElements(mode, count, type, indices);
}
AliasExport(void,glDrawElements,,(GLenum mode, GLsizei count, GLenum type, const GLvoid * indices));
#endif
#ifndef skip_glDrawTexf
void APIENTRY_GL4ES gl4es_glDrawTexf(GLfloat x, GLfloat y, GLfloat z, GLfloat width, GLfloat height) {
    LOAD_GLES_OES(glDrawTexf);
#ifndef direct_glDrawTexf
    PUSH_IF_COMPILING(glDrawTexf)
#endif
    if(likely(gles_glDrawTexf)) gles_glDrawTexf(x, y, z, width, height);
}
AliasExport(void,glDrawTexf,,(GLfloat x, GLfloat y, GLfloat z, GLfloat width, GLfloat height));
#endif

#ifndef skip_glDrawTexi
void APIENTRY_GL4ES gl4es_glDrawTexi(GLint x, GLint y, GLint z, GLint width, GLint height) {
    LOAD_GLES_OES(glDrawTexi);
#ifndef direct_glDrawTexi
    PUSH_IF_COMPILING(glDrawTexi)
#endif
    if(likely(gles_glDrawTexi)) gles_glDrawTexi(x, y, z, width, height);
}
AliasExport(void,glDrawTexi,,(GLint x, GLint y, GLint z, GLint width, GLint height));
#endif

#ifndef skip_glEnable
void APIENTRY_GL4ES gl4es_glEnable(GLenum cap) {
    LOAD_GLES(glEnable);
#ifndef direct_glEnable
    PUSH_IF_COMPILING(glEnable)
#endif
    if(likely(gles_glEnable)) gles_glEnable(cap);
}
AliasExport(void,glEnable,,(GLenum cap));
#endif

#ifndef skip_glEnableClientState
void APIENTRY_GL4ES gl4es_glEnableClientState(GLenum array) {
    LOAD_GLES(glEnableClientState);
#ifndef direct_glEnableClientState
    PUSH_IF_COMPILING(glEnableClientState)
#endif
    if(likely(gles_glEnableClientState)) gles_glEnableClientState(array);
}
AliasExport(void,glEnableClientState,,(GLenum array));
#endif

#ifndef skip_glEnableVertexAttribArray
void APIENTRY_GL4ES gl4es_glEnableVertexAttribArray(GLuint index) {
    LOAD_GLES(glEnableVertexAttribArray);
#ifndef direct_glEnableVertexAttribArray
    PUSH_IF_COMPILING(glEnableVertexAttribArray)
#endif
    if(likely(gles_glEnableVertexAttribArray)) gles_glEnableVertexAttribArray(index);
}
AliasExport(void,glEnableVertexAttribArray,,(GLuint index));
#endif

#ifndef skip_glFinish
void APIENTRY_GL4ES gl4es_glFinish() {
    LOAD_GLES(glFinish);
#ifndef direct_glFinish
    PUSH_IF_COMPILING(glFinish)
#endif
    if(likely(gles_glFinish)) gles_glFinish();
}
AliasExport(void,glFinish,,());
#endif

#ifndef skip_glFlush
void APIENTRY_GL4ES gl4es_glFlush() {
    LOAD_GLES(glFlush);
#ifndef direct_glFlush
    PUSH_IF_COMPILING(glFlush)
#endif
    if(likely(gles_glFlush)) gles_glFlush();
}
AliasExport(void,glFlush,,());
#endif

#ifndef skip_glFogCoordPointer
void APIENTRY_GL4ES gl4es_glFogCoordPointer(GLenum type, GLsizei stride, const GLvoid * pointer) {
    LOAD_GLES(glFogCoordPointer);
#ifndef direct_glFogCoordPointer
    PUSH_IF_COMPILING(glFogCoordPointer)
#endif
    if(likely(gles_glFogCoordPointer)) gles_glFogCoordPointer(type, stride, pointer);
}
AliasExport(void,glFogCoordPointer,,(GLenum type, GLsizei stride, const GLvoid * pointer));
#endif

#ifndef skip_glFogCoordf
void APIENTRY_GL4ES gl4es_glFogCoordf(GLfloat coord) {
    LOAD_GLES(glFogCoordf);
#ifndef direct_glFogCoordf
    PUSH_IF_COMPILING(glFogCoordf)
#endif
    if(likely(gles_glFogCoordf)) gles_glFogCoordf(coord);
}
AliasExport(void,glFogCoordf,,(GLfloat coord));
#endif

#ifndef skip_glFogCoordfv
void APIENTRY_GL4ES gl4es_glFogCoordfv(const GLfloat * coord) {
    LOAD_GLES(glFogCoordfv);
#ifndef direct_glFogCoordfv
    PUSH_IF_COMPILING(glFogCoordfv)
#endif
    if(likely(gles_glFogCoordfv)) gles_glFogCoordfv(coord);
}
AliasExport(void,glFogCoordfv,,(const GLfloat * coord));
#endif

#ifndef skip_glFogf
void APIENTRY_GL4ES gl4es_glFogf(GLenum pname, GLfloat param) {
    LOAD_GLES(glFogf);
#ifndef direct_glFogf
    PUSH_IF_COMPILING(glFogf)
#endif
    if(likely(gles_glFogf)) gles_glFogf(pname, param);
}
AliasExport(void,glFogf,,(GLenum pname, GLfloat param));
#endif

#ifndef skip_glFogfv
void APIENTRY_GL4ES gl4es_glFogfv(GLenum pname, const GLfloat * params) {
    LOAD_GLES(glFogfv);
#ifndef direct_glFogfv
    PUSH_IF_COMPILING(glFogfv)
#endif
    if(likely(gles_glFogfv)) gles_glFogfv(pname, params);
}
AliasExport(void,glFogfv,,(GLenum pname, const GLfloat * params));
#endif

#ifndef skip_glFogx
void APIENTRY_GL4ES gl4es_glFogx(GLenum pname, GLfixed param) {
    LOAD_GLES(glFogx);
#ifndef direct_glFogx
    PUSH_IF_COMPILING(glFogx)
#endif
    if(likely(gles_glFogx)) gles_glFogx(pname, param);
}
AliasExport(void,glFogx,,(GLenum pname, GLfixed param));
#endif

#ifndef skip_glFogxv
void APIENTRY_GL4ES gl4es_glFogxv(GLenum pname, const GLfixed * params) {
    LOAD_GLES(glFogxv);
#ifndef direct_glFogxv
    PUSH_IF_COMPILING(glFogxv)
#endif
    if(likely(gles_glFogxv)) gles_glFogxv(pname, params);
}
AliasExport(void,glFogxv,,(GLenum pname, const GLfixed * params));
#endif
#ifndef skip_glFramebufferRenderbuffer
void APIENTRY_GL4ES gl4es_glFramebufferRenderbuffer(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer) {
    LOAD_GLES_OES(glFramebufferRenderbuffer);
#ifndef direct_glFramebufferRenderbuffer
    PUSH_IF_COMPILING(glFramebufferRenderbuffer)
#endif
    if(likely(gles_glFramebufferRenderbuffer)) gles_glFramebufferRenderbuffer(target, attachment, renderbuffertarget, renderbuffer);
}
AliasExport(void,glFramebufferRenderbuffer,,(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer));
#endif

#ifndef skip_glFramebufferTexture2D
void APIENTRY_GL4ES gl4es_glFramebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level) {
    LOAD_GLES_OES(glFramebufferTexture2D);
#ifndef direct_glFramebufferTexture2D
    PUSH_IF_COMPILING(glFramebufferTexture2D)
#endif
    if(likely(gles_glFramebufferTexture2D)) gles_glFramebufferTexture2D(target, attachment, textarget, texture, level);
}
AliasExport(void,glFramebufferTexture2D,,(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level));
#endif

#ifndef skip_glFrontFace
void APIENTRY_GL4ES gl4es_glFrontFace(GLenum mode) {
    LOAD_GLES(glFrontFace);
#ifndef direct_glFrontFace
    PUSH_IF_COMPILING(glFrontFace)
#endif
    if(likely(gles_glFrontFace)) gles_glFrontFace(mode);
}
AliasExport(void,glFrontFace,,(GLenum mode));
#endif

#ifndef skip_glFrustumf
void APIENTRY_GL4ES gl4es_glFrustumf(GLfloat left, GLfloat right, GLfloat bottom, GLfloat top, GLfloat Near, GLfloat Far) {
    LOAD_GLES(glFrustumf);
#ifndef direct_glFrustumf
    PUSH_IF_COMPILING(glFrustumf)
#endif
    if(likely(gles_glFrustumf)) gles_glFrustumf(left, right, bottom, top, Near, Far);
}
AliasExport(void,glFrustumf,,(GLfloat left, GLfloat right, GLfloat bottom, GLfloat top, GLfloat Near, GLfloat Far));
#endif

#ifndef skip_glFrustumx
void APIENTRY_GL4ES gl4es_glFrustumx(GLfixed left, GLfixed right, GLfixed bottom, GLfixed top, GLfixed Near, GLfixed Far) {
    LOAD_GLES(glFrustumx);
#ifndef direct_glFrustumx
    PUSH_IF_COMPILING(glFrustumx)
#endif
    if(likely(gles_glFrustumx)) gles_glFrustumx(left, right, bottom, top, Near, Far);
}
AliasExport(void,glFrustumx,,(GLfixed left, GLfixed right, GLfixed bottom, GLfixed top, GLfixed Near, GLfixed Far));
#endif

#ifndef skip_glGenBuffers
void APIENTRY_GL4ES gl4es_glGenBuffers(GLsizei n, GLuint * buffer) {
    LOAD_GLES(glGenBuffers);
#ifndef direct_glGenBuffers
    PUSH_IF_COMPILING(glGenBuffers)
#endif
    if(likely(gles_glGenBuffers)) gles_glGenBuffers(n, buffer);
}
AliasExport(void,glGenBuffers,,(GLsizei n, GLuint * buffer));
#endif

#ifndef skip_glGenFramebuffers
void APIENTRY_GL4ES gl4es_glGenFramebuffers(GLsizei n, GLuint * ids) {
    LOAD_GLES_OES(glGenFramebuffers);
#ifndef direct_glGenFramebuffers
    PUSH_IF_COMPILING(glGenFramebuffers)
#endif
    if(likely(gles_glGenFramebuffers)) gles_glGenFramebuffers(n, ids);
}
AliasExport(void,glGenFramebuffers,,(GLsizei n, GLuint * ids));
#endif

#ifndef skip_glGenRenderbuffers
void APIENTRY_GL4ES gl4es_glGenRenderbuffers(GLsizei n, GLuint * renderbuffers) {
    LOAD_GLES_OES(glGenRenderbuffers);
#ifndef direct_glGenRenderbuffers
    PUSH_IF_COMPILING(glGenRenderbuffers)
#endif
    if(likely(gles_glGenRenderbuffers)) gles_glGenRenderbuffers(n, renderbuffers);
}
AliasExport(void,glGenRenderbuffers,,(GLsizei n, GLuint * renderbuffers));
#endif

#ifndef skip_glGenTextures
void APIENTRY_GL4ES gl4es_glGenTextures(GLsizei n, GLuint * textures) {
    LOAD_GLES(glGenTextures);
#ifndef direct_glGenTextures
    PUSH_IF_COMPILING(glGenTextures)
#endif
    if(likely(gles_glGenTextures)) gles_glGenTextures(n, textures);
}
AliasExport(void,glGenTextures,,(GLsizei n, GLuint * textures));
#endif

#ifndef skip_glGenerateMipmap
void APIENTRY_GL4ES gl4es_glGenerateMipmap(GLenum target) {
    LOAD_GLES_OES(glGenerateMipmap);
#ifndef direct_glGenerateMipmap
    PUSH_IF_COMPILING(glGenerateMipmap)
#endif
    if(likely(gles_glGenerateMipmap)) gles_glGenerateMipmap(target);
}
AliasExport(void,glGenerateMipmap,,(GLenum target));
#endif

#ifndef skip_glGetActiveAttrib
void APIENTRY_GL4ES gl4es_glGetActiveAttrib(GLuint program, GLuint index, GLsizei bufSize, GLsizei * length, GLint * size, GLenum * type, GLchar * name) {
    LOAD_GLES(glGetActiveAttrib);
#ifndef direct_glGetActiveAttrib
    PUSH_IF_COMPILING(glGetActiveAttrib)
#endif
    if(likely(gles_glGetActiveAttrib)) gles_glGetActiveAttrib(program, index, bufSize, length, size, type, name);
}
AliasExport(void,glGetActiveAttrib,,(GLuint program, GLuint index, GLsizei bufSize, GLsizei * length, GLint * size, GLenum * type, GLchar * name));
#endif

#ifndef skip_glGetActiveUniform
void APIENTRY_GL4ES gl4es_glGetActiveUniform(GLuint program, GLuint index, GLsizei bufSize, GLsizei * length, GLint * size, GLenum * type, GLchar * name) {
    LOAD_GLES(glGetActiveUniform);
#ifndef direct_glGetActiveUniform
    PUSH_IF_COMPILING(glGetActiveUniform)
#endif
    if(likely(gles_glGetActiveUniform)) gles_glGetActiveUniform(program, index, bufSize, length, size, type, name);
}
AliasExport(void,glGetActiveUniform,,(GLuint program, GLuint index, GLsizei bufSize, GLsizei * length, GLint * size, GLenum * type, GLchar * name));
#endif

#ifndef skip_glGetAttachedShaders
void APIENTRY_GL4ES gl4es_glGetAttachedShaders(GLuint program, GLsizei maxCount, GLsizei * count, GLuint * obj) {
    LOAD_GLES(glGetAttachedShaders);
#ifndef direct_glGetAttachedShaders
    PUSH_IF_COMPILING(glGetAttachedShaders)
#endif
    if(likely(gles_glGetAttachedShaders)) gles_glGetAttachedShaders(program, maxCount, count, obj);
}
AliasExport(void,glGetAttachedShaders,,(GLuint program, GLsizei maxCount, GLsizei * count, GLuint * obj));
#endif

#ifndef skip_glGetAttribLocation
GLint APIENTRY_GL4ES gl4es_glGetAttribLocation(GLuint program, const GLchar * name) {
    LOAD_GLES(glGetAttribLocation);
#ifndef direct_glGetAttribLocation
    PUSH_IF_COMPILING(glGetAttribLocation)
#endif
    if(likely(gles_glGetAttribLocation)) return gles_glGetAttribLocation(program, name);
    return -1;
}
AliasExport(GLint,glGetAttribLocation,,(GLuint program, const GLchar * name));
#endif

#ifndef skip_glGetBooleanv
void APIENTRY_GL4ES gl4es_glGetBooleanv(GLenum pname, GLboolean * params) {
    // Calls the emulated version defined at top of file
    void gles_glGetBooleanv(glGetBooleanv_ARG_EXPAND); 
#ifndef direct_glGetBooleanv
    PUSH_IF_COMPILING(glGetBooleanv)
#endif
    gles_glGetBooleanv(pname, params);
}
AliasExport(void,glGetBooleanv,,(GLenum pname, GLboolean * params));
#endif

#ifndef skip_glGetBufferParameteriv
void APIENTRY_GL4ES gl4es_glGetBufferParameteriv(GLenum target, GLenum pname, GLint * params) {
    LOAD_GLES(glGetBufferParameteriv);
#ifndef direct_glGetBufferParameteriv
    PUSH_IF_COMPILING(glGetBufferParameteriv)
#endif
    if(likely(gles_glGetBufferParameteriv)) gles_glGetBufferParameteriv(target, pname, params);
}
AliasExport(void,glGetBufferParameteriv,,(GLenum target, GLenum pname, GLint * params));
#endif

#ifndef skip_glGetClipPlanef
void APIENTRY_GL4ES gl4es_glGetClipPlanef(GLenum plane, GLfloat * equation) {
    LOAD_GLES(glGetClipPlanef);
#ifndef direct_glGetClipPlanef
    PUSH_IF_COMPILING(glGetClipPlanef)
#endif
    if(likely(gles_glGetClipPlanef)) gles_glGetClipPlanef(plane, equation);
}
AliasExport(void,glGetClipPlanef,,(GLenum plane, GLfloat * equation));
#endif

#ifndef skip_glGetClipPlanex
void APIENTRY_GL4ES gl4es_glGetClipPlanex(GLenum plane, GLfixed * equation) {
    LOAD_GLES(glGetClipPlanex);
#ifndef direct_glGetClipPlanex
    PUSH_IF_COMPILING(glGetClipPlanex)
#endif
    if(likely(gles_glGetClipPlanex)) gles_glGetClipPlanex(plane, equation);
}
AliasExport(void,glGetClipPlanex,,(GLenum plane, GLfixed * equation));
#endif

#ifndef skip_glGetError
GLenum APIENTRY_GL4ES gl4es_glGetError(void) {
    LOAD_GLES(glGetError);
#ifndef direct_glGetError
    PUSH_IF_COMPILING(glGetError)
#endif
    if(likely(gles_glGetError)) return gles_glGetError();
    return 0; // GL_NO_ERROR
}
AliasExport(GLenum,glGetError,,());
#endif

#ifndef skip_glGetFixedv
void APIENTRY_GL4ES gl4es_glGetFixedv(GLenum pname, GLfixed * params) {
    LOAD_GLES(glGetFixedv);
#ifndef direct_glGetFixedv
    PUSH_IF_COMPILING(glGetFixedv)
#endif
    if(likely(gles_glGetFixedv)) gles_glGetFixedv(pname, params);
}
AliasExport(void,glGetFixedv,,(GLenum pname, GLfixed * params));
#endif

#ifndef skip_glGetFloatv
void APIENTRY_GL4ES gl4es_glGetFloatv(GLenum pname, GLfloat * params) {
    LOAD_GLES(glGetFloatv);
#ifndef direct_glGetFloatv
    PUSH_IF_COMPILING(glGetFloatv)
#endif
    if(likely(gles_glGetFloatv)) gles_glGetFloatv(pname, params);
}
AliasExport(void,glGetFloatv,,(GLenum pname, GLfloat * params));
#endif

#ifndef skip_glGetFramebufferAttachmentParameteriv
void APIENTRY_GL4ES gl4es_glGetFramebufferAttachmentParameteriv(GLenum target, GLenum attachment, GLenum pname, GLint * params) {
    LOAD_GLES_OES(glGetFramebufferAttachmentParameteriv);
#ifndef direct_glGetFramebufferAttachmentParameteriv
    PUSH_IF_COMPILING(glGetFramebufferAttachmentParameteriv)
#endif
    if(likely(gles_glGetFramebufferAttachmentParameteriv)) gles_glGetFramebufferAttachmentParameteriv(target, attachment, pname, params);
}
AliasExport(void,glGetFramebufferAttachmentParameteriv,,(GLenum target, GLenum attachment, GLenum pname, GLint * params));
#endif

#ifndef skip_glGetIntegerv
void APIENTRY_GL4ES gl4es_glGetIntegerv(GLenum pname, GLint * params) {
    LOAD_GLES(glGetIntegerv);
#ifndef direct_glGetIntegerv
    PUSH_IF_COMPILING(glGetIntegerv)
#endif
    if(likely(gles_glGetIntegerv)) gles_glGetIntegerv(pname, params);
}
AliasExport(void,glGetIntegerv,,(GLenum pname, GLint * params));
#endif

#ifndef skip_glGetLightfv
void APIENTRY_GL4ES gl4es_glGetLightfv(GLenum light, GLenum pname, GLfloat * params) {
    LOAD_GLES(glGetLightfv);
#ifndef direct_glGetLightfv
    PUSH_IF_COMPILING(glGetLightfv)
#endif
    if(likely(gles_glGetLightfv)) gles_glGetLightfv(light, pname, params);
}
AliasExport(void,glGetLightfv,,(GLenum light, GLenum pname, GLfloat * params));
#endif

#ifndef skip_glGetLightxv
void APIENTRY_GL4ES gl4es_glGetLightxv(GLenum light, GLenum pname, GLfixed * params) {
    LOAD_GLES(glGetLightxv);
#ifndef direct_glGetLightxv
    PUSH_IF_COMPILING(glGetLightxv)
#endif
    if(likely(gles_glGetLightxv)) gles_glGetLightxv(light, pname, params);
}
AliasExport(void,glGetLightxv,,(GLenum light, GLenum pname, GLfixed * params));
#endif

#ifndef skip_glGetMaterialfv
void APIENTRY_GL4ES gl4es_glGetMaterialfv(GLenum face, GLenum pname, GLfloat * params) {
    LOAD_GLES(glGetMaterialfv);
#ifndef direct_glGetMaterialfv
    PUSH_IF_COMPILING(glGetMaterialfv)
#endif
    if(likely(gles_glGetMaterialfv)) gles_glGetMaterialfv(face, pname, params);
}
AliasExport(void,glGetMaterialfv,,(GLenum face, GLenum pname, GLfloat * params));
#endif

#ifndef skip_glGetMaterialxv
void APIENTRY_GL4ES gl4es_glGetMaterialxv(GLenum face, GLenum pname, GLfixed * params) {
    LOAD_GLES(glGetMaterialxv);
#ifndef direct_glGetMaterialxv
    PUSH_IF_COMPILING(glGetMaterialxv)
#endif
    if(likely(gles_glGetMaterialxv)) gles_glGetMaterialxv(face, pname, params);
}
AliasExport(void,glGetMaterialxv,,(GLenum face, GLenum pname, GLfixed * params));
#endif

#ifndef skip_glGetPointerv
void APIENTRY_GL4ES gl4es_glGetPointerv(GLenum pname, GLvoid ** params) {
    LOAD_GLES(glGetPointerv);
#ifndef direct_glGetPointerv
    PUSH_IF_COMPILING(glGetPointerv)
#endif
    if(likely(gles_glGetPointerv)) gles_glGetPointerv(pname, params);
}
AliasExport(void,glGetPointerv,,(GLenum pname, GLvoid ** params));
#endif

#ifndef skip_glGetProgramBinary
void APIENTRY_GL4ES gl4es_glGetProgramBinary(GLuint program, GLsizei bufSize, GLsizei * length, GLenum * binaryFormat, GLvoid * binary) {
    LOAD_GLES_OES(glGetProgramBinary);
#ifndef direct_glGetProgramBinary
    PUSH_IF_COMPILING(glGetProgramBinary)
#endif
    if(likely(gles_glGetProgramBinary)) gles_glGetProgramBinary(program, bufSize, length, binaryFormat, binary);
}
AliasExport(void,glGetProgramBinary,,(GLuint program, GLsizei bufSize, GLsizei * length, GLenum * binaryFormat, GLvoid * binary));
#endif

#ifndef skip_glGetProgramInfoLog
void APIENTRY_GL4ES gl4es_glGetProgramInfoLog(GLuint program, GLsizei bufSize, GLsizei * length, GLchar * infoLog) {
    LOAD_GLES(glGetProgramInfoLog);
#ifndef direct_glGetProgramInfoLog
    PUSH_IF_COMPILING(glGetProgramInfoLog)
#endif
    if(likely(gles_glGetProgramInfoLog)) gles_glGetProgramInfoLog(program, bufSize, length, infoLog);
}
AliasExport(void,glGetProgramInfoLog,,(GLuint program, GLsizei bufSize, GLsizei * length, GLchar * infoLog));
#endif

#ifndef skip_glGetProgramiv
void APIENTRY_GL4ES gl4es_glGetProgramiv(GLuint program, GLenum pname, GLint * params) {
    LOAD_GLES(glGetProgramiv);
#ifndef direct_glGetProgramiv
    PUSH_IF_COMPILING(glGetProgramiv)
#endif
    if(likely(gles_glGetProgramiv)) gles_glGetProgramiv(program, pname, params);
}
AliasExport(void,glGetProgramiv,,(GLuint program, GLenum pname, GLint * params));
#endif

#ifndef skip_glGetRenderbufferParameteriv
void APIENTRY_GL4ES gl4es_glGetRenderbufferParameteriv(GLenum target, GLenum pname, GLint * params) {
    LOAD_GLES_OES(glGetRenderbufferParameteriv);
#ifndef direct_glGetRenderbufferParameteriv
    PUSH_IF_COMPILING(glGetRenderbufferParameteriv)
#endif
    if(likely(gles_glGetRenderbufferParameteriv)) gles_glGetRenderbufferParameteriv(target, pname, params);
}
AliasExport(void,glGetRenderbufferParameteriv,,(GLenum target, GLenum pname, GLint * params));
#endif
#ifndef skip_glGetShaderInfoLog
void APIENTRY_GL4ES gl4es_glGetShaderInfoLog(GLuint shader, GLsizei bufSize, GLsizei * length, GLchar * infoLog) {
    LOAD_GLES(glGetShaderInfoLog);
#ifndef direct_glGetShaderInfoLog
    PUSH_IF_COMPILING(glGetShaderInfoLog)
#endif
    if(likely(gles_glGetShaderInfoLog)) gles_glGetShaderInfoLog(shader, bufSize, length, infoLog);
}
AliasExport(void,glGetShaderInfoLog,,(GLuint shader, GLsizei bufSize, GLsizei * length, GLchar * infoLog));
#endif

#ifndef skip_glGetShaderPrecisionFormat
void APIENTRY_GL4ES gl4es_glGetShaderPrecisionFormat(GLenum shadertype, GLenum precisiontype, GLint * range, GLint * precision) {
    LOAD_GLES(glGetShaderPrecisionFormat);
#ifndef direct_glGetShaderPrecisionFormat
    PUSH_IF_COMPILING(glGetShaderPrecisionFormat)
#endif
    if(likely(gles_glGetShaderPrecisionFormat)) gles_glGetShaderPrecisionFormat(shadertype, precisiontype, range, precision);
}
AliasExport(void,glGetShaderPrecisionFormat,,(GLenum shadertype, GLenum precisiontype, GLint * range, GLint * precision));
#endif

#ifndef skip_glGetShaderSource
void APIENTRY_GL4ES gl4es_glGetShaderSource(GLuint shader, GLsizei bufSize, GLsizei * length, GLchar * source) {
    LOAD_GLES(glGetShaderSource);
#ifndef direct_glGetShaderSource
    PUSH_IF_COMPILING(glGetShaderSource)
#endif
    if(likely(gles_glGetShaderSource)) gles_glGetShaderSource(shader, bufSize, length, source);
}
AliasExport(void,glGetShaderSource,,(GLuint shader, GLsizei bufSize, GLsizei * length, GLchar * source));
#endif

#ifndef skip_glGetShaderiv
void APIENTRY_GL4ES gl4es_glGetShaderiv(GLuint shader, GLenum pname, GLint * params) {
    LOAD_GLES(glGetShaderiv);
#ifndef direct_glGetShaderiv
    PUSH_IF_COMPILING(glGetShaderiv)
#endif
    if(likely(gles_glGetShaderiv)) gles_glGetShaderiv(shader, pname, params);
}
AliasExport(void,glGetShaderiv,,(GLuint shader, GLenum pname, GLint * params));
#endif

#ifndef skip_glGetString
const GLubyte * APIENTRY_GL4ES gl4es_glGetString(GLenum name) {
    LOAD_GLES(glGetString);
#ifndef direct_glGetString
    PUSH_IF_COMPILING(glGetString)
#endif
    if(likely(gles_glGetString)) return gles_glGetString(name);
    return NULL;
}
AliasExport(const GLubyte*,glGetString,,(GLenum name));
#endif

#ifndef skip_glGetTexEnvfv
void APIENTRY_GL4ES gl4es_glGetTexEnvfv(GLenum target, GLenum pname, GLfloat * params) {
    LOAD_GLES(glGetTexEnvfv);
#ifndef direct_glGetTexEnvfv
    PUSH_IF_COMPILING(glGetTexEnvfv)
#endif
    if(likely(gles_glGetTexEnvfv)) gles_glGetTexEnvfv(target, pname, params);
}
AliasExport(void,glGetTexEnvfv,,(GLenum target, GLenum pname, GLfloat * params));
#endif

#ifndef skip_glGetTexEnviv
void APIENTRY_GL4ES gl4es_glGetTexEnviv(GLenum target, GLenum pname, GLint * params) {
    LOAD_GLES(glGetTexEnviv);
#ifndef direct_glGetTexEnviv
    PUSH_IF_COMPILING(glGetTexEnviv)
#endif
    if(likely(gles_glGetTexEnviv)) gles_glGetTexEnviv(target, pname, params);
}
AliasExport(void,glGetTexEnviv,,(GLenum target, GLenum pname, GLint * params));
#endif

#ifndef skip_glGetTexEnvxv
void APIENTRY_GL4ES gl4es_glGetTexEnvxv(GLenum target, GLenum pname, GLfixed * params) {
    LOAD_GLES(glGetTexEnvxv);
#ifndef direct_glGetTexEnvxv
    PUSH_IF_COMPILING(glGetTexEnvxv)
#endif
    if(likely(gles_glGetTexEnvxv)) gles_glGetTexEnvxv(target, pname, params);
}
AliasExport(void,glGetTexEnvxv,,(GLenum target, GLenum pname, GLfixed * params));
#endif

#ifndef skip_glGetTexParameterfv
void APIENTRY_GL4ES gl4es_glGetTexParameterfv(GLenum target, GLenum pname, GLfloat * params) {
    LOAD_GLES(glGetTexParameterfv);
#ifndef direct_glGetTexParameterfv
    PUSH_IF_COMPILING(glGetTexParameterfv)
#endif
    if(likely(gles_glGetTexParameterfv)) gles_glGetTexParameterfv(target, pname, params);
}
AliasExport(void,glGetTexParameterfv,,(GLenum target, GLenum pname, GLfloat * params));
#endif

#ifndef skip_glGetTexParameteriv
void APIENTRY_GL4ES gl4es_glGetTexParameteriv(GLenum target, GLenum pname, GLint * params) {
    LOAD_GLES(glGetTexParameteriv);
#ifndef direct_glGetTexParameteriv
    PUSH_IF_COMPILING(glGetTexParameteriv)
#endif
    if(likely(gles_glGetTexParameteriv)) gles_glGetTexParameteriv(target, pname, params);
}
AliasExport(void,glGetTexParameteriv,,(GLenum target, GLenum pname, GLint * params));
#endif

#ifndef skip_glGetTexParameterxv
void APIENTRY_GL4ES gl4es_glGetTexParameterxv(GLenum target, GLenum pname, GLfixed * params) {
    LOAD_GLES(glGetTexParameterxv);
#ifndef direct_glGetTexParameterxv
    PUSH_IF_COMPILING(glGetTexParameterxv)
#endif
    if(likely(gles_glGetTexParameterxv)) gles_glGetTexParameterxv(target, pname, params);
}
AliasExport(void,glGetTexParameterxv,,(GLenum target, GLenum pname, GLfixed * params));
#endif

#ifndef skip_glGetUniformLocation
GLint APIENTRY_GL4ES gl4es_glGetUniformLocation(GLuint program, const GLchar * name) {
    LOAD_GLES(glGetUniformLocation);
#ifndef direct_glGetUniformLocation
    PUSH_IF_COMPILING(glGetUniformLocation)
#endif
    if(likely(gles_glGetUniformLocation)) return gles_glGetUniformLocation(program, name);
    return -1;
}
AliasExport(GLint,glGetUniformLocation,,(GLuint program, const GLchar * name));
#endif

#ifndef skip_glGetUniformfv
void APIENTRY_GL4ES gl4es_glGetUniformfv(GLuint program, GLint location, GLfloat * params) {
    LOAD_GLES(glGetUniformfv);
#ifndef direct_glGetUniformfv
    PUSH_IF_COMPILING(glGetUniformfv)
#endif
    if(likely(gles_glGetUniformfv)) gles_glGetUniformfv(program, location, params);
}
AliasExport(void,glGetUniformfv,,(GLuint program, GLint location, GLfloat * params));
#endif

#ifndef skip_glGetUniformiv
void APIENTRY_GL4ES gl4es_glGetUniformiv(GLuint program, GLint location, GLint * params) {
    LOAD_GLES(glGetUniformiv);
#ifndef direct_glGetUniformiv
    PUSH_IF_COMPILING(glGetUniformiv)
#endif
    if(likely(gles_glGetUniformiv)) gles_glGetUniformiv(program, location, params);
}
AliasExport(void,glGetUniformiv,,(GLuint program, GLint location, GLint * params));
#endif

#ifndef skip_glGetVertexAttribPointerv
void APIENTRY_GL4ES gl4es_glGetVertexAttribPointerv(GLuint index, GLenum pname, GLvoid ** pointer) {
    LOAD_GLES(glGetVertexAttribPointerv);
#ifndef direct_glGetVertexAttribPointerv
    PUSH_IF_COMPILING(glGetVertexAttribPointerv)
#endif
    if(likely(gles_glGetVertexAttribPointerv)) gles_glGetVertexAttribPointerv(index, pname, pointer);
}
AliasExport(void,glGetVertexAttribPointerv,,(GLuint index, GLenum pname, GLvoid ** pointer));
#endif

#ifndef skip_glGetVertexAttribfv
void APIENTRY_GL4ES gl4es_glGetVertexAttribfv(GLuint index, GLenum pname, GLfloat * params) {
    LOAD_GLES(glGetVertexAttribfv);
#ifndef direct_glGetVertexAttribfv
    PUSH_IF_COMPILING(glGetVertexAttribfv)
#endif
    if(likely(gles_glGetVertexAttribfv)) gles_glGetVertexAttribfv(index, pname, params);
}
AliasExport(void,glGetVertexAttribfv,,(GLuint index, GLenum pname, GLfloat * params));
#endif

#ifndef skip_glGetVertexAttribiv
void APIENTRY_GL4ES gl4es_glGetVertexAttribiv(GLuint index, GLenum pname, GLint * params) {
    LOAD_GLES(glGetVertexAttribiv);
#ifndef direct_glGetVertexAttribiv
    PUSH_IF_COMPILING(glGetVertexAttribiv)
#endif
    if(likely(gles_glGetVertexAttribiv)) gles_glGetVertexAttribiv(index, pname, params);
}
AliasExport(void,glGetVertexAttribiv,,(GLuint index, GLenum pname, GLint * params));
#endif

#ifndef skip_glHint
void APIENTRY_GL4ES gl4es_glHint(GLenum target, GLenum mode) {
    LOAD_GLES(glHint);
#ifndef direct_glHint
    PUSH_IF_COMPILING(glHint)
#endif
    if(likely(gles_glHint)) gles_glHint(target, mode);
}
AliasExport(void,glHint,,(GLenum target, GLenum mode));
#endif

#ifndef skip_glIsBuffer
GLboolean APIENTRY_GL4ES gl4es_glIsBuffer(GLuint buffer) {
    LOAD_GLES(glIsBuffer);
#ifndef direct_glIsBuffer
    PUSH_IF_COMPILING(glIsBuffer)
#endif
    if(likely(gles_glIsBuffer)) return gles_glIsBuffer(buffer);
    return GL_FALSE;
}
AliasExport(GLboolean,glIsBuffer,,(GLuint buffer));
#endif

#ifndef skip_glIsEnabled
GLboolean APIENTRY_GL4ES gl4es_glIsEnabled(GLenum cap) {
    // Prototype check for the emulated function at top of file
    GLboolean gles_glIsEnabled(glIsEnabled_ARG_EXPAND); 
#ifndef direct_glIsEnabled
    PUSH_IF_COMPILING(glIsEnabled)
#endif
    // No likely check here as it calls our internal emulation wrapper
    return gles_glIsEnabled(cap);
}
AliasExport(GLboolean,glIsEnabled,,(GLenum cap));
#endif

#ifndef skip_glIsFramebuffer
GLboolean APIENTRY_GL4ES gl4es_glIsFramebuffer(GLuint framebuffer) {
    LOAD_GLES_OES(glIsFramebuffer);
#ifndef direct_glIsFramebuffer
    PUSH_IF_COMPILING(glIsFramebuffer)
#endif
    if(likely(gles_glIsFramebuffer)) return gles_glIsFramebuffer(framebuffer);
    return GL_FALSE;
}
AliasExport(GLboolean,glIsFramebuffer,,(GLuint framebuffer));
#endif

#ifndef skip_glIsProgram
GLboolean APIENTRY_GL4ES gl4es_glIsProgram(GLuint program) {
    LOAD_GLES(glIsProgram);
#ifndef direct_glIsProgram
    PUSH_IF_COMPILING(glIsProgram)
#endif
    if(likely(gles_glIsProgram)) return gles_glIsProgram(program);
    return GL_FALSE;
}
AliasExport(GLboolean,glIsProgram,,(GLuint program));
#endif

#ifndef skip_glIsRenderbuffer
GLboolean APIENTRY_GL4ES gl4es_glIsRenderbuffer(GLuint renderbuffer) {
    LOAD_GLES_OES(glIsRenderbuffer);
#ifndef direct_glIsRenderbuffer
    PUSH_IF_COMPILING(glIsRenderbuffer)
#endif
    if(likely(gles_glIsRenderbuffer)) return gles_glIsRenderbuffer(renderbuffer);
    return GL_FALSE;
}
AliasExport(GLboolean,glIsRenderbuffer,,(GLuint renderbuffer));
#endif

#ifndef skip_glIsShader
GLboolean APIENTRY_GL4ES gl4es_glIsShader(GLuint shader) {
    LOAD_GLES(glIsShader);
#ifndef direct_glIsShader
    PUSH_IF_COMPILING(glIsShader)
#endif
    if(likely(gles_glIsShader)) return gles_glIsShader(shader);
    return GL_FALSE;
}
AliasExport(GLboolean,glIsShader,,(GLuint shader));
#endif

#ifndef skip_glIsTexture
GLboolean APIENTRY_GL4ES gl4es_glIsTexture(GLuint texture) {
    LOAD_GLES(glIsTexture);
#ifndef direct_glIsTexture
    PUSH_IF_COMPILING(glIsTexture)
#endif
    if(likely(gles_glIsTexture)) return gles_glIsTexture(texture);
    return GL_FALSE;
}
AliasExport(GLboolean,glIsTexture,,(GLuint texture));
#endif
#ifndef skip_glLightModelf
void APIENTRY_GL4ES gl4es_glLightModelf(GLenum pname, GLfloat param) {
    LOAD_GLES(glLightModelf);
#ifndef direct_glLightModelf
    PUSH_IF_COMPILING(glLightModelf)
#endif
    if(likely(gles_glLightModelf)) gles_glLightModelf(pname, param);
}
AliasExport(void,glLightModelf,,(GLenum pname, GLfloat param));
#endif

#ifndef skip_glLightModelfv
void APIENTRY_GL4ES gl4es_glLightModelfv(GLenum pname, const GLfloat * params) {
    LOAD_GLES(glLightModelfv);
#ifndef direct_glLightModelfv
    PUSH_IF_COMPILING(glLightModelfv)
#endif
    if(likely(gles_glLightModelfv)) gles_glLightModelfv(pname, params);
}
AliasExport(void,glLightModelfv,,(GLenum pname, const GLfloat * params));
#endif

#ifndef skip_glLightModelx
void APIENTRY_GL4ES gl4es_glLightModelx(GLenum pname, GLfixed param) {
    LOAD_GLES(glLightModelx);
#ifndef direct_glLightModelx
    PUSH_IF_COMPILING(glLightModelx)
#endif
    if(likely(gles_glLightModelx)) gles_glLightModelx(pname, param);
}
AliasExport(void,glLightModelx,,(GLenum pname, GLfixed param));
#endif

#ifndef skip_glLightModelxv
void APIENTRY_GL4ES gl4es_glLightModelxv(GLenum pname, const GLfixed * params) {
    LOAD_GLES(glLightModelxv);
#ifndef direct_glLightModelxv
    PUSH_IF_COMPILING(glLightModelxv)
#endif
    if(likely(gles_glLightModelxv)) gles_glLightModelxv(pname, params);
}
AliasExport(void,glLightModelxv,,(GLenum pname, const GLfixed * params));
#endif

#ifndef skip_glLightf
void APIENTRY_GL4ES gl4es_glLightf(GLenum light, GLenum pname, GLfloat param) {
    LOAD_GLES(glLightf);
#ifndef direct_glLightf
    PUSH_IF_COMPILING(glLightf)
#endif
    if(likely(gles_glLightf)) gles_glLightf(light, pname, param);
}
AliasExport(void,glLightf,,(GLenum light, GLenum pname, GLfloat param));
#endif

#ifndef skip_glLightfv
void APIENTRY_GL4ES gl4es_glLightfv(GLenum light, GLenum pname, const GLfloat * params) {
    LOAD_GLES(glLightfv);
#ifndef direct_glLightfv
    PUSH_IF_COMPILING(glLightfv)
#endif
    if(likely(gles_glLightfv)) gles_glLightfv(light, pname, params);
}
AliasExport(void,glLightfv,,(GLenum light, GLenum pname, const GLfloat * params));
#endif

#ifndef skip_glLightx
void APIENTRY_GL4ES gl4es_glLightx(GLenum light, GLenum pname, GLfixed param) {
    LOAD_GLES(glLightx);
#ifndef direct_glLightx
    PUSH_IF_COMPILING(glLightx)
#endif
    if(likely(gles_glLightx)) gles_glLightx(light, pname, param);
}
AliasExport(void,glLightx,,(GLenum light, GLenum pname, GLfixed param));
#endif

#ifndef skip_glLightxv
void APIENTRY_GL4ES gl4es_glLightxv(GLenum light, GLenum pname, const GLfixed * params) {
    LOAD_GLES(glLightxv);
#ifndef direct_glLightxv
    PUSH_IF_COMPILING(glLightxv)
#endif
    if(likely(gles_glLightxv)) gles_glLightxv(light, pname, params);
}
AliasExport(void,glLightxv,,(GLenum light, GLenum pname, const GLfixed * params));
#endif

#ifndef skip_glLineWidth
void APIENTRY_GL4ES gl4es_glLineWidth(GLfloat width) {
    LOAD_GLES(glLineWidth);
#ifndef direct_glLineWidth
    PUSH_IF_COMPILING(glLineWidth)
#endif
    if(likely(gles_glLineWidth)) gles_glLineWidth(width);
}
AliasExport(void,glLineWidth,,(GLfloat width));
#endif

#ifndef skip_glLineWidthx
void APIENTRY_GL4ES gl4es_glLineWidthx(GLfixed width) {
    LOAD_GLES(glLineWidthx);
#ifndef direct_glLineWidthx
    PUSH_IF_COMPILING(glLineWidthx)
#endif
    if(likely(gles_glLineWidthx)) gles_glLineWidthx(width);
}
AliasExport(void,glLineWidthx,,(GLfixed width));
#endif

#ifndef skip_glLinkProgram
void APIENTRY_GL4ES gl4es_glLinkProgram(GLuint program) {
    LOAD_GLES(glLinkProgram);
#ifndef direct_glLinkProgram
    PUSH_IF_COMPILING(glLinkProgram)
#endif
    if(likely(gles_glLinkProgram)) gles_glLinkProgram(program);
}
AliasExport(void,glLinkProgram,,(GLuint program));
#endif

#ifndef skip_glLoadIdentity
void APIENTRY_GL4ES gl4es_glLoadIdentity(void) {
    LOAD_GLES(glLoadIdentity);
#ifndef direct_glLoadIdentity
    PUSH_IF_COMPILING(glLoadIdentity)
#endif
    if(likely(gles_glLoadIdentity)) gles_glLoadIdentity();
}
AliasExport(void,glLoadIdentity,,());
#endif

#ifndef skip_glLoadMatrixf
void APIENTRY_GL4ES gl4es_glLoadMatrixf(const GLfloat * m) {
    LOAD_GLES(glLoadMatrixf);
#ifndef direct_glLoadMatrixf
    PUSH_IF_COMPILING(glLoadMatrixf)
#endif
    if(likely(gles_glLoadMatrixf)) gles_glLoadMatrixf(m);
}
AliasExport(void,glLoadMatrixf,,(const GLfloat * m));
#endif

#ifndef skip_glLoadMatrixx
void APIENTRY_GL4ES gl4es_glLoadMatrixx(const GLfixed * m) {
    LOAD_GLES(glLoadMatrixx);
#ifndef direct_glLoadMatrixx
    PUSH_IF_COMPILING(glLoadMatrixx)
#endif
    if(likely(gles_glLoadMatrixx)) gles_glLoadMatrixx(m);
}
AliasExport(void,glLoadMatrixx,,(const GLfixed * m));
#endif

#ifndef skip_glLogicOp
void APIENTRY_GL4ES gl4es_glLogicOp(GLenum opcode) {
    LOAD_GLES(glLogicOp);
#ifndef direct_glLogicOp
    PUSH_IF_COMPILING(glLogicOp)
#endif
    if(likely(gles_glLogicOp)) gles_glLogicOp(opcode);
}
AliasExport(void,glLogicOp,,(GLenum opcode));
#endif

#ifndef skip_glMaterialf
void APIENTRY_GL4ES gl4es_glMaterialf(GLenum face, GLenum pname, GLfloat param) {
    LOAD_GLES(glMaterialf);
#ifndef direct_glMaterialf
    PUSH_IF_COMPILING(glMaterialf)
#endif
    if(likely(gles_glMaterialf)) gles_glMaterialf(face, pname, param);
}
AliasExport(void,glMaterialf,,(GLenum face, GLenum pname, GLfloat param));
#endif

#ifndef skip_glMaterialfv
void APIENTRY_GL4ES gl4es_glMaterialfv(GLenum face, GLenum pname, const GLfloat * params) {
    LOAD_GLES(glMaterialfv);
#ifndef direct_glMaterialfv
    PUSH_IF_COMPILING(glMaterialfv)
#endif
    if(likely(gles_glMaterialfv)) gles_glMaterialfv(face, pname, params);
}
AliasExport(void,glMaterialfv,,(GLenum face, GLenum pname, const GLfloat * params));
#endif

#ifndef skip_glMaterialx
void APIENTRY_GL4ES gl4es_glMaterialx(GLenum face, GLenum pname, GLfixed param) {
    LOAD_GLES(glMaterialx);
#ifndef direct_glMaterialx
    PUSH_IF_COMPILING(glMaterialx)
#endif
    if(likely(gles_glMaterialx)) gles_glMaterialx(face, pname, param);
}
AliasExport(void,glMaterialx,,(GLenum face, GLenum pname, GLfixed param));
#endif

#ifndef skip_glMaterialxv
void APIENTRY_GL4ES gl4es_glMaterialxv(GLenum face, GLenum pname, const GLfixed * params) {
    LOAD_GLES(glMaterialxv);
#ifndef direct_glMaterialxv
    PUSH_IF_COMPILING(glMaterialxv)
#endif
    if(likely(gles_glMaterialxv)) gles_glMaterialxv(face, pname, params);
}
AliasExport(void,glMaterialxv,,(GLenum face, GLenum pname, const GLfixed * params));
#endif

#ifndef skip_glMatrixMode
void APIENTRY_GL4ES gl4es_glMatrixMode(GLenum mode) {
    LOAD_GLES(glMatrixMode);
#ifndef direct_glMatrixMode
    PUSH_IF_COMPILING(glMatrixMode)
#endif
    if(likely(gles_glMatrixMode)) gles_glMatrixMode(mode);
}
AliasExport(void,glMatrixMode,,(GLenum mode));
#endif

#ifndef skip_glMultMatrixf
void APIENTRY_GL4ES gl4es_glMultMatrixf(const GLfloat * m) {
    LOAD_GLES(glMultMatrixf);
#ifndef direct_glMultMatrixf
    PUSH_IF_COMPILING(glMultMatrixf)
#endif
    if(likely(gles_glMultMatrixf)) gles_glMultMatrixf(m);
}
AliasExport(void,glMultMatrixf,,(const GLfloat * m));
#endif

#ifndef skip_glMultMatrixx
void APIENTRY_GL4ES gl4es_glMultMatrixx(const GLfixed * m) {
    LOAD_GLES(glMultMatrixx);
#ifndef direct_glMultMatrixx
    PUSH_IF_COMPILING(glMultMatrixx)
#endif
    if(likely(gles_glMultMatrixx)) gles_glMultMatrixx(m);
}
AliasExport(void,glMultMatrixx,,(const GLfixed * m));
#endif

#ifndef skip_glMultiDrawArrays
void APIENTRY_GL4ES gl4es_glMultiDrawArrays(GLenum mode, const GLint * first, const GLsizei * count, GLsizei primcount) {
    LOAD_GLES_OES(glMultiDrawArrays);
#ifndef direct_glMultiDrawArrays
    PUSH_IF_COMPILING(glMultiDrawArrays)
#endif
    if(likely(gles_glMultiDrawArrays)) gles_glMultiDrawArrays(mode, first, count, primcount);
}
AliasExport(void,glMultiDrawArrays,,(GLenum mode, const GLint * first, const GLsizei * count, GLsizei primcount));
#endif

#ifndef skip_glMultiDrawElements
void APIENTRY_GL4ES gl4es_glMultiDrawElements(GLenum mode, GLsizei * count, GLenum type, const void * const * indices, GLsizei primcount) {
    LOAD_GLES_OES(glMultiDrawElements);
#ifndef direct_glMultiDrawElements
    PUSH_IF_COMPILING(glMultiDrawElements)
#endif
    if(likely(gles_glMultiDrawElements)) gles_glMultiDrawElements(mode, count, type, indices, primcount);
}
AliasExport(void,glMultiDrawElements,,(GLenum mode, GLsizei * count, GLenum type, const void * const * indices, GLsizei primcount));
#endif

#ifndef skip_glMultiTexCoord4f
void APIENTRY_GL4ES gl4es_glMultiTexCoord4f(GLenum target, GLfloat s, GLfloat t, GLfloat r, GLfloat q) {
    LOAD_GLES(glMultiTexCoord4f);
#ifndef direct_glMultiTexCoord4f
    PUSH_IF_COMPILING(glMultiTexCoord4f)
#endif
    if(likely(gles_glMultiTexCoord4f)) gles_glMultiTexCoord4f(target, s, t, r, q);
}
AliasExport(void,glMultiTexCoord4f,,(GLenum target, GLfloat s, GLfloat t, GLfloat r, GLfloat q));
#endif

#ifndef skip_glMultiTexCoord4x
void APIENTRY_GL4ES gl4es_glMultiTexCoord4x(GLenum target, GLfixed s, GLfixed t, GLfixed r, GLfixed q) {
    LOAD_GLES(glMultiTexCoord4x);
#ifndef direct_glMultiTexCoord4x
    PUSH_IF_COMPILING(glMultiTexCoord4x)
#endif
    if(likely(gles_glMultiTexCoord4x)) gles_glMultiTexCoord4x(target, s, t, r, q);
}
AliasExport(void,glMultiTexCoord4x,,(GLenum target, GLfixed s, GLfixed t, GLfixed r, GLfixed q));
#endif

#ifndef skip_glNormal3f
void APIENTRY_GL4ES gl4es_glNormal3f(GLfloat nx, GLfloat ny, GLfloat nz) {
    LOAD_GLES(glNormal3f);
#ifndef direct_glNormal3f
    PUSH_IF_COMPILING(glNormal3f)
#endif
    if(likely(gles_glNormal3f)) gles_glNormal3f(nx, ny, nz);
}
AliasExport(void,glNormal3f,,(GLfloat nx, GLfloat ny, GLfloat nz));
#endif

#ifndef skip_glNormal3x
void APIENTRY_GL4ES gl4es_glNormal3x(GLfixed nx, GLfixed ny, GLfixed nz) {
    LOAD_GLES(glNormal3x);
#ifndef direct_glNormal3x
    PUSH_IF_COMPILING(glNormal3x)
#endif
    if(likely(gles_glNormal3x)) gles_glNormal3x(nx, ny, nz);
}
AliasExport(void,glNormal3x,,(GLfixed nx, GLfixed ny, GLfixed nz));
#endif

#ifndef skip_glNormalPointer
void APIENTRY_GL4ES gl4es_glNormalPointer(GLenum type, GLsizei stride, const GLvoid * pointer) {
    LOAD_GLES(glNormalPointer);
#ifndef direct_glNormalPointer
    PUSH_IF_COMPILING(glNormalPointer)
#endif
    if(likely(gles_glNormalPointer)) gles_glNormalPointer(type, stride, pointer);
}
AliasExport(void,glNormalPointer,,(GLenum type, GLsizei stride, const GLvoid * pointer));
#endif

#ifndef skip_glOrthof
void APIENTRY_GL4ES gl4es_glOrthof(GLfloat left, GLfloat right, GLfloat bottom, GLfloat top, GLfloat Near, GLfloat Far) {
    LOAD_GLES(glOrthof);
#ifndef direct_glOrthof
    PUSH_IF_COMPILING(glOrthof)
#endif
    if(likely(gles_glOrthof)) gles_glOrthof(left, right, bottom, top, Near, Far);
}
AliasExport(void,glOrthof,,(GLfloat left, GLfloat right, GLfloat bottom, GLfloat top, GLfloat Near, GLfloat Far));
#endif

#ifndef skip_glOrthox
void APIENTRY_GL4ES gl4es_glOrthox(GLfixed left, GLfixed right, GLfixed bottom, GLfixed top, GLfixed Near, GLfixed Far) {
    LOAD_GLES(glOrthox);
#ifndef direct_glOrthox
    PUSH_IF_COMPILING(glOrthox)
#endif
    if(likely(gles_glOrthox)) gles_glOrthox(left, right, bottom, top, Near, Far);
}
AliasExport(void,glOrthox,,(GLfixed left, GLfixed right, GLfixed bottom, GLfixed top, GLfixed Near, GLfixed Far));
#endif

#ifndef skip_glPixelStorei
void APIENTRY_GL4ES gl4es_glPixelStorei(GLenum pname, GLint param) {
    LOAD_GLES(glPixelStorei);
#ifndef direct_glPixelStorei
    PUSH_IF_COMPILING(glPixelStorei)
#endif
    if(likely(gles_glPixelStorei)) gles_glPixelStorei(pname, param);
}
AliasExport(void,glPixelStorei,,(GLenum pname, GLint param));
#endif

#ifndef skip_glPointParameterf
void APIENTRY_GL4ES gl4es_glPointParameterf(GLenum pname, GLfloat param) {
    LOAD_GLES(glPointParameterf);
#ifndef direct_glPointParameterf
    PUSH_IF_COMPILING(glPointParameterf)
#endif
    if(likely(gles_glPointParameterf)) gles_glPointParameterf(pname, param);
}
AliasExport(void,glPointParameterf,,(GLenum pname, GLfloat param));
#endif

#ifndef skip_glPointParameterfv
void APIENTRY_GL4ES gl4es_glPointParameterfv(GLenum pname, const GLfloat * params) {
    LOAD_GLES(glPointParameterfv);
#ifndef direct_glPointParameterfv
    PUSH_IF_COMPILING(glPointParameterfv)
#endif
    if(likely(gles_glPointParameterfv)) gles_glPointParameterfv(pname, params);
}
AliasExport(void,glPointParameterfv,,(GLenum pname, const GLfloat * params));
#endif

#ifndef skip_glPointParameterx
void APIENTRY_GL4ES gl4es_glPointParameterx(GLenum pname, GLfixed param) {
    LOAD_GLES(glPointParameterx);
#ifndef direct_glPointParameterx
    PUSH_IF_COMPILING(glPointParameterx)
#endif
    if(likely(gles_glPointParameterx)) gles_glPointParameterx(pname, param);
}
AliasExport(void,glPointParameterx,,(GLenum pname, GLfixed param));
#endif

#ifndef skip_glPointParameterxv
void APIENTRY_GL4ES gl4es_glPointParameterxv(GLenum pname, const GLfixed * params) {
    LOAD_GLES(glPointParameterxv);
#ifndef direct_glPointParameterxv
    PUSH_IF_COMPILING(glPointParameterxv)
#endif
    if(likely(gles_glPointParameterxv)) gles_glPointParameterxv(pname, params);
}
AliasExport(void,glPointParameterxv,,(GLenum pname, const GLfixed * params));
#endif

#ifndef skip_glPointSize
void APIENTRY_GL4ES gl4es_glPointSize(GLfloat size) {
    LOAD_GLES(glPointSize);
#ifndef direct_glPointSize
    PUSH_IF_COMPILING(glPointSize)
#endif
    if(likely(gles_glPointSize)) gles_glPointSize(size);
}
AliasExport(void,glPointSize,,(GLfloat size));
#endif

#ifndef skip_glPointSizePointerOES
void APIENTRY_GL4ES gl4es_glPointSizePointerOES(GLenum type, GLsizei stride, const GLvoid * pointer) {
    LOAD_GLES(glPointSizePointerOES);
#ifndef direct_glPointSizePointerOES
    PUSH_IF_COMPILING(glPointSizePointerOES)
#endif
    if(likely(gles_glPointSizePointerOES)) gles_glPointSizePointerOES(type, stride, pointer);
}
AliasExport(void,glPointSizePointerOES,,(GLenum type, GLsizei stride, const GLvoid * pointer));
#endif

#ifndef skip_glPointSizex
void APIENTRY_GL4ES gl4es_glPointSizex(GLfixed size) {
    LOAD_GLES(glPointSizex);
#ifndef direct_glPointSizex
    PUSH_IF_COMPILING(glPointSizex)
#endif
    if(likely(gles_glPointSizex)) gles_glPointSizex(size);
}
AliasExport(void,glPointSizex,,(GLfixed size));
#endif

#ifndef skip_glPolygonOffset
void APIENTRY_GL4ES gl4es_glPolygonOffset(GLfloat factor, GLfloat units) {
    LOAD_GLES(glPolygonOffset);
#ifndef direct_glPolygonOffset
    PUSH_IF_COMPILING(glPolygonOffset)
#endif
    if(likely(gles_glPolygonOffset)) gles_glPolygonOffset(factor, units);
}
AliasExport(void,glPolygonOffset,,(GLfloat factor, GLfloat units));
#endif

#ifndef skip_glPolygonOffsetx
void APIENTRY_GL4ES gl4es_glPolygonOffsetx(GLfixed factor, GLfixed units) {
    LOAD_GLES(glPolygonOffsetx);
#ifndef direct_glPolygonOffsetx
    PUSH_IF_COMPILING(glPolygonOffsetx)
#endif
    if(likely(gles_glPolygonOffsetx)) gles_glPolygonOffsetx(factor, units);
}
AliasExport(void,glPolygonOffsetx,,(GLfixed factor, GLfixed units));
#endif

#ifndef skip_glPopMatrix
void APIENTRY_GL4ES gl4es_glPopMatrix(void) {
    LOAD_GLES(glPopMatrix);
#ifndef direct_glPopMatrix
    PUSH_IF_COMPILING(glPopMatrix)
#endif
    if(likely(gles_glPopMatrix)) gles_glPopMatrix();
}
AliasExport(void,glPopMatrix,,());
#endif

#ifndef skip_glProgramBinary
void APIENTRY_GL4ES gl4es_glProgramBinary(GLuint program, GLenum binaryFormat, const GLvoid * binary, GLint length) {
    LOAD_GLES_OES(glProgramBinary);
#ifndef direct_glProgramBinary
    PUSH_IF_COMPILING(glProgramBinary)
#endif
    if(likely(gles_glProgramBinary)) gles_glProgramBinary(program, binaryFormat, binary, length);
}
AliasExport(void,glProgramBinary,,(GLuint program, GLenum binaryFormat, const GLvoid * binary, GLint length));
#endif

#ifndef skip_glPushMatrix
void APIENTRY_GL4ES gl4es_glPushMatrix(void) {
    LOAD_GLES(glPushMatrix);
#ifndef direct_glPushMatrix
    PUSH_IF_COMPILING(glPushMatrix)
#endif
    if(likely(gles_glPushMatrix)) gles_glPushMatrix();
}
AliasExport(void,glPushMatrix,,());
#endif

#ifndef skip_glReadPixels
void APIENTRY_GL4ES gl4es_glReadPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLvoid * pixels) {
    LOAD_GLES(glReadPixels);
#ifndef direct_glReadPixels
    PUSH_IF_COMPILING(glReadPixels)
#endif
    if(likely(gles_glReadPixels)) gles_glReadPixels(x, y, width, height, format, type, pixels);
}
AliasExport(void,glReadPixels,,(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLvoid * pixels));
#endif

#ifndef skip_glReleaseShaderCompiler
void APIENTRY_GL4ES gl4es_glReleaseShaderCompiler() {
    LOAD_GLES(glReleaseShaderCompiler);
#ifndef direct_glReleaseShaderCompiler
    PUSH_IF_COMPILING(glReleaseShaderCompiler)
#endif
    if(likely(gles_glReleaseShaderCompiler)) gles_glReleaseShaderCompiler();
}
AliasExport(void,glReleaseShaderCompiler,,());
#endif

#ifndef skip_glRenderbufferStorage
void APIENTRY_GL4ES gl4es_glRenderbufferStorage(GLenum target, GLenum internalformat, GLsizei width, GLsizei height) {
    LOAD_GLES_OES(glRenderbufferStorage);
#ifndef direct_glRenderbufferStorage
    PUSH_IF_COMPILING(glRenderbufferStorage)
#endif
    if(likely(gles_glRenderbufferStorage)) gles_glRenderbufferStorage(target, internalformat, width, height);
}
AliasExport(void,glRenderbufferStorage,,(GLenum target, GLenum internalformat, GLsizei width, GLsizei height));
#endif

#ifndef skip_glRotatef
void APIENTRY_GL4ES gl4es_glRotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z) {
    LOAD_GLES(glRotatef);
#ifndef direct_glRotatef
    PUSH_IF_COMPILING(glRotatef)
#endif
    if(likely(gles_glRotatef)) gles_glRotatef(angle, x, y, z);
}
AliasExport(void,glRotatef,,(GLfloat angle, GLfloat x, GLfloat y, GLfloat z));
#endif

#ifndef skip_glRotatex
void APIENTRY_GL4ES gl4es_glRotatex(GLfixed angle, GLfixed x, GLfixed y, GLfixed z) {
    LOAD_GLES(glRotatex);
#ifndef direct_glRotatex
    PUSH_IF_COMPILING(glRotatex)
#endif
    if(likely(gles_glRotatex)) gles_glRotatex(angle, x, y, z);
}
AliasExport(void,glRotatex,,(GLfixed angle, GLfixed x, GLfixed y, GLfixed z));
#endif

#ifndef skip_glSampleCoverage
void APIENTRY_GL4ES gl4es_glSampleCoverage(GLclampf value, GLboolean invert) {
    LOAD_GLES(glSampleCoverage);
#ifndef direct_glSampleCoverage
    PUSH_IF_COMPILING(glSampleCoverage)
#endif
    if(likely(gles_glSampleCoverage)) gles_glSampleCoverage(value, invert);
}
AliasExport(void,glSampleCoverage,,(GLclampf value, GLboolean invert));
#endif

#ifndef skip_glSampleCoveragex
void APIENTRY_GL4ES gl4es_glSampleCoveragex(GLclampx value, GLboolean invert) {
    LOAD_GLES(glSampleCoveragex);
#ifndef direct_glSampleCoveragex
    PUSH_IF_COMPILING(glSampleCoveragex)
#endif
    if(likely(gles_glSampleCoveragex)) gles_glSampleCoveragex(value, invert);
}
AliasExport(void,glSampleCoveragex,,(GLclampx value, GLboolean invert));
#endif

#ifndef skip_glScalef
void APIENTRY_GL4ES gl4es_glScalef(GLfloat x, GLfloat y, GLfloat z) {
    LOAD_GLES(glScalef);
#ifndef direct_glScalef
    PUSH_IF_COMPILING(glScalef)
#endif
    if(likely(gles_glScalef)) gles_glScalef(x, y, z);
}
AliasExport(void,glScalef,,(GLfloat x, GLfloat y, GLfloat z));
#endif

#ifndef skip_glScalex
void APIENTRY_GL4ES gl4es_glScalex(GLfixed x, GLfixed y, GLfixed z) {
    LOAD_GLES(glScalex);
#ifndef direct_glScalex
    PUSH_IF_COMPILING(glScalex)
#endif
    if(likely(gles_glScalex)) gles_glScalex(x, y, z);
}
AliasExport(void,glScalex,,(GLfixed x, GLfixed y, GLfixed z));
#endif

#ifndef skip_glScissor
void APIENTRY_GL4ES gl4es_glScissor(GLint x, GLint y, GLsizei width, GLsizei height) {
    LOAD_GLES(glScissor);
#ifndef direct_glScissor
    PUSH_IF_COMPILING(glScissor)
#endif
    if(likely(gles_glScissor)) gles_glScissor(x, y, width, height);
}
AliasExport(void,glScissor,,(GLint x, GLint y, GLsizei width, GLsizei height));
#endif

#ifndef skip_glShadeModel
void APIENTRY_GL4ES gl4es_glShadeModel(GLenum mode) {
    LOAD_GLES(glShadeModel);
#ifndef direct_glShadeModel
    PUSH_IF_COMPILING(glShadeModel)
#endif
    if(likely(gles_glShadeModel)) gles_glShadeModel(mode);
}
AliasExport(void,glShadeModel,,(GLenum mode));
#endif

#ifndef skip_glShaderBinary
void APIENTRY_GL4ES gl4es_glShaderBinary(GLsizei n, const GLuint * shaders, GLenum binaryformat, const GLvoid * binary, GLsizei length) {
    LOAD_GLES(glShaderBinary);
#ifndef direct_glShaderBinary
    PUSH_IF_COMPILING(glShaderBinary)
#endif
    if(likely(gles_glShaderBinary)) gles_glShaderBinary(n, shaders, binaryformat, binary, length);
}
AliasExport(void,glShaderBinary,,(GLsizei n, const GLuint * shaders, GLenum binaryformat, const GLvoid * binary, GLsizei length));
#endif

#ifndef skip_glShaderSource
void APIENTRY_GL4ES gl4es_glShaderSource(GLuint shader, GLsizei count, const GLchar * const * string, const GLint * length) {
    LOAD_GLES(glShaderSource);
#ifndef direct_glShaderSource
    PUSH_IF_COMPILING(glShaderSource)
#endif
    if(likely(gles_glShaderSource)) gles_glShaderSource(shader, count, string, length);
}
AliasExport(void,glShaderSource,,(GLuint shader, GLsizei count, const GLchar * const * string, const GLint * length));
#endif

#ifndef skip_glStencilFunc
void APIENTRY_GL4ES gl4es_glStencilFunc(GLenum func, GLint ref, GLuint mask) {
    LOAD_GLES(glStencilFunc);
#ifndef direct_glStencilFunc
    PUSH_IF_COMPILING(glStencilFunc)
#endif
    if(likely(gles_glStencilFunc)) gles_glStencilFunc(func, ref, mask);
}
AliasExport(void,glStencilFunc,,(GLenum func, GLint ref, GLuint mask));
#endif

#ifndef skip_glStencilFuncSeparate
void APIENTRY_GL4ES gl4es_glStencilFuncSeparate(GLenum face, GLenum func, GLint ref, GLuint mask) {
    LOAD_GLES(glStencilFuncSeparate);
#ifndef direct_glStencilFuncSeparate
    PUSH_IF_COMPILING(glStencilFuncSeparate)
#endif
    if(likely(gles_glStencilFuncSeparate)) gles_glStencilFuncSeparate(face, func, ref, mask);
}
AliasExport(void,glStencilFuncSeparate,,(GLenum face, GLenum func, GLint ref, GLuint mask));
#endif

#ifndef skip_glStencilMask
void APIENTRY_GL4ES gl4es_glStencilMask(GLuint mask) {
    LOAD_GLES(glStencilMask);
#ifndef direct_glStencilMask
    PUSH_IF_COMPILING(glStencilMask)
#endif
    if(likely(gles_glStencilMask)) gles_glStencilMask(mask);
}
AliasExport(void,glStencilMask,,(GLuint mask));
#endif

#ifndef skip_glStencilMaskSeparate
void APIENTRY_GL4ES gl4es_glStencilMaskSeparate(GLenum face, GLuint mask) {
    LOAD_GLES(glStencilMaskSeparate);
#ifndef direct_glStencilMaskSeparate
    PUSH_IF_COMPILING(glStencilMaskSeparate)
#endif
    if(likely(gles_glStencilMaskSeparate)) gles_glStencilMaskSeparate(face, mask);
}
AliasExport(void,glStencilMaskSeparate,,(GLenum face, GLuint mask));
#endif

#ifndef skip_glStencilOp
void APIENTRY_GL4ES gl4es_glStencilOp(GLenum fail, GLenum zfail, GLenum zpass) {
    LOAD_GLES(glStencilOp);
#ifndef direct_glStencilOp
    PUSH_IF_COMPILING(glStencilOp)
#endif
    if(likely(gles_glStencilOp)) gles_glStencilOp(fail, zfail, zpass);
}
AliasExport(void,glStencilOp,,(GLenum fail, GLenum zfail, GLenum zpass));
#endif

#ifndef skip_glStencilOpSeparate
void APIENTRY_GL4ES gl4es_glStencilOpSeparate(GLenum face, GLenum sfail, GLenum zfail, GLenum zpass) {
    LOAD_GLES(glStencilOpSeparate);
#ifndef direct_glStencilOpSeparate
    PUSH_IF_COMPILING(glStencilOpSeparate)
#endif
    if(likely(gles_glStencilOpSeparate)) gles_glStencilOpSeparate(face, sfail, zfail, zpass);
}
AliasExport(void,glStencilOpSeparate,,(GLenum face, GLenum sfail, GLenum zfail, GLenum zpass));
#endif

#ifndef skip_glTexCoordPointer
void APIENTRY_GL4ES gl4es_glTexCoordPointer(GLint size, GLenum type, GLsizei stride, const GLvoid * pointer) {
    LOAD_GLES(glTexCoordPointer);
#ifndef direct_glTexCoordPointer
    PUSH_IF_COMPILING(glTexCoordPointer)
#endif
    if(likely(gles_glTexCoordPointer)) gles_glTexCoordPointer(size, type, stride, pointer);
}
AliasExport(void,glTexCoordPointer,,(GLint size, GLenum type, GLsizei stride, const GLvoid * pointer));
#endif

#ifndef skip_glTexEnvf
void APIENTRY_GL4ES gl4es_glTexEnvf(GLenum target, GLenum pname, GLfloat param) {
    LOAD_GLES(glTexEnvf);
#ifndef direct_glTexEnvf
    PUSH_IF_COMPILING(glTexEnvf)
#endif
    if(likely(gles_glTexEnvf)) gles_glTexEnvf(target, pname, param);
}
AliasExport(void,glTexEnvf,,(GLenum target, GLenum pname, GLfloat param));
#endif

#ifndef skip_glTexEnvfv
void APIENTRY_GL4ES gl4es_glTexEnvfv(GLenum target, GLenum pname, const GLfloat * params) {
    LOAD_GLES(glTexEnvfv);
#ifndef direct_glTexEnvfv
    PUSH_IF_COMPILING(glTexEnvfv)
#endif
    if(likely(gles_glTexEnvfv)) gles_glTexEnvfv(target, pname, params);
}
AliasExport(void,glTexEnvfv,,(GLenum target, GLenum pname, const GLfloat * params));
#endif

#ifndef skip_glTexEnvi
void APIENTRY_GL4ES gl4es_glTexEnvi(GLenum target, GLenum pname, GLint param) {
    LOAD_GLES(glTexEnvi);
#ifndef direct_glTexEnvi
    PUSH_IF_COMPILING(glTexEnvi)
#endif
    if(likely(gles_glTexEnvi)) gles_glTexEnvi(target, pname, param);
}
AliasExport(void,glTexEnvi,,(GLenum target, GLenum pname, GLint param));
#endif

#ifndef skip_glTexEnviv
void APIENTRY_GL4ES gl4es_glTexEnviv(GLenum target, GLenum pname, const GLint * params) {
    LOAD_GLES(glTexEnviv);
#ifndef direct_glTexEnviv
    PUSH_IF_COMPILING(glTexEnviv)
#endif
    if(likely(gles_glTexEnviv)) gles_glTexEnviv(target, pname, params);
}
AliasExport(void,glTexEnviv,,(GLenum target, GLenum pname, const GLint * params));
#endif

#ifndef skip_glTexEnvx
void APIENTRY_GL4ES gl4es_glTexEnvx(GLenum target, GLenum pname, GLfixed param) {
    LOAD_GLES(glTexEnvx);
#ifndef direct_glTexEnvx
    PUSH_IF_COMPILING(glTexEnvx)
#endif
    if(likely(gles_glTexEnvx)) gles_glTexEnvx(target, pname, param);
}
AliasExport(void,glTexEnvx,,(GLenum target, GLenum pname, GLfixed param));
#endif

#ifndef skip_glTexEnvxv
void APIENTRY_GL4ES gl4es_glTexEnvxv(GLenum target, GLenum pname, const GLfixed * params) {
    LOAD_GLES(glTexEnvxv);
#ifndef direct_glTexEnvxv
    PUSH_IF_COMPILING(glTexEnvxv)
#endif
    if(likely(gles_glTexEnvxv)) gles_glTexEnvxv(target, pname, params);
}
AliasExport(void,glTexEnvxv,,(GLenum target, GLenum pname, const GLfixed * params));
#endif

#ifndef skip_glTexGenfv
void APIENTRY_GL4ES gl4es_glTexGenfv(GLenum coord, GLenum pname, const GLfloat * params) {
    LOAD_GLES_OES(glTexGenfv);
#ifndef direct_glTexGenfv
    PUSH_IF_COMPILING(glTexGenfv)
#endif
    if(likely(gles_glTexGenfv)) gles_glTexGenfv(coord, pname, params);
}
AliasExport(void,glTexGenfv,,(GLenum coord, GLenum pname, const GLfloat * params));
#endif

#ifndef skip_glTexGeni
void APIENTRY_GL4ES gl4es_glTexGeni(GLenum coord, GLenum pname, GLint param) {
    LOAD_GLES_OES(glTexGeni);
#ifndef direct_glTexGeni
    PUSH_IF_COMPILING(glTexGeni)
#endif
    if(likely(gles_glTexGeni)) gles_glTexGeni(coord, pname, param);
}
AliasExport(void,glTexGeni,,(GLenum coord, GLenum pname, GLint param));
#endif

#ifndef skip_glTexImage2D
void APIENTRY_GL4ES gl4es_glTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid * data) {
    LOAD_GLES(glTexImage2D);
#ifndef direct_glTexImage2D
    PUSH_IF_COMPILING(glTexImage2D)
#endif
    if(likely(gles_glTexImage2D)) gles_glTexImage2D(target, level, internalformat, width, height, border, format, type, data);
}
AliasExport(void,glTexImage2D,,(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid * data));
#endif

#ifndef skip_glTexParameterf
void APIENTRY_GL4ES gl4es_glTexParameterf(GLenum target, GLenum pname, GLfloat param) {
    LOAD_GLES(glTexParameterf);
#ifndef direct_glTexParameterf
    PUSH_IF_COMPILING(glTexParameterf)
#endif
    if(likely(gles_glTexParameterf)) gles_glTexParameterf(target, pname, param);
}
AliasExport(void,glTexParameterf,,(GLenum target, GLenum pname, GLfloat param));
#endif

#ifndef skip_glTexParameterfv
void APIENTRY_GL4ES gl4es_glTexParameterfv(GLenum target, GLenum pname, const GLfloat * params) {
    LOAD_GLES(glTexParameterfv);
#ifndef direct_glTexParameterfv
    PUSH_IF_COMPILING(glTexParameterfv)
#endif
    if(likely(gles_glTexParameterfv)) gles_glTexParameterfv(target, pname, params);
}
AliasExport(void,glTexParameterfv,,(GLenum target, GLenum pname, const GLfloat * params));
#endif

#ifndef skip_glTexParameteri
void APIENTRY_GL4ES gl4es_glTexParameteri(GLenum target, GLenum pname, GLint param) {
    // Calls internal emulated wrapper at top of file
    void gles_glTexParameteri(glTexParameteri_ARG_EXPAND); 
#ifndef direct_glTexParameteri
    PUSH_IF_COMPILING(glTexParameteri)
#endif
    gles_glTexParameteri(target, pname, param);
}
AliasExport(void,glTexParameteri,,(GLenum target, GLenum pname, GLint param));
#endif

#ifndef skip_glTexParameteriv
void APIENTRY_GL4ES gl4es_glTexParameteriv(GLenum target, GLenum pname, const GLint * params) {
    LOAD_GLES(glTexParameteriv);
#ifndef direct_glTexParameteriv
    PUSH_IF_COMPILING(glTexParameteriv)
#endif
    if(likely(gles_glTexParameteriv)) gles_glTexParameteriv(target, pname, params);
}
AliasExport(void,glTexParameteriv,,(GLenum target, GLenum pname, const GLint * params));
#endif

#ifndef skip_glTexParameterx
void APIENTRY_GL4ES gl4es_glTexParameterx(GLenum target, GLenum pname, GLfixed param) {
    LOAD_GLES(glTexParameterx);
#ifndef direct_glTexParameterx
    PUSH_IF_COMPILING(glTexParameterx)
#endif
    if(likely(gles_glTexParameterx)) gles_glTexParameterx(target, pname, param);
}
AliasExport(void,glTexParameterx,,(GLenum target, GLenum pname, GLfixed param));
#endif

#ifndef skip_glTexParameterxv
void APIENTRY_GL4ES gl4es_glTexParameterxv(GLenum target, GLenum pname, const GLfixed * params) {
    LOAD_GLES(glTexParameterxv);
#ifndef direct_glTexParameterxv
    PUSH_IF_COMPILING(glTexParameterxv)
#endif
    if(likely(gles_glTexParameterxv)) gles_glTexParameterxv(target, pname, params);
}
AliasExport(void,glTexParameterxv,,(GLenum target, GLenum pname, const GLfixed * params));
#endif

#ifndef skip_glTexSubImage2D
void APIENTRY_GL4ES gl4es_glTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid * data) {
    LOAD_GLES(glTexSubImage2D);
#ifndef direct_glTexSubImage2D
    PUSH_IF_COMPILING(glTexSubImage2D)
#endif
    if(likely(gles_glTexSubImage2D)) gles_glTexSubImage2D(target, level, xoffset, yoffset, width, height, format, type, data);
}
AliasExport(void,glTexSubImage2D,,(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid * data));
#endif

#ifndef skip_glTranslatef
void APIENTRY_GL4ES gl4es_glTranslatef(GLfloat x, GLfloat y, GLfloat z) {
    LOAD_GLES(glTranslatef);
#ifndef direct_glTranslatef
    PUSH_IF_COMPILING(glTranslatef)
#endif
    if(likely(gles_glTranslatef)) gles_glTranslatef(x, y, z);
}
AliasExport(void,glTranslatef,,(GLfloat x, GLfloat y, GLfloat z));
#endif

#ifndef skip_glTranslatex
void APIENTRY_GL4ES gl4es_glTranslatex(GLfixed x, GLfixed y, GLfixed z) {
    LOAD_GLES(glTranslatex);
#ifndef direct_glTranslatex
    PUSH_IF_COMPILING(glTranslatex)
#endif
    if(likely(gles_glTranslatex)) gles_glTranslatex(x, y, z);
}
AliasExport(void,glTranslatex,,(GLfixed x, GLfixed y, GLfixed z));
#endif
#ifndef skip_glUniform1f
void APIENTRY_GL4ES gl4es_glUniform1f(GLint location, GLfloat v0) {
    LOAD_GLES(glUniform1f);
#ifndef direct_glUniform1f
    PUSH_IF_COMPILING(glUniform1f)
#endif
    if(likely(gles_glUniform1f)) gles_glUniform1f(location, v0);
}
AliasExport(void,glUniform1f,,(GLint location, GLfloat v0));
#endif

#ifndef skip_glUniform1fv
void APIENTRY_GL4ES gl4es_glUniform1fv(GLint location, GLsizei count, const GLfloat * value) {
    LOAD_GLES(glUniform1fv);
#ifndef direct_glUniform1fv
    PUSH_IF_COMPILING(glUniform1fv)
#endif
    if(likely(gles_glUniform1fv)) gles_glUniform1fv(location, count, value);
}
AliasExport(void,glUniform1fv,,(GLint location, GLsizei count, const GLfloat * value));
#endif

#ifndef skip_glUniform1i
void APIENTRY_GL4ES gl4es_glUniform1i(GLint location, GLint v0) {
    LOAD_GLES(glUniform1i);
#ifndef direct_glUniform1i
    PUSH_IF_COMPILING(glUniform1i)
#endif
    if(likely(gles_glUniform1i)) gles_glUniform1i(location, v0);
}
AliasExport(void,glUniform1i,,(GLint location, GLint v0));
#endif

#ifndef skip_glUniform1iv
void APIENTRY_GL4ES gl4es_glUniform1iv(GLint location, GLsizei count, const GLint * value) {
    LOAD_GLES(glUniform1iv);
#ifndef direct_glUniform1iv
    PUSH_IF_COMPILING(glUniform1iv)
#endif
    if(likely(gles_glUniform1iv)) gles_glUniform1iv(location, count, value);
}
AliasExport(void,glUniform1iv,,(GLint location, GLsizei count, const GLint * value));
#endif

#ifndef skip_glUniform2f
void APIENTRY_GL4ES gl4es_glUniform2f(GLint location, GLfloat v0, GLfloat v1) {
    LOAD_GLES(glUniform2f);
#ifndef direct_glUniform2f
    PUSH_IF_COMPILING(glUniform2f)
#endif
    if(likely(gles_glUniform2f)) gles_glUniform2f(location, v0, v1);
}
AliasExport(void,glUniform2f,,(GLint location, GLfloat v0, GLfloat v1));
#endif

#ifndef skip_glUniform2fv
void APIENTRY_GL4ES gl4es_glUniform2fv(GLint location, GLsizei count, const GLfloat * value) {
    LOAD_GLES(glUniform2fv);
#ifndef direct_glUniform2fv
    PUSH_IF_COMPILING(glUniform2fv)
#endif
    if(likely(gles_glUniform2fv)) gles_glUniform2fv(location, count, value);
}
AliasExport(void,glUniform2fv,,(GLint location, GLsizei count, const GLfloat * value));
#endif

#ifndef skip_glUniform2i
void APIENTRY_GL4ES gl4es_glUniform2i(GLint location, GLint v0, GLint v1) {
    LOAD_GLES(glUniform2i);
#ifndef direct_glUniform2i
    PUSH_IF_COMPILING(glUniform2i)
#endif
    if(likely(gles_glUniform2i)) gles_glUniform2i(location, v0, v1);
}
AliasExport(void,glUniform2i,,(GLint location, GLint v0, GLint v1));
#endif

#ifndef skip_glUniform2iv
void APIENTRY_GL4ES gl4es_glUniform2iv(GLint location, GLsizei count, const GLint * value) {
    LOAD_GLES(glUniform2iv);
#ifndef direct_glUniform2iv
    PUSH_IF_COMPILING(glUniform2iv)
#endif
    if(likely(gles_glUniform2iv)) gles_glUniform2iv(location, count, value);
}
AliasExport(void,glUniform2iv,,(GLint location, GLsizei count, const GLint * value));
#endif

#ifndef skip_glUniform3f
void APIENTRY_GL4ES gl4es_glUniform3f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2) {
    LOAD_GLES(glUniform3f);
#ifndef direct_glUniform3f
    PUSH_IF_COMPILING(glUniform3f)
#endif
    if(likely(gles_glUniform3f)) gles_glUniform3f(location, v0, v1, v2);
}
AliasExport(void,glUniform3f,,(GLint location, GLfloat v0, GLfloat v1, GLfloat v2));
#endif

#ifndef skip_glUniform3fv
void APIENTRY_GL4ES gl4es_glUniform3fv(GLint location, GLsizei count, const GLfloat * value) {
    LOAD_GLES(glUniform3fv);
#ifndef direct_glUniform3fv
    PUSH_IF_COMPILING(glUniform3fv)
#endif
    if(likely(gles_glUniform3fv)) gles_glUniform3fv(location, count, value);
}
AliasExport(void,glUniform3fv,,(GLint location, GLsizei count, const GLfloat * value));
#endif

#ifndef skip_glUniform3i
void APIENTRY_GL4ES gl4es_glUniform3i(GLint location, GLint v0, GLint v1, GLint v2) {
    LOAD_GLES(glUniform3i);
#ifndef direct_glUniform3i
    PUSH_IF_COMPILING(glUniform3i)
#endif
    if(likely(gles_glUniform3i)) gles_glUniform3i(location, v0, v1, v2);
}
AliasExport(void,glUniform3i,,(GLint location, GLint v0, GLint v1, GLint v2));
#endif

#ifndef skip_glUniform3iv
void APIENTRY_GL4ES gl4es_glUniform3iv(GLint location, GLsizei count, const GLint * value) {
    LOAD_GLES(glUniform3iv);
#ifndef direct_glUniform3iv
    PUSH_IF_COMPILING(glUniform3iv)
#endif
    if(likely(gles_glUniform3iv)) gles_glUniform3iv(location, count, value);
}
AliasExport(void,glUniform3iv,,(GLint location, GLsizei count, const GLint * value));
#endif

#ifndef skip_glUniform4f
void APIENTRY_GL4ES gl4es_glUniform4f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3) {
    LOAD_GLES(glUniform4f);
#ifndef direct_glUniform4f
    PUSH_IF_COMPILING(glUniform4f)
#endif
    if(likely(gles_glUniform4f)) gles_glUniform4f(location, v0, v1, v2, v3);
}
AliasExport(void,glUniform4f,,(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3));
#endif

#ifndef skip_glUniform4fv
void APIENTRY_GL4ES gl4es_glUniform4fv(GLint location, GLsizei count, const GLfloat * value) {
    LOAD_GLES(glUniform4fv);
#ifndef direct_glUniform4fv
    PUSH_IF_COMPILING(glUniform4fv)
#endif
    if(likely(gles_glUniform4fv)) gles_glUniform4fv(location, count, value);
}
AliasExport(void,glUniform4fv,,(GLint location, GLsizei count, const GLfloat * value));
#endif

#ifndef skip_glUniform4i
void APIENTRY_GL4ES gl4es_glUniform4i(GLint location, GLint v0, GLint v1, GLint v2, GLint v3) {
    LOAD_GLES(glUniform4i);
#ifndef direct_glUniform4i
    PUSH_IF_COMPILING(glUniform4i)
#endif
    if(likely(gles_glUniform4i)) gles_glUniform4i(location, v0, v1, v2, v3);
}
AliasExport(void,glUniform4i,,(GLint location, GLint v0, GLint v1, GLint v2, GLint v3));
#endif

#ifndef skip_glUniform4iv
void APIENTRY_GL4ES gl4es_glUniform4iv(GLint location, GLsizei count, const GLint * value) {
    LOAD_GLES(glUniform4iv);
#ifndef direct_glUniform4iv
    PUSH_IF_COMPILING(glUniform4iv)
#endif
    if(likely(gles_glUniform4iv)) gles_glUniform4iv(location, count, value);
}
AliasExport(void,glUniform4iv,,(GLint location, GLsizei count, const GLint * value));
#endif
#ifndef skip_glUniformMatrix2fv
void APIENTRY_GL4ES gl4es_glUniformMatrix2fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat * value) {
    LOAD_GLES(glUniformMatrix2fv);
#ifndef direct_glUniformMatrix2fv
    PUSH_IF_COMPILING(glUniformMatrix2fv)
#endif
    if(likely(gles_glUniformMatrix2fv)) gles_glUniformMatrix2fv(location, count, transpose, value);
}
AliasExport(void,glUniformMatrix2fv,,(GLint location, GLsizei count, GLboolean transpose, const GLfloat * value));
#endif

#ifndef skip_glUniformMatrix3fv
void APIENTRY_GL4ES gl4es_glUniformMatrix3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat * value) {
    LOAD_GLES(glUniformMatrix3fv);
#ifndef direct_glUniformMatrix3fv
    PUSH_IF_COMPILING(glUniformMatrix3fv)
#endif
    if(likely(gles_glUniformMatrix3fv)) gles_glUniformMatrix3fv(location, count, transpose, value);
}
AliasExport(void,glUniformMatrix3fv,,(GLint location, GLsizei count, GLboolean transpose, const GLfloat * value));
#endif

#ifndef skip_glUniformMatrix4fv
void APIENTRY_GL4ES gl4es_glUniformMatrix4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat * value) {
    LOAD_GLES(glUniformMatrix4fv);
#ifndef direct_glUniformMatrix4fv
    PUSH_IF_COMPILING(glUniformMatrix4fv)
#endif
    if(likely(gles_glUniformMatrix4fv)) gles_glUniformMatrix4fv(location, count, transpose, value);
}
AliasExport(void,glUniformMatrix4fv,,(GLint location, GLsizei count, GLboolean transpose, const GLfloat * value));
#endif

#ifndef skip_glUseProgram
void APIENTRY_GL4ES gl4es_glUseProgram(GLuint program) {
    LOAD_GLES(glUseProgram);
#ifndef direct_glUseProgram
    PUSH_IF_COMPILING(glUseProgram)
#endif
    if(likely(gles_glUseProgram)) gles_glUseProgram(program);
}
AliasExport(void,glUseProgram,,(GLuint program));
#endif

#ifndef skip_glValidateProgram
void APIENTRY_GL4ES gl4es_glValidateProgram(GLuint program) {
    LOAD_GLES(glValidateProgram);
#ifndef direct_glValidateProgram
    PUSH_IF_COMPILING(glValidateProgram)
#endif
    if(likely(gles_glValidateProgram)) gles_glValidateProgram(program);
}
AliasExport(void,glValidateProgram,,(GLuint program));
#endif

#ifndef skip_glVertexAttrib1f
void APIENTRY_GL4ES gl4es_glVertexAttrib1f(GLuint index, GLfloat x) {
    LOAD_GLES(glVertexAttrib1f);
#ifndef direct_glVertexAttrib1f
    PUSH_IF_COMPILING(glVertexAttrib1f)
#endif
    if(likely(gles_glVertexAttrib1f)) gles_glVertexAttrib1f(index, x);
}
AliasExport(void,glVertexAttrib1f,,(GLuint index, GLfloat x));
#endif

#ifndef skip_glVertexAttrib1fv
void APIENTRY_GL4ES gl4es_glVertexAttrib1fv(GLuint index, const GLfloat * v) {
    LOAD_GLES(glVertexAttrib1fv);
#ifndef direct_glVertexAttrib1fv
    PUSH_IF_COMPILING(glVertexAttrib1fv)
#endif
    if(likely(gles_glVertexAttrib1fv)) gles_glVertexAttrib1fv(index, v);
}
AliasExport(void,glVertexAttrib1fv,,(GLuint index, const GLfloat * v));
#endif

#ifndef skip_glVertexAttrib2f
void APIENTRY_GL4ES gl4es_glVertexAttrib2f(GLuint index, GLfloat x, GLfloat y) {
    LOAD_GLES(glVertexAttrib2f);
#ifndef direct_glVertexAttrib2f
    PUSH_IF_COMPILING(glVertexAttrib2f)
#endif
    if(likely(gles_glVertexAttrib2f)) gles_glVertexAttrib2f(index, x, y);
}
AliasExport(void,glVertexAttrib2f,,(GLuint index, GLfloat x, GLfloat y));
#endif

#ifndef skip_glVertexAttrib2fv
void APIENTRY_GL4ES gl4es_glVertexAttrib2fv(GLuint index, const GLfloat * v) {
    LOAD_GLES(glVertexAttrib2fv);
#ifndef direct_glVertexAttrib2fv
    PUSH_IF_COMPILING(glVertexAttrib2fv)
#endif
    if(likely(gles_glVertexAttrib2fv)) gles_glVertexAttrib2fv(index, v);
}
AliasExport(void,glVertexAttrib2fv,,(GLuint index, const GLfloat * v));
#endif

#ifndef skip_glVertexAttrib3f
void APIENTRY_GL4ES gl4es_glVertexAttrib3f(GLuint index, GLfloat x, GLfloat y, GLfloat z) {
    LOAD_GLES(glVertexAttrib3f);
#ifndef direct_glVertexAttrib3f
    PUSH_IF_COMPILING(glVertexAttrib3f)
#endif
    if(likely(gles_glVertexAttrib3f)) gles_glVertexAttrib3f(index, x, y, z);
}
AliasExport(void,glVertexAttrib3f,,(GLuint index, GLfloat x, GLfloat y, GLfloat z));
#endif

#ifndef skip_glVertexAttrib3fv
void APIENTRY_GL4ES gl4es_glVertexAttrib3fv(GLuint index, const GLfloat * v) {
    LOAD_GLES(glVertexAttrib3fv);
#ifndef direct_glVertexAttrib3fv
    PUSH_IF_COMPILING(glVertexAttrib3fv)
#endif
    if(likely(gles_glVertexAttrib3fv)) gles_glVertexAttrib3fv(index, v);
}
AliasExport(void,glVertexAttrib3fv,,(GLuint index, const GLfloat * v));
#endif

#ifndef skip_glVertexAttrib4f
void APIENTRY_GL4ES gl4es_glVertexAttrib4f(GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w) {
    LOAD_GLES(glVertexAttrib4f);
#ifndef direct_glVertexAttrib4f
    PUSH_IF_COMPILING(glVertexAttrib4f)
#endif
    if(likely(gles_glVertexAttrib4f)) gles_glVertexAttrib4f(index, x, y, z, w);
}
AliasExport(void,glVertexAttrib4f,,(GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w));
#endif

#ifndef skip_glVertexAttrib4fv
void APIENTRY_GL4ES gl4es_glVertexAttrib4fv(GLuint index, const GLfloat * v) {
    LOAD_GLES(glVertexAttrib4fv);
#ifndef direct_glVertexAttrib4fv
    PUSH_IF_COMPILING(glVertexAttrib4fv)
#endif
    if(likely(gles_glVertexAttrib4fv)) gles_glVertexAttrib4fv(index, v);
}
AliasExport(void,glVertexAttrib4fv,,(GLuint index, const GLfloat * v));
#endif

#ifndef skip_glVertexAttribPointer
void APIENTRY_GL4ES gl4es_glVertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid * pointer) {
    LOAD_GLES(glVertexAttribPointer);
#ifndef direct_glVertexAttribPointer
    PUSH_IF_COMPILING(glVertexAttribPointer)
#endif
    if(likely(gles_glVertexAttribPointer)) gles_glVertexAttribPointer(index, size, type, normalized, stride, pointer);
}
AliasExport(void,glVertexAttribPointer,,(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid * pointer));
#endif

#ifndef skip_glVertexAttribIPointer
void APIENTRY_GL4ES gl4es_glVertexAttribIPointer(GLuint index, GLint size, GLenum type, GLsizei stride, const GLvoid * pointer) {
    LOAD_GLES(glVertexAttribIPointer);
#ifndef direct_glVertexAttribIPointer
    PUSH_IF_COMPILING(glVertexAttribIPointer)
#endif
    if(likely(gles_glVertexAttribIPointer)) gles_glVertexAttribIPointer(index, size, type, stride, pointer);
}
AliasExport(void,glVertexAttribIPointer,,(GLuint index, GLint size, GLenum type, GLsizei stride, const GLvoid * pointer));
#endif

#ifndef skip_glVertexPointer
void APIENTRY_GL4ES gl4es_glVertexPointer(GLint size, GLenum type, GLsizei stride, const GLvoid * pointer) {
    LOAD_GLES(glVertexPointer);
#ifndef direct_glVertexPointer
    PUSH_IF_COMPILING(glVertexPointer)
#endif
    if(likely(gles_glVertexPointer)) gles_glVertexPointer(size, type, stride, pointer);
}
AliasExport(void,glVertexPointer,,(GLint size, GLenum type, GLsizei stride, const GLvoid * pointer));
#endif

#ifndef skip_glViewport
void APIENTRY_GL4ES gl4es_glViewport(GLint x, GLint y, GLsizei width, GLsizei height) {
    LOAD_GLES(glViewport);
#ifndef direct_glViewport
    PUSH_IF_COMPILING(glViewport)
#endif
    if(likely(gles_glViewport)) gles_glViewport(x, y, width, height);
}
AliasExport(void,glViewport,,(GLint x, GLint y, GLsizei width, GLsizei height));
#endif

// The massive dispatcher for display lists
void APIENTRY_GL4ES glPackedCall(const packed_call_t *packed) {
    switch (packed->format) {
        case FORMAT_void_GLenum: {
            PACKED_void_GLenum *unpacked = (PACKED_void_GLenum *)packed;
            unpacked->func(unpacked->args.a1);
            break;
        }
        case FORMAT_void_GLenum_GLclampf: {
            PACKED_void_GLenum_GLclampf *unpacked = (PACKED_void_GLenum_GLclampf *)packed;
            unpacked->func(unpacked->args.a1, unpacked->args.a2);
            break;
        }
        case FORMAT_void_GLenum_GLclampx: {
            PACKED_void_GLenum_GLclampx *unpacked = (PACKED_void_GLenum_GLclampx *)packed;
            unpacked->func(unpacked->args.a1, unpacked->args.a2);
            break;
        }
        case FORMAT_void_GLuint_GLuint: {
            PACKED_void_GLuint_GLuint *unpacked = (PACKED_void_GLuint_GLuint *)packed;
            unpacked->func(unpacked->args.a1, unpacked->args.a2);
            break;
        }
        case FORMAT_void_GLuint_GLuint_const_GLchar___GENPT__: {
            PACKED_void_GLuint_GLuint_const_GLchar___GENPT__ *unpacked = (PACKED_void_GLuint_GLuint_const_GLchar___GENPT__ *)packed;
            unpacked->func(unpacked->args.a1, unpacked->args.a2, unpacked->args.a3);
            break;
        }
        case FORMAT_void_GLenum_GLuint: {
            PACKED_void_GLenum_GLuint *unpacked = (PACKED_void_GLenum_GLuint *)packed;
            unpacked->func(unpacked->args.a1, unpacked->args.a2);
            break;
        }
        case FORMAT_void_GLclampf_GLclampf_GLclampf_GLclampf: {
            PACKED_void_GLclampf_GLclampf_GLclampf_GLclampf *unpacked = (PACKED_void_GLclampf_GLclampf_GLclampf_GLclampf *)packed;
            unpacked->func(unpacked->args.a1, unpacked->args.a2, unpacked->args.a3, unpacked->args.a4);
            break;
        }
        case FORMAT_void_GLenum_GLenum: {
            PACKED_void_GLenum_GLenum *unpacked = (PACKED_void_GLenum_GLenum *)packed;
            unpacked->func(unpacked->args.a1, unpacked->args.a2);
            break;
        }
        case FORMAT_void_GLenum_GLenum_GLenum_GLenum: {
            PACKED_void_GLenum_GLenum_GLenum_GLenum *unpacked = (PACKED_void_GLenum_GLenum_GLenum_GLenum *)packed;
            unpacked->func(unpacked->args.a1, unpacked->args.a2, unpacked->args.a3, unpacked->args.a4);
            break;
        }
        case FORMAT_void_GLenum_GLsizeiptr_const_GLvoid___GENPT___GLenum: {
            PACKED_void_GLenum_GLsizeiptr_const_GLvoid___GENPT___GLenum *unpacked = (PACKED_void_GLenum_GLsizeiptr_const_GLvoid___GENPT___GLenum *)packed;
            unpacked->func(unpacked->args.a1, unpacked->args.a2, unpacked->args.a3, unpacked->args.a4);
            break;
        }
        case FORMAT_void_GLenum_GLintptr_GLsizeiptr_const_GLvoid___GENPT__: {
            PACKED_void_GLenum_GLintptr_GLsizeiptr_const_GLvoid___GENPT__ *unpacked = (PACKED_void_GLenum_GLintptr_GLsizeiptr_const_GLvoid___GENPT__ *)packed;
            unpacked->func(unpacked->args.a1, unpacked->args.a2, unpacked->args.a3, unpacked->args.a4);
            break;
        }
        case FORMAT_GLenum_GLenum: {
            PACKED_GLenum_GLenum *unpacked = (PACKED_GLenum_GLenum *)packed;
            unpacked->func(unpacked->args.a1);
            break;
        }
        case FORMAT_void_GLbitfield: {
            PACKED_void_GLbitfield *unpacked = (PACKED_void_GLbitfield *)packed;
            unpacked->func(unpacked->args.a1);
            break;
        }
        case FORMAT_void_GLclampx_GLclampx_GLclampx_GLclampx: {
            PACKED_void_GLclampx_GLclampx_GLclampx_GLclampx *unpacked = (PACKED_void_GLclampx_GLclampx_GLclampx_GLclampx *)packed;
            unpacked->func(unpacked->args.a1, unpacked->args.a2, unpacked->args.a3, unpacked->args.a4);
            break;
        }
        case FORMAT_void_GLclampf: {
            PACKED_void_GLclampf *unpacked = (PACKED_void_GLclampf *)packed;
            unpacked->func(unpacked->args.a1);
            break;
        }
        case FORMAT_void_GLclampx: {
            PACKED_void_GLclampx *unpacked = (PACKED_void_GLclampx *)packed;
            unpacked->func(unpacked->args.a1);
            break;
        }
        case FORMAT_void_GLint: {
            PACKED_void_GLint *unpacked = (PACKED_void_GLint *)packed;
            unpacked->func(unpacked->args.a1);
            break;
        }
        case FORMAT_void_GLenum_const_GLfloat___GENPT__: {
            PACKED_void_GLenum_const_GLfloat___GENPT__ *unpacked = (PACKED_void_GLenum_const_GLfloat___GENPT__ *)packed;
            unpacked->func(unpacked->args.a1, unpacked->args.a2);
            break;
        }
        case FORMAT_void_GLenum_const_GLfixed___GENPT__: {
            PACKED_void_GLenum_const_GLfixed___GENPT__ *unpacked = (PACKED_void_GLenum_const_GLfixed___GENPT__ *)packed;
            unpacked->func(unpacked->args.a1, unpacked->args.a2);
            break;
        }
        case FORMAT_void_GLfloat_GLfloat_GLfloat_GLfloat: {
            PACKED_void_GLfloat_GLfloat_GLfloat_GLfloat *unpacked = (PACKED_void_GLfloat_GLfloat_GLfloat_GLfloat *)packed;
            unpacked->func(unpacked->args.a1, unpacked->args.a2, unpacked->args.a3, unpacked->args.a4);
            break;
        }
        case FORMAT_void_GLubyte_GLubyte_GLubyte_GLubyte: {
            PACKED_void_GLubyte_GLubyte_GLubyte_GLubyte *unpacked = (PACKED_void_GLubyte_GLubyte_GLubyte_GLubyte *)packed;
            unpacked->func(unpacked->args.a1, unpacked->args.a2, unpacked->args.a3, unpacked->args.a4);
            break;
        }
        case FORMAT_void_GLfixed_GLfixed_GLfixed_GLfixed: {
            PACKED_void_GLfixed_GLfixed_GLfixed_GLfixed *unpacked = (PACKED_void_GLfixed_GLfixed_GLfixed_GLfixed *)packed;
            unpacked->func(unpacked->args.a1, unpacked->args.a2, unpacked->args.a3, unpacked->args.a4);
            break;
        }
        case FORMAT_void_GLboolean_GLboolean_GLboolean_GLboolean: {
            PACKED_void_GLboolean_GLboolean_GLboolean_GLboolean *unpacked = (PACKED_void_GLboolean_GLboolean_GLboolean_GLboolean *)packed;
            unpacked->func(unpacked->args.a1, unpacked->args.a2, unpacked->args.a3, unpacked->args.a4);
            break;
        }
        case FORMAT_void_GLint_GLenum_GLsizei_const_GLvoid___GENPT__: {
            PACKED_void_GLint_GLenum_GLsizei_const_GLvoid___GENPT__ *unpacked = (PACKED_void_GLint_GLenum_GLsizei_const_GLvoid___GENPT__ *)packed;
            unpacked->func(unpacked->args.a1, unpacked->args.a2, unpacked->args.a3, unpacked->args.a4);
            break;
        }
        case FORMAT_void_GLuint: {
            PACKED_void_GLuint *unpacked = (PACKED_void_GLuint *)packed;
            unpacked->func(unpacked->args.a1);
            break;
        }
        case FORMAT_void_GLenum_GLint_GLenum_GLsizei_GLsizei_GLint_GLsizei_const_GLvoid___GENPT__: {
            PACKED_void_GLenum_GLint_GLenum_GLsizei_GLsizei_GLint_GLsizei_const_GLvoid___GENPT__ *unpacked = (PACKED_void_GLenum_GLint_GLenum_GLsizei_GLsizei_GLint_GLsizei_const_GLvoid___GENPT__ *)packed;
            unpacked->func(unpacked->args.a1, unpacked->args.a2, unpacked->args.a3, unpacked->args.a4, unpacked->args.a5, unpacked->args.a6, unpacked->args.a7, unpacked->args.a8);
            break;
        }
        case FORMAT_void_GLenum_GLint_GLint_GLint_GLsizei_GLsizei_GLenum_GLsizei_const_GLvoid___GENPT__: {
            PACKED_void_GLenum_GLint_GLint_GLint_GLsizei_GLsizei_GLenum_GLsizei_const_GLvoid___GENPT__ *unpacked = (PACKED_void_GLenum_GLint_GLint_GLint_GLsizei_GLsizei_GLenum_GLsizei_const_GLvoid___GENPT__ *)packed;
            unpacked->func(unpacked->args.a1, unpacked->args.a2, unpacked->args.a3, unpacked->args.a4, unpacked->args.a5, unpacked->args.a6, unpacked->args.a7, unpacked->args.a8, unpacked->args.a9);
            break;
        }
        case FORMAT_void_GLenum_GLint_GLenum_GLint_GLint_GLsizei_GLsizei_GLint: {
            PACKED_void_GLenum_GLint_GLenum_GLint_GLint_GLsizei_GLsizei_GLint *unpacked = (PACKED_void_GLenum_GLint_GLenum_GLint_GLint_GLsizei_GLsizei_GLint *)packed;
            unpacked->func(unpacked->args.a1, unpacked->args.a2, unpacked->args.a3, unpacked->args.a4, unpacked->args.a5, unpacked->args.a6, unpacked->args.a7, unpacked->args.a8);
            break;
        }
        case FORMAT_void_GLenum_GLint_GLint_GLint_GLint_GLint_GLsizei_GLsizei: {
            PACKED_void_GLenum_GLint_GLint_GLint_GLint_GLint_GLsizei_GLsizei *unpacked = (PACKED_void_GLenum_GLint_GLint_GLint_GLint_GLint_GLsizei_GLsizei *)packed;
            unpacked->func(unpacked->args.a1, unpacked->args.a2, unpacked->args.a3, unpacked->args.a4, unpacked->args.a5, unpacked->args.a6, unpacked->args.a7, unpacked->args.a8);
            break;
        }
        case FORMAT_GLuint: {
            PACKED_GLuint *unpacked = (PACKED_GLuint *)packed;
            unpacked->func();
            break;
        }
        case FORMAT_GLuint_GLenum: {
            PACKED_GLuint_GLenum *unpacked = (PACKED_GLuint_GLenum *)packed;
            unpacked->func(unpacked->args.a1);
            break;
        }
        case FORMAT_void_GLsizei_const_GLuint___GENPT__: {
            PACKED_void_GLsizei_const_GLuint___GENPT__ *unpacked = (PACKED_void_GLsizei_const_GLuint___GENPT__ *)packed;
            unpacked->func(unpacked->args.a1, unpacked->args.a2);
            break;
        }
        case FORMAT_void_GLsizei_GLuint___GENPT__: {
            PACKED_void_GLsizei_GLuint___GENPT__ *unpacked = (PACKED_void_GLsizei_GLuint___GENPT__ *)packed;
            unpacked->func(unpacked->args.a1, unpacked->args.a2);
            break;
        }
        case FORMAT_void_GLboolean: {
            PACKED_void_GLboolean *unpacked = (PACKED_void_GLboolean *)packed;
            unpacked->func(unpacked->args.a1);
            break;
        }
        case FORMAT_void_GLclampf_GLclampf: {
            PACKED_void_GLclampf_GLclampf *unpacked = (PACKED_void_GLclampf_GLclampf *)packed;
            unpacked->func(unpacked->args.a1, unpacked->args.a2);
            break;
        }
        case FORMAT_void_GLclampx_GLclampx: {
            PACKED_void_GLclampx_GLclampx *unpacked = (PACKED_void_GLclampx_GLclampx *)packed;
            unpacked->func(unpacked->args.a1, unpacked->args.a2);
            break;
        }
        case FORMAT_void_GLenum_GLint_GLsizei: {
            PACKED_void_GLenum_GLint_GLsizei *unpacked = (PACKED_void_GLenum_GLint_GLsizei *)packed;
            unpacked->func(unpacked->args.a1, unpacked->args.a2, unpacked->args.a3);
            break;
        }
        case FORMAT_void_GLsizei_const_GLenum___GENPT__: {
            PACKED_void_GLsizei_const_GLenum___GENPT__ *unpacked = (PACKED_void_GLsizei_const_GLenum___GENPT__ *)packed;
            unpacked->func(unpacked->args.a1, unpacked->args.a2);
            break;
        }
        case FORMAT_void_GLenum_GLsizei_GLenum_const_GLvoid___GENPT__: {
            PACKED_void_GLenum_GLsizei_GLenum_const_GLvoid___GENPT__ *unpacked = (PACKED_void_GLenum_GLsizei_GLenum_const_GLvoid___GENPT__ *)packed;
            unpacked->func(unpacked->args.a1, unpacked->args.a2, unpacked->args.a3, unpacked->args.a4);
            break;
        }
        case FORMAT_void_GLfloat_GLfloat_GLfloat_GLfloat_GLfloat: {
            PACKED_void_GLfloat_GLfloat_GLfloat_GLfloat_GLfloat *unpacked = (PACKED_void_GLfloat_GLfloat_GLfloat_GLfloat_GLfloat *)packed;
            unpacked->func(unpacked->args.a1, unpacked->args.a2, unpacked->args.a3, unpacked->args.a4, unpacked->args.a5);
            break;
        }
        case FORMAT_void_GLint_GLint_GLint_GLint_GLint: {
            PACKED_void_GLint_GLint_GLint_GLint_GLint *unpacked = (PACKED_void_GLint_GLint_GLint_GLint_GLint *)packed;
            unpacked->func(unpacked->args.a1, unpacked->args.a2, unpacked->args.a3, unpacked->args.a4, unpacked->args.a5);
            break;
        }
        case FORMAT_void: {
            PACKED_void *unpacked = (PACKED_void *)packed;
            unpacked->func();
            break;
        }
        case FORMAT_void_GLenum_GLsizei_const_GLvoid___GENPT__: {
            PACKED_void_GLenum_GLsizei_const_GLvoid___GENPT__ *unpacked = (PACKED_void_GLenum_GLsizei_const_GLvoid___GENPT__ *)packed;
            unpacked->func(unpacked->args.a1, unpacked->args.a2, unpacked->args.a3);
            break;
        }
        case FORMAT_void_GLfloat: {
            PACKED_void_GLfloat *unpacked = (PACKED_void_GLfloat *)packed;
            unpacked->func(unpacked->args.a1);
            break;
        }
        case FORMAT_void_const_GLfloat___GENPT__: {
            PACKED_void_const_GLfloat___GENPT__ *unpacked = (PACKED_void_const_GLfloat___GENPT__ *)packed;
            unpacked->func(unpacked->args.a1);
            break;
        }
        case FORMAT_void_GLenum_GLfloat: {
            PACKED_void_GLenum_GLfloat *unpacked = (PACKED_void_GLenum_GLfloat *)packed;
            unpacked->func(unpacked->args.a1, unpacked->args.a2);
            break;
        }
        case FORMAT_void_GLenum_GLfixed: {
            PACKED_void_GLenum_GLfixed *unpacked = (PACKED_void_GLenum_GLfixed *)packed;
            unpacked->func(unpacked->args.a1, unpacked->args.a2);
            break;
        }
        case FORMAT_void_GLenum_GLenum_GLenum_GLuint: {
            PACKED_void_GLenum_GLenum_GLenum_GLuint *unpacked = (PACKED_void_GLenum_GLenum_GLenum_GLuint *)packed;
            unpacked->func(unpacked->args.a1, unpacked->args.a2, unpacked->args.a3, unpacked->args.a4);
            break;
        }
        case FORMAT_void_GLenum_GLenum_GLenum_GLuint_GLint: {
            PACKED_void_GLenum_GLenum_GLenum_GLuint_GLint *unpacked = (PACKED_void_GLenum_GLenum_GLenum_GLuint_GLint *)packed;
            unpacked->func(unpacked->args.a1, unpacked->args.a2, unpacked->args.a3, unpacked->args.a4, unpacked->args.a5);
            break;
        }
        case FORMAT_void_GLfloat_GLfloat_GLfloat_GLfloat_GLfloat_GLfloat: {
            PACKED_void_GLfloat_GLfloat_GLfloat_GLfloat_GLfloat_GLfloat *unpacked = (PACKED_void_GLfloat_GLfloat_GLfloat_GLfloat_GLfloat_GLfloat *)packed;
            unpacked->func(unpacked->args.a1, unpacked->args.a2, unpacked->args.a3, unpacked->args.a4, unpacked->args.a5, unpacked->args.a6);
            break;
        }
        case FORMAT_void_GLfixed_GLfixed_GLfixed_GLfixed_GLfixed_GLfixed: {
            PACKED_void_GLfixed_GLfixed_GLfixed_GLfixed_GLfixed_GLfixed *unpacked = (PACKED_void_GLfixed_GLfixed_GLfixed_GLfixed_GLfixed_GLfixed *)packed;
            unpacked->func(unpacked->args.a1, unpacked->args.a2, unpacked->args.a3, unpacked->args.a4, unpacked->args.a5, unpacked->args.a6);
            break;
        }
        case FORMAT_void_GLuint_GLuint_GLsizei_GLsizei___GENPT___GLint___GENPT___GLenum___GENPT___GLchar___GENPT__: {
            PACKED_void_GLuint_GLuint_GLsizei_GLsizei___GENPT___GLint___GENPT___GLenum___GENPT___GLchar___GENPT__ *unpacked = (PACKED_void_GLuint_GLuint_GLsizei_GLsizei___GENPT___GLint___GENPT___GLenum___GENPT___GLchar___GENPT__ *)packed;
            unpacked->func(unpacked->args.a1, unpacked->args.a2, unpacked->args.a3, unpacked->args.a4, unpacked->args.a5, unpacked->args.a6, unpacked->args.a7);
            break;
        }
        case FORMAT_void_GLuint_GLsizei_GLsizei___GENPT___GLuint___GENPT__: {
            PACKED_void_GLuint_GLsizei_GLsizei___GENPT___GLuint___GENPT__ *unpacked = (PACKED_void_GLuint_GLsizei_GLsizei___GENPT___GLuint___GENPT__ *)packed;
            unpacked->func(unpacked->args.a1, unpacked->args.a2, unpacked->args.a3, unpacked->args.a4);
            break;
        }
        case FORMAT_GLint_GLuint_const_GLchar___GENPT__: {
            PACKED_GLint_GLuint_const_GLchar___GENPT__ *unpacked = (PACKED_GLint_GLuint_const_GLchar___GENPT__ *)packed;
            unpacked->func(unpacked->args.a1, unpacked->args.a2);
            break;
        }
        case FORMAT_void_GLenum_GLboolean___GENPT__: {
            PACKED_void_GLenum_GLboolean___GENPT__ *unpacked = (PACKED_void_GLenum_GLboolean___GENPT__ *)packed;
            unpacked->func(unpacked->args.a1, unpacked->args.a2);
            break;
        }
        case FORMAT_void_GLenum_GLenum_GLint___GENPT__: {
            PACKED_void_GLenum_GLenum_GLint___GENPT__ *unpacked = (PACKED_void_GLenum_GLenum_GLint___GENPT__ *)packed;
            unpacked->func(unpacked->args.a1, unpacked->args.a2, unpacked->args.a3);
            break;
        }
        case FORMAT_void_GLenum_GLfloat___GENPT__: {
            PACKED_void_GLenum_GLfloat___GENPT__ *unpacked = (PACKED_void_GLenum_GLfloat___GENPT__ *)packed;
            unpacked->func(unpacked->args.a1, unpacked->args.a2);
            break;
        }
        case FORMAT_void_GLenum_GLfixed___GENPT__: {
            PACKED_void_GLenum_GLfixed___GENPT__ *unpacked = (PACKED_void_GLenum_GLfixed___GENPT__ *)packed;
            unpacked->func(unpacked->args.a1, unpacked->args.a2);
            break;
        }
        case FORMAT_GLenum: {
            PACKED_GLenum *unpacked = (PACKED_GLenum *)packed;
            unpacked->func();
            break;
        }
        case FORMAT_void_GLenum_GLenum_GLenum_GLint___GENPT__: {
            PACKED_void_GLenum_GLenum_GLenum_GLint___GENPT__ *unpacked = (PACKED_void_GLenum_GLenum_GLenum_GLint___GENPT__ *)packed;
            unpacked->func(unpacked->args.a1, unpacked->args.a2, unpacked->args.a3, unpacked->args.a4);
            break;
        }
        case FORMAT_void_GLenum_GLint___GENPT__: {
            PACKED_void_GLenum_GLint___GENPT__ *unpacked = (PACKED_void_GLenum_GLint___GENPT__ *)packed;
            unpacked->func(unpacked->args.a1, unpacked->args.a2);
            break;
        }
        case FORMAT_void_GLenum_GLenum_GLfloat___GENPT__: {
            PACKED_void_GLenum_GLenum_GLfloat___GENPT__ *unpacked = (PACKED_void_GLenum_GLenum_GLfloat___GENPT__ *)packed;
            unpacked->func(unpacked->args.a1, unpacked->args.a2, unpacked->args.a3);
            break;
        }
        case FORMAT_void_GLenum_GLenum_GLfixed___GENPT__: {
            PACKED_void_GLenum_GLenum_GLfixed___GENPT__ *unpacked = (PACKED_void_GLenum_GLenum_GLfixed___GENPT__ *)packed;
            unpacked->func(unpacked->args.a1, unpacked->args.a2, unpacked->args.a3);
            break;
        }
        case FORMAT_void_GLenum_GLvoid___GENPT____GENPT__: {
            PACKED_void_GLenum_GLvoid___GENPT____GENPT__ *unpacked = (PACKED_void_GLenum_GLvoid___GENPT____GENPT__ *)packed;
            unpacked->func(unpacked->args.a1, unpacked->args.a2);
            break;
        }
        case FORMAT_void_GLuint_GLsizei_GLsizei___GENPT___GLenum___GENPT___GLvoid___GENPT__: {
            PACKED_void_GLuint_GLsizei_GLsizei___GENPT___GLenum___GENPT___GLvoid___GENPT__ *unpacked = (PACKED_void_GLuint_GLsizei_GLsizei___GENPT___GLenum___GENPT___GLvoid___GENPT__ *)packed;
            unpacked->func(unpacked->args.a1, unpacked->args.a2, unpacked->args.a3, unpacked->args.a4, unpacked->args.a5);
            break;
        }
        case FORMAT_void_GLuint_GLsizei_GLsizei___GENPT___GLchar___GENPT__: {
            PACKED_void_GLuint_GLsizei_GLsizei___GENPT___GLchar___GENPT__ *unpacked = (PACKED_void_GLuint_GLsizei_GLsizei___GENPT___GLchar___GENPT__ *)packed;
            unpacked->func(unpacked->args.a1, unpacked->args.a2, unpacked->args.a3, unpacked->args.a4);
            break;
        }
        case FORMAT_void_GLuint_GLenum_GLint___GENPT__: {
            PACKED_void_GLuint_GLenum_GLint___GENPT__ *unpacked = (PACKED_void_GLuint_GLenum_GLint___GENPT__ *)packed;
            unpacked->func(unpacked->args.a1, unpacked->args.a2, unpacked->args.a3);
            break;
        }
        case FORMAT_void_GLenum_GLenum_GLint___GENPT___GLint___GENPT__: {
            PACKED_void_GLenum_GLenum_GLint___GENPT___GLint___GENPT__ *unpacked = (PACKED_void_GLenum_GLenum_GLint___GENPT___GLint___GENPT__ *)packed;
            unpacked->func(unpacked->args.a1, unpacked->args.a2, unpacked->args.a3, unpacked->args.a4);
            break;
        }
        case FORMAT_const_GLubyte___GENPT___GLenum: {
            PACKED_const_GLubyte___GENPT___GLenum *unpacked = (PACKED_const_GLubyte___GENPT___GLenum *)packed;
            unpacked->func(unpacked->args.a1);
            break;
        }
        case FORMAT_void_GLuint_GLint_GLfloat___GENPT__: {
            PACKED_void_GLuint_GLint_GLfloat___GENPT__ *unpacked = (PACKED_void_GLuint_GLint_GLfloat___GENPT__ *)packed;
            unpacked->func(unpacked->args.a1, unpacked->args.a2, unpacked->args.a3);
            break;
        }
        case FORMAT_void_GLuint_GLint_GLint___GENPT__: {
            PACKED_void_GLuint_GLint_GLint___GENPT__ *unpacked = (PACKED_void_GLuint_GLint_GLint___GENPT__ *)packed;
            unpacked->func(unpacked->args.a1, unpacked->args.a2, unpacked->args.a3);
            break;
        }
        case FORMAT_void_GLuint_GLenum_GLvoid___GENPT____GENPT__: {
            PACKED_void_GLuint_GLenum_GLvoid___GENPT____GENPT__ *unpacked = (PACKED_void_GLuint_GLenum_GLvoid___GENPT____GENPT__ *)packed;
            unpacked->func(unpacked->args.a1, unpacked->args.a2, unpacked->args.a3);
            break;
        }
        case FORMAT_void_GLuint_GLenum_GLfloat___GENPT__: {
            PACKED_void_GLuint_GLenum_GLfloat___GENPT__ *unpacked = (PACKED_void_GLuint_GLenum_GLfloat___GENPT__ *)packed;
            unpacked->func(unpacked->args.a1, unpacked->args.a2, unpacked->args.a3);
            break;
        }
        case FORMAT_GLboolean_GLuint: {
            PACKED_GLboolean_GLuint *unpacked = (PACKED_GLboolean_GLuint *)packed;
            unpacked->func(unpacked->args.a1);
            break;
        }
        case FORMAT_GLboolean_GLenum: {
            PACKED_GLboolean_GLenum *unpacked = (PACKED_GLboolean_GLenum *)packed;
            unpacked->func(unpacked->args.a1);
            break;
        }
        case FORMAT_void_GLenum_GLenum_GLfloat: {
            PACKED_void_GLenum_GLenum_GLfloat *unpacked = (PACKED_void_GLenum_GLenum_GLfloat *)packed;
            unpacked->func(unpacked->args.a1, unpacked->args.a2, unpacked->args.a3);
            break;
        }
        case FORMAT_void_GLenum_GLenum_const_GLfloat___GENPT__: {
            PACKED_void_GLenum_GLenum_const_GLfloat___GENPT__ *unpacked = (PACKED_void_GLenum_GLenum_const_GLfloat___GENPT__ *)packed;
            unpacked->func(unpacked->args.a1, unpacked->args.a2, unpacked->args.a3);
            break;
        }
        case FORMAT_void_GLenum_GLenum_GLfixed: {
            PACKED_void_GLenum_GLenum_GLfixed *unpacked = (PACKED_void_GLenum_GLenum_GLfixed *)packed;
            unpacked->func(unpacked->args.a1, unpacked->args.a2, unpacked->args.a3);
            break;
        }
        case FORMAT_void_GLenum_GLenum_const_GLfixed___GENPT__: {
            PACKED_void_GLenum_GLenum_const_GLfixed___GENPT__ *unpacked = (PACKED_void_GLenum_GLenum_const_GLfixed___GENPT__ *)packed;
            unpacked->func(unpacked->args.a1, unpacked->args.a2, unpacked->args.a3);
            break;
        }
        case FORMAT_void_GLfixed: {
            PACKED_void_GLfixed *unpacked = (PACKED_void_GLfixed *)packed;
            unpacked->func(unpacked->args.a1);
            break;
        }
        case FORMAT_void_const_GLfixed___GENPT__: {
            PACKED_void_const_GLfixed___GENPT__ *unpacked = (PACKED_void_const_GLfixed___GENPT__ *)packed;
            unpacked->func(unpacked->args.a1);
            break;
        }
        case FORMAT_void_GLenum_const_GLint___GENPT___const_GLsizei___GENPT___GLsizei: {
            PACKED_void_GLenum_const_GLint___GENPT___const_GLsizei___GENPT___GLsizei *unpacked = (PACKED_void_GLenum_const_GLint___GENPT___const_GLsizei___GENPT___GLsizei *)packed;
            unpacked->func(unpacked->args.a1, unpacked->args.a2, unpacked->args.a3, unpacked->args.a4);
            break;
        }
        case FORMAT_void_GLenum_GLsizei___GENPT___GLenum_const_void___GENPT___const___GENPT___GLsizei: {
            PACKED_void_GLenum_GLsizei___GENPT___GLenum_const_void___GENPT___const___GENPT___GLsizei *unpacked = (PACKED_void_GLenum_GLsizei___GENPT___GLenum_const_void___GENPT___const___GENPT___GLsizei *)packed;
            unpacked->func(unpacked->args.a1, unpacked->args.a2, unpacked->args.a3, unpacked->args.a4, unpacked->args.a5);
            break;
        }
        case FORMAT_void_GLenum_GLfloat_GLfloat_GLfloat_GLfloat: {
            PACKED_void_GLenum_GLfloat_GLfloat_GLfloat_GLfloat *unpacked = (PACKED_void_GLenum_GLfloat_GLfloat_GLfloat_GLfloat *)packed;
            unpacked->func(unpacked->args.a1, unpacked->args.a2, unpacked->args.a3, unpacked->args.a4, unpacked->args.a5);
            break;
        }
        case FORMAT_void_GLenum_GLfixed_GLfixed_GLfixed_GLfixed: {
            PACKED_void_GLenum_GLfixed_GLfixed_GLfixed_GLfixed *unpacked = (PACKED_void_GLenum_GLfixed_GLfixed_GLfixed_GLfixed *)packed;
            unpacked->func(unpacked->args.a1, unpacked->args.a2, unpacked->args.a3, unpacked->args.a4, unpacked->args.a5);
            break;
        }
        case FORMAT_void_GLfloat_GLfloat_GLfloat: {
            PACKED_void_GLfloat_GLfloat_GLfloat *unpacked = (PACKED_void_GLfloat_GLfloat_GLfloat *)packed;
            unpacked->func(unpacked->args.a1, unpacked->args.a2, unpacked->args.a3);
            break;
        }
        case FORMAT_void_GLfixed_GLfixed_GLfixed: {
            PACKED_void_GLfixed_GLfixed_GLfixed *unpacked = (PACKED_void_GLfixed_GLfixed_GLfixed *)packed;
            unpacked->func(unpacked->args.a1, unpacked->args.a2, unpacked->args.a3);
            break;
        }
        case FORMAT_void_GLenum_GLint: {
            PACKED_void_GLenum_GLint *unpacked = (PACKED_void_GLenum_GLint *)packed;
            unpacked->func(unpacked->args.a1, unpacked->args.a2);
            break;
        }
        case FORMAT_void_GLfloat_GLfloat: {
            PACKED_void_GLfloat_GLfloat *unpacked = (PACKED_void_GLfloat_GLfloat *)packed;
            unpacked->func(unpacked->args.a1, unpacked->args.a2);
            break;
        }
        case FORMAT_void_GLfixed_GLfixed: {
            PACKED_void_GLfixed_GLfixed *unpacked = (PACKED_void_GLfixed_GLfixed *)packed;
            unpacked->func(unpacked->args.a1, unpacked->args.a2);
            break;
        }
        case FORMAT_void_GLuint_GLenum_const_GLvoid___GENPT___GLint: {
            PACKED_void_GLuint_GLenum_const_GLvoid___GENPT___GLint *unpacked = (PACKED_void_GLuint_GLenum_const_GLvoid___GENPT___GLint *)packed;
            unpacked->func(unpacked->args.a1, unpacked->args.a2, unpacked->args.a3, unpacked->args.a4);
            break;
        }
        case FORMAT_void_GLint_GLint_GLsizei_GLsizei_GLenum_GLenum_GLvoid___GENPT__: {
            PACKED_void_GLint_GLint_GLsizei_GLsizei_GLenum_GLenum_GLvoid___GENPT__ *unpacked = (PACKED_void_GLint_GLint_GLsizei_GLsizei_GLenum_GLenum_GLvoid___GENPT__ *)packed;
            unpacked->func(unpacked->args.a1, unpacked->args.a2, unpacked->args.a3, unpacked->args.a4, unpacked->args.a5, unpacked->args.a6, unpacked->args.a7);
            break;
        }
        case FORMAT_void_GLenum_GLenum_GLsizei_GLsizei: {
            PACKED_void_GLenum_GLenum_GLsizei_GLsizei *unpacked = (PACKED_void_GLenum_GLenum_GLsizei_GLsizei *)packed;
            unpacked->func(unpacked->args.a1, unpacked->args.a2, unpacked->args.a3, unpacked->args.a4);
            break;
        }
        case FORMAT_void_GLclampf_GLboolean: {
            PACKED_void_GLclampf_GLboolean *unpacked = (PACKED_void_GLclampf_GLboolean *)packed;
            unpacked->func(unpacked->args.a1, unpacked->args.a2);
            break;
        }
        case FORMAT_void_GLclampx_GLboolean: {
            PACKED_void_GLclampx_GLboolean *unpacked = (PACKED_void_GLclampx_GLboolean *)packed;
            unpacked->func(unpacked->args.a1, unpacked->args.a2);
            break;
        }
        case FORMAT_void_GLint_GLint_GLsizei_GLsizei: {
            PACKED_void_GLint_GLint_GLsizei_GLsizei *unpacked = (PACKED_void_GLint_GLint_GLsizei_GLsizei *)packed;
            unpacked->func(unpacked->args.a1, unpacked->args.a2, unpacked->args.a3, unpacked->args.a4);
            break;
        }
        case FORMAT_void_GLsizei_const_GLuint___GENPT___GLenum_const_GLvoid___GENPT___GLsizei: {
            PACKED_void_GLsizei_const_GLuint___GENPT___GLenum_const_GLvoid___GENPT___GLsizei *unpacked = (PACKED_void_GLsizei_const_GLuint___GENPT___GLenum_const_GLvoid___GENPT___GLsizei *)packed;
            unpacked->func(unpacked->args.a1, unpacked->args.a2, unpacked->args.a3, unpacked->args.a4, unpacked->args.a5);
            break;
        }
        case FORMAT_void_GLuint_GLsizei_const_GLchar___GENPT___const___GENPT___const_GLint___GENPT__: {
            PACKED_void_GLuint_GLsizei_const_GLchar___GENPT___const___GENPT___const_GLint___GENPT__ *unpacked = (PACKED_void_GLuint_GLsizei_const_GLchar___GENPT___const___GENPT___const_GLint___GENPT__ *)packed;
            unpacked->func(unpacked->args.a1, unpacked->args.a2, unpacked->args.a3, unpacked->args.a4);
            break;
        }
        case FORMAT_void_GLenum_GLint_GLuint: {
            PACKED_void_GLenum_GLint_GLuint *unpacked = (PACKED_void_GLenum_GLint_GLuint *)packed;
            unpacked->func(unpacked->args.a1, unpacked->args.a2, unpacked->args.a3);
            break;
        }
        case FORMAT_void_GLenum_GLenum_GLint_GLuint: {
            PACKED_void_GLenum_GLenum_GLint_GLuint *unpacked = (PACKED_void_GLenum_GLenum_GLint_GLuint *)packed;
            unpacked->func(unpacked->args.a1, unpacked->args.a2, unpacked->args.a3, unpacked->args.a4);
            break;
        }
        case FORMAT_void_GLenum_GLenum_GLenum: {
            PACKED_void_GLenum_GLenum_GLenum *unpacked = (PACKED_void_GLenum_GLenum_GLenum *)packed;
            unpacked->func(unpacked->args.a1, unpacked->args.a2, unpacked->args.a3);
            break;
        }
        case FORMAT_void_GLenum_GLenum_GLint: {
            PACKED_void_GLenum_GLenum_GLint *unpacked = (PACKED_void_GLenum_GLenum_GLint *)packed;
            unpacked->func(unpacked->args.a1, unpacked->args.a2, unpacked->args.a3);
            break;
        }
        case FORMAT_void_GLenum_GLenum_const_GLint___GENPT__: {
            PACKED_void_GLenum_GLenum_const_GLint___GENPT__ *unpacked = (PACKED_void_GLenum_GLenum_const_GLint___GENPT__ *)packed;
            unpacked->func(unpacked->args.a1, unpacked->args.a2, unpacked->args.a3);
            break;
        }
        case FORMAT_void_GLenum_GLint_GLint_GLsizei_GLsizei_GLint_GLenum_GLenum_const_GLvoid___GENPT__: {
            PACKED_void_GLenum_GLint_GLint_GLsizei_GLsizei_GLint_GLenum_GLenum_const_GLvoid___GENPT__ *unpacked = (PACKED_void_GLenum_GLint_GLint_GLsizei_GLsizei_GLint_GLenum_GLenum_const_GLvoid___GENPT__ *)packed;
            unpacked->func(unpacked->args.a1, unpacked->args.a2, unpacked->args.a3, unpacked->args.a4, unpacked->args.a5, unpacked->args.a6, unpacked->args.a7, unpacked->args.a8, unpacked->args.a9);
            break;
        }
        case FORMAT_void_GLenum_GLint_GLint_GLint_GLsizei_GLsizei_GLenum_GLenum_const_GLvoid___GENPT__: {
            PACKED_void_GLenum_GLint_GLint_GLint_GLsizei_GLsizei_GLenum_GLenum_const_GLvoid___GENPT__ *unpacked = (PACKED_void_GLenum_GLint_GLint_GLint_GLsizei_GLsizei_GLenum_GLenum_const_GLvoid___GENPT__ *)packed;
            unpacked->func(unpacked->args.a1, unpacked->args.a2, unpacked->args.a3, unpacked->args.a4, unpacked->args.a5, unpacked->args.a6, unpacked->args.a7, unpacked->args.a8, unpacked->args.a9);
            break;
        }
        case FORMAT_void_GLint_GLfloat: {
            PACKED_void_GLint_GLfloat *unpacked = (PACKED_void_GLint_GLfloat *)packed;
            unpacked->func(unpacked->args.a1, unpacked->args.a2);
            break;
        }
        case FORMAT_void_GLint_GLsizei_const_GLfloat___GENPT__: {
            PACKED_void_GLint_GLsizei_const_GLfloat___GENPT__ *unpacked = (PACKED_void_GLint_GLsizei_const_GLfloat___GENPT__ *)packed;
            unpacked->func(unpacked->args.a1, unpacked->args.a2, unpacked->args.a3);
            break;
        }
        case FORMAT_void_GLint_GLint: {
            PACKED_void_GLint_GLint *unpacked = (PACKED_void_GLint_GLint *)packed;
            unpacked->func(unpacked->args.a1, unpacked->args.a2);
            break;
        }
        case FORMAT_void_GLint_GLsizei_const_GLint___GENPT__: {
            PACKED_void_GLint_GLsizei_const_GLint___GENPT__ *unpacked = (PACKED_void_GLint_GLsizei_const_GLint___GENPT__ *)packed;
            unpacked->func(unpacked->args.a1, unpacked->args.a2, unpacked->args.a3);
            break;
        }
        case FORMAT_void_GLint_GLfloat_GLfloat: {
            PACKED_void_GLint_GLfloat_GLfloat *unpacked = (PACKED_void_GLint_GLfloat_GLfloat *)packed;
            unpacked->func(unpacked->args.a1, unpacked->args.a2, unpacked->args.a3);
            break;
        }
        case FORMAT_void_GLint_GLint_GLint: {
            PACKED_void_GLint_GLint_GLint *unpacked = (PACKED_void_GLint_GLint_GLint *)packed;
            unpacked->func(unpacked->args.a1, unpacked->args.a2, unpacked->args.a3);
            break;
        }
        case FORMAT_void_GLint_GLfloat_GLfloat_GLfloat: {
            PACKED_void_GLint_GLfloat_GLfloat_GLfloat *unpacked = (PACKED_void_GLint_GLfloat_GLfloat_GLfloat *)packed;
            unpacked->func(unpacked->args.a1, unpacked->args.a2, unpacked->args.a3, unpacked->args.a4);
            break;
        }
        case FORMAT_void_GLint_GLint_GLint_GLint: {
            PACKED_void_GLint_GLint_GLint_GLint *unpacked = (PACKED_void_GLint_GLint_GLint_GLint *)packed;
            unpacked->func(unpacked->args.a1, unpacked->args.a2, unpacked->args.a3, unpacked->args.a4);
            break;
        }
        case FORMAT_void_GLint_GLfloat_GLfloat_GLfloat_GLfloat: {
            PACKED_void_GLint_GLfloat_GLfloat_GLfloat_GLfloat *unpacked = (PACKED_void_GLint_GLfloat_GLfloat_GLfloat_GLfloat *)packed;
            unpacked->func(unpacked->args.a1, unpacked->args.a2, unpacked->args.a3, unpacked->args.a4, unpacked->args.a5);
            break;
        }
        case FORMAT_void_GLint_GLsizei_GLboolean_const_GLfloat___GENPT__: {
            PACKED_void_GLint_GLsizei_GLboolean_const_GLfloat___GENPT__ *unpacked = (PACKED_void_GLint_GLsizei_GLboolean_const_GLfloat___GENPT__ *)packed;
            unpacked->func(unpacked->args.a1, unpacked->args.a2, unpacked->args.a3, unpacked->args.a4);
            break;
        }
        case FORMAT_void_GLuint_GLfloat: {
            PACKED_void_GLuint_GLfloat *unpacked = (PACKED_void_GLuint_GLfloat *)packed;
            unpacked->func(unpacked->args.a1, unpacked->args.a2);
            break;
        }
        case FORMAT_void_GLuint_const_GLfloat___GENPT__: {
            PACKED_void_GLuint_const_GLfloat___GENPT__ *unpacked = (PACKED_void_GLuint_const_GLfloat___GENPT__ *)packed;
            unpacked->func(unpacked->args.a1, unpacked->args.a2);
            break;
        }
        case FORMAT_void_GLuint_GLfloat_GLfloat: {
            PACKED_void_GLuint_GLfloat_GLfloat *unpacked = (PACKED_void_GLuint_GLfloat_GLfloat *)packed;
            unpacked->func(unpacked->args.a1, unpacked->args.a2, unpacked->args.a3);
            break;
        }
        case FORMAT_void_GLuint_GLfloat_GLfloat_GLfloat: {
            PACKED_void_GLuint_GLfloat_GLfloat_GLfloat *unpacked = (PACKED_void_GLuint_GLfloat_GLfloat_GLfloat *)packed;
            unpacked->func(unpacked->args.a1, unpacked->args.a2, unpacked->args.a3, unpacked->args.a4);
            break;
        }
        case FORMAT_void_GLuint_GLfloat_GLfloat_GLfloat_GLfloat: {
            PACKED_void_GLuint_GLfloat_GLfloat_GLfloat_GLfloat *unpacked = (PACKED_void_GLuint_GLfloat_GLfloat_GLfloat_GLfloat *)packed;
            unpacked->func(unpacked->args.a1, unpacked->args.a2, unpacked->args.a3, unpacked->args.a4, unpacked->args.a5);
            break;
        }
        case FORMAT_void_GLuint_GLint_GLenum_GLboolean_GLsizei_const_GLvoid___GENPT__: {
            PACKED_void_GLuint_GLint_GLenum_GLboolean_GLsizei_const_GLvoid___GENPT__ *unpacked = (PACKED_void_GLuint_GLint_GLenum_GLboolean_GLsizei_const_GLvoid___GENPT__ *)packed;
            unpacked->func(unpacked->args.a1, unpacked->args.a2, unpacked->args.a3, unpacked->args.a4, unpacked->args.a5, unpacked->args.a6);
            break;
        }
    }
}