/*
 * Refactored vertexattrib.c for GL4ES
 * Optimized for ARMv8
 * - Added GL_VERTEX_ATTRIB_ARRAY_INTEGER definition fallback
 * - Redundant state filtering
 * - Branch prediction optimization
 */

#include "vertexattrib.h"
#include "../glx/hardext.h"
#include "buffers.h"
#include "enum_info.h"
#include "gl4es.h"
#include "glstate.h"

#ifndef likely
#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)
#endif

// Fallback definition if missing in GLES headers
#ifndef GL_VERTEX_ATTRIB_ARRAY_INTEGER
#define GL_VERTEX_ATTRIB_ARRAY_INTEGER 0x88FD
#endif

//#define DEBUG
#ifdef DEBUG
#define DBG(a) a
#else
#define DBG(a)
#endif

void APIENTRY_GL4ES gl4es_glVertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid * pointer) {
    DBG(printf("glVertexAttribPointer(%d, %d, %s, %d, %d, %p)\n", index, size, PrintEnum(type), normalized, stride, pointer);)
    
    // Sanity tests
    if (unlikely(index >= hardext.maxvattrib)) {
        errorShim(GL_INVALID_VALUE);
        return;
    }
    if (unlikely(size < 1 || (size > 4 && size != GL_BGRA))) {
        errorShim(GL_INVALID_VALUE);
        return;
    }

    FLUSH_BEGINEND;
    vertexattrib_t *v = &glstate->vao->vertexattrib[index];
    
    // Determine effective stride
    GLsizei effective_stride = (stride == 0) ? ((size == GL_BGRA ? 4 : size) * gl_sizeof(type)) : stride;

    // Optimization: Redundant state check
    if (v->size == size && v->type == type && v->normalized == normalized && 
        v->stride == effective_stride && v->pointer == pointer && 
        v->buffer == glstate->vao->vertex && v->integer == 0) {
        noerrorShim();
        return;
    }

    v->size = size;
    v->type = type;
    v->normalized = normalized;
    v->integer = 0;
    v->stride = effective_stride;
    v->pointer = pointer;
    v->buffer = glstate->vao->vertex;
    
    if (v->buffer) {
        v->real_buffer = v->buffer->real_buffer;
        v->real_pointer = pointer;
    } else {
        v->real_buffer = 0;
        v->real_pointer = 0;
    }
    noerrorShim();
}

void APIENTRY_GL4ES gl4es_glVertexAttribIPointer(GLuint index, GLint size, GLenum type, GLsizei stride, const GLvoid * pointer) {
    DBG(printf("glVertexAttribIPointer(%d, %d, %s, %d, %p)\n", index, size, PrintEnum(type), stride, pointer);)
    
    if (unlikely(index >= hardext.maxvattrib)) {
        errorShim(GL_INVALID_VALUE);
        return;
    }
    if (unlikely(size < 1 || (size > 4 && size != GL_BGRA))) {
        errorShim(GL_INVALID_VALUE);
        return;
    }

    FLUSH_BEGINEND;
    vertexattrib_t *v = &glstate->vao->vertexattrib[index];
    
    GLsizei effective_stride = (stride == 0) ? ((size == GL_BGRA ? 4 : size) * gl_sizeof(type)) : stride;

    if (v->size == size && v->type == type && v->normalized == 0 && 
        v->stride == effective_stride && v->pointer == pointer && 
        v->buffer == glstate->vao->vertex && v->integer == 1) {
        noerrorShim();
        return;
    }

    v->size = size;
    v->type = type;
    v->normalized = 0;
    v->integer = 1;
    v->stride = effective_stride;
    v->pointer = pointer;
    v->buffer = glstate->vao->vertex;
    
    if (v->buffer) {
        v->real_buffer = v->buffer->real_buffer;
        v->real_pointer = pointer;
    } else {
        v->real_buffer = 0;
        v->real_pointer = 0;
    }
    noerrorShim();
}

void APIENTRY_GL4ES gl4es_glEnableVertexAttribArray(GLuint index) {
    DBG(printf("glEnableVertexAttrib(%d)\n", index);)
    if (unlikely(index >= hardext.maxvattrib)) {
        errorShim(GL_INVALID_VALUE);
        return;
    }
    if (glstate->vao->vertexattrib[index].enabled == 1) return;
    
    FLUSH_BEGINEND;
    glstate->vao->vertexattrib[index].enabled = 1;
}

void APIENTRY_GL4ES gl4es_glDisableVertexAttribArray(GLuint index) {
    DBG(printf("glDisableVertexAttrib(%d)\n", index);)
    if (unlikely(index >= hardext.maxvattrib)) {
        errorShim(GL_INVALID_VALUE);
        return;
    }
    if (glstate->vao->vertexattrib[index].enabled == 0) return;

    FLUSH_BEGINEND;
    glstate->vao->vertexattrib[index].enabled = 0;
}

void APIENTRY_GL4ES gl4es_glVertexAttrib4f(GLuint index, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3) {
    if (unlikely(index >= hardext.maxvattrib)) {
        errorShim(GL_INVALID_VALUE);
        return;
    }
    
    GLfloat f[4] = {v0, v1, v2, v3};
    if (memcmp(glstate->vavalue[index], f, 4 * sizeof(GLfloat)) == 0) {
        noerrorShim();
        return;
    }

    FLUSH_BEGINEND;
    memcpy(glstate->vavalue[index], f, 4 * sizeof(GLfloat));
}

void APIENTRY_GL4ES gl4es_glVertexAttrib4fv(GLuint index, const GLfloat *v) {
    if (unlikely(index >= hardext.maxvattrib)) {
        errorShim(GL_INVALID_VALUE);
        return;
    }
    if (memcmp(glstate->vavalue[index], v, 4 * sizeof(GLfloat)) == 0) {
        noerrorShim();
        return;
    }

    FLUSH_BEGINEND;
    memcpy(glstate->vavalue[index], v, 4 * sizeof(GLfloat));
}

#define GetVertexAttrib(suffix, Type, factor) \
void APIENTRY_GL4ES gl4es_glGetVertexAttrib##suffix##v(GLuint index, GLenum pname, Type *params) { \
    if (unlikely(index >= hardext.maxvattrib)) { \
        errorShim(GL_INVALID_VALUE); \
        return; \
    } \
    noerrorShim(); \
    switch(pname) { \
        case GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING: \
            *params = (Type)((glstate->vao->vertexattrib[index].buffer) ? glstate->vao->vertexattrib[index].buffer->buffer : 0); \
            return; \
        case GL_VERTEX_ATTRIB_ARRAY_ENABLED: \
            *params = (Type)((glstate->vao->vertexattrib[index].enabled) ? 1 : 0); \
            return; \
        case GL_VERTEX_ATTRIB_ARRAY_SIZE: \
            *params = (Type)glstate->vao->vertexattrib[index].size; \
            return; \
        case GL_VERTEX_ATTRIB_ARRAY_STRIDE: \
            *params = (Type)glstate->vao->vertexattrib[index].stride; \
            return; \
        case GL_VERTEX_ATTRIB_ARRAY_TYPE: \
            *params = (Type)glstate->vao->vertexattrib[index].type; \
            return; \
        case GL_VERTEX_ATTRIB_ARRAY_NORMALIZED: \
            *params = (Type)glstate->vao->vertexattrib[index].normalized; \
            return; \
        case GL_CURRENT_VERTEX_ATTRIB: \
            if (glstate->vao->vertexattrib[index].normalized) \
                for (int i = 0; i < 4; i++) *params++ = (Type)(glstate->vavalue[index][i] * factor); \
            else \
                for (int i = 0; i < 4; i++) *params++ = (Type)glstate->vavalue[index][i]; \
            return; \
        case GL_VERTEX_ATTRIB_ARRAY_DIVISOR: \
            *params = (Type)glstate->vao->vertexattrib[index].divisor; \
            return; \
        case GL_VERTEX_ATTRIB_ARRAY_INTEGER: \
            *params = (Type)glstate->vao->vertexattrib[index].integer; \
            return; \
    } \
    errorShim(GL_INVALID_ENUM); \
}

GetVertexAttrib(d, GLdouble, 1.0);
GetVertexAttrib(f, GLfloat, 1.0f);
GetVertexAttrib(i, GLint, 2147483647.0f);
#undef GetVertexAttrib

void APIENTRY_GL4ES gl4es_glGetVertexAttribPointerv(GLuint index, GLenum pname, GLvoid **pointer) {
    if (unlikely(index >= hardext.maxvattrib)) {
        errorShim(GL_INVALID_VALUE);
        return;
    }
    if (unlikely(pname != GL_VERTEX_ATTRIB_ARRAY_POINTER)) {
        errorShim(GL_INVALID_ENUM);
        return;
    }
    *pointer = (GLvoid*)glstate->vao->vertexattrib[index].pointer;
    noerrorShim();
}

void APIENTRY_GL4ES gl4es_glVertexAttribDivisor(GLuint index, GLuint divisor) {
    if (unlikely(index >= hardext.maxvattrib)) {
        errorShim(GL_INVALID_VALUE);
        return;
    }
    if (glstate->vao->vertexattrib[index].divisor == divisor) return;

    FLUSH_BEGINEND;
    glstate->vao->vertexattrib[index].divisor = divisor;
}

// Exports
AliasExport(void,glVertexAttribPointer,,(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid * pointer));
AliasExport(void,glVertexAttribIPointer,,(GLuint index, GLint size, GLenum type, GLsizei stride, const GLvoid * pointer));
AliasExport(void,glEnableVertexAttribArray,,(GLuint index));
AliasExport(void,glDisableVertexAttribArray,,(GLuint index));
AliasExport(void,glVertexAttrib4f,,(GLuint index, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3));
AliasExport(void,glVertexAttrib4fv,,(GLuint index, const GLfloat *v));
AliasExport(void,glGetVertexAttribdv,,(GLuint index, GLenum pname, GLdouble *params));
AliasExport(void,glGetVertexAttribfv,,(GLuint index, GLenum pname, GLfloat *params));
AliasExport(void,glGetVertexAttribiv,,(GLuint index, GLenum pname, GLint *params));
AliasExport(void,glGetVertexAttribPointerv,,(GLuint index, GLenum pname, GLvoid **pointer));

// ARB Wrappers
AliasExport(GLvoid,glVertexAttrib4f,ARB,(GLuint index, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3));
AliasExport(GLvoid,glVertexAttrib4fv,ARB,(GLuint index, const GLfloat *v));
AliasExport(GLvoid,glVertexAttribPointer,ARB,(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid *pointer));
AliasExport(GLvoid,glEnableVertexAttribArray,ARB,(GLuint index));
AliasExport(GLvoid,glDisableVertexAttribArray,ARB,(GLuint index));
AliasExport(GLvoid,glGetVertexAttribdv,ARB,(GLuint index, GLenum pname, GLdouble *params));
AliasExport(GLvoid,glGetVertexAttribfv,ARB,(GLuint index, GLenum pname, GLfloat *params));
AliasExport(GLvoid,glGetVertexAttribiv,ARB,(GLuint index, GLenum pname, GLint *params));
AliasExport(GLvoid,glGetVertexAttribPointerv,ARB,(GLuint index, GLenum pname, GLvoid **pointer));

// Instanced Arrays
AliasExport(void,glVertexAttribDivisor,,(GLuint index, GLuint divisor));
AliasExport(void,glVertexAttribDivisor,ARB,(GLuint index, GLuint divisor));