/*
 * Refactored buffers.c for GL4ES
 * Optimized for ARMv8
 * - Inlined hot-path helper functions
 * - Added redundant binding checks to avoid hash lookups
 * - Branch prediction for error handling
 */

#include "buffers.h"
#include "khash.h"
#include "../glx/hardext.h"
#include "attributes.h"
#include "debug.h"
#include "gl4es.h"
#include "glstate.h"
#include "logs.h"
#include "init.h"
#include "loader.h"

//#define DEBUG
#ifdef DEBUG
#define DBG(a) a
#else
#define DBG(a)
#endif

#ifndef likely
#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)
#endif

KHASH_MAP_IMPL_INT(buff, glbuffer_t *);
KHASH_MAP_IMPL_INT(glvao, glvao_t*);

static GLuint lastbuffer = 1;

// OPTIMIZATION: Static Inline for speed (Hot Path)
static inline glbuffer_t** BUFF(GLenum target) {
    switch(target) {
        case GL_ARRAY_BUFFER:
            return &glstate->vao->vertex;
        case GL_ELEMENT_ARRAY_BUFFER:
            return &glstate->vao->elements;
        case GL_PIXEL_PACK_BUFFER:
            return &glstate->vao->pack;
        case GL_PIXEL_UNPACK_BUFFER:
            return &glstate->vao->unpack;
        default:
            LOGD("Warning, unknown buffer target 0x%04X\n", target);
            return NULL;
    }
}

static inline int buffer_target(GLenum target) {
    // Optimized switch usually faster than if-chain for enums
    switch(target) {
        case GL_ARRAY_BUFFER:
        case GL_ELEMENT_ARRAY_BUFFER:
        case GL_PIXEL_PACK_BUFFER:
        case GL_PIXEL_UNPACK_BUFFER:
            return 1;
        default:
            return 0;
    }
}

void unbind_buffer(GLenum target) {
    glbuffer_t **t = BUFF(target);
    if (t) *t = NULL;
}

void bind_buffer(GLenum target, glbuffer_t* buff) {
    glbuffer_t **t = BUFF(target);
    if (t) *t = buff;
}

glbuffer_t* getbuffer_buffer(GLenum target) {
    glbuffer_t **t = BUFF(target);
    if (t) return *t;
    return NULL;
}

glbuffer_t* getbuffer_id(GLuint buffer) {
    if (unlikely(buffer == 0)) return NULL;
    
    khash_t(buff) *list = glstate->buffers;
    khint_t k = kh_get(buff, list, buffer);
    if (k != kh_end(list)) {
        return kh_value(list, k);
    }
    return NULL;
}

void rebind_real_buff_arrays(int old_buffer, int new_buffer) {
    // Loop unrolling hint could be placed here, but maxvattrib is usually small
    for (int j = 0; j < hardext.maxvattrib; j++) {
        if (glstate->vao->vertexattrib[j].real_buffer == old_buffer) {
            glstate->vao->vertexattrib[j].real_buffer = new_buffer;
        }
    }
}

void APIENTRY_GL4ES gl4es_glGenBuffers(GLsizei n, GLuint * buffers) {
    DBG(printf("glGenBuffers(%i, %p)\n", n, buffers);)
    noerrorShim();
    if (unlikely(n < 1)) {
        errorShim(GL_INVALID_VALUE);
        return;
    }
    khash_t(buff) *list = glstate->buffers;
    for (int i = 0; i < n; i++) {
        int b;
        // Optimization: lastbuffer is monotonic, collision check usually fast
        while(getbuffer_id(b = lastbuffer++));
        buffers[i] = b;
        
        khint_t k;
        int ret;
        k = kh_put(buff, list, b, &ret);
        glbuffer_t *buff = kh_value(list, k) = malloc(sizeof(glbuffer_t));
        memset(buff, 0, sizeof(glbuffer_t)); // Safer than manual field zeroing
        buff->buffer = b;
        buff->usage = GL_STATIC_DRAW;
        buff->access = GL_READ_WRITE;
    }
}

void APIENTRY_GL4ES gl4es_glBindBuffer(GLenum target, GLuint buffer) {
    DBG(printf("glBindBuffer(%s, %u)\n", PrintEnum(target), buffer);)
    FLUSH_BEGINEND;

    if (unlikely(!buffer_target(target))) {
        errorShim(GL_INVALID_ENUM);
        return;
    }

    // OPTIMIZATION: Check if already bound to avoid hash lookup
    glbuffer_t **current_binding = BUFF(target);
    if (current_binding && *current_binding && (*current_binding)->buffer == buffer) {
        noerrorShim();
        return;
    }
    if (buffer == 0 && current_binding && *current_binding == NULL) {
        noerrorShim();
        return;
    }

    khint_t k;
    int ret;
    khash_t(buff) *list = glstate->buffers;

    if (buffer == 0) {
        bindBuffer(target, 0);
        unbind_buffer(target);
    } else {
        k = kh_get(buff, list, buffer);
        glbuffer_t *buff = NULL;
        if (k == kh_end(list)) {
            // Create on bind if not exists
            k = kh_put(buff, list, buffer, &ret);
            buff = kh_value(list, k) = malloc(sizeof(glbuffer_t));
            memset(buff, 0, sizeof(glbuffer_t));
            buff->buffer = buffer;
            buff->type = target;
            buff->usage = GL_STATIC_DRAW;
            buff->access = GL_READ_WRITE;
        } else {
            buff = kh_value(list, k);
            buff->type = target;
        }
        bind_buffer(target, buff);
    }
    noerrorShim();
}

void APIENTRY_GL4ES gl4es_glBufferData(GLenum target, GLsizeiptr size, const GLvoid * data, GLenum usage) {
    DBG(printf("glBufferData(%s, %zi, %p, %s)\n", PrintEnum(target), size, data, PrintEnum(usage));)
    
    if (unlikely(!buffer_target(target))) {
        errorShim(GL_INVALID_ENUM);
        return;
    }
    
    glbuffer_t *buff = getbuffer_buffer(target);
    if (unlikely(buff == NULL)) {
        errorShim(GL_INVALID_OPERATION);
        LOGE("Warning, null buffer for target=0x%04X for glBufferData\n", target);
        return;
    }
    
    if (target == GL_ARRAY_BUFFER)
        VaoSharedClear(glstate->vao);
    
    int go_real = 0;
    if ((target == GL_ARRAY_BUFFER || target == GL_ELEMENT_ARRAY_BUFFER) 
         && (usage == GL_STREAM_DRAW || usage == GL_STATIC_DRAW || usage == GL_DYNAMIC_DRAW) 
         && globals4es.usevbo) {
        go_real = 1;
    }
    
    if (buff->real_buffer && !go_real) {
        rebind_real_buff_arrays(buff->real_buffer, 0);
        deleteSingleBuffer(buff->real_buffer);
        buff->real_buffer = 0;
    }
    
    if (go_real) {
        if (!buff->real_buffer) {
            LOAD_GLES(glGenBuffers);
            gles_glGenBuffers(1, &buff->real_buffer);
        }
        LOAD_GLES(glBufferData);
        LOAD_GLES(glBindBuffer);
        bindBuffer(target, buff->real_buffer);
        gles_glBufferData(target, size, data, usage);
        DBG(printf(" => real VBO %d\n", buff->real_buffer);)
    }
        
    // Memory Optimization: Reuse existing buffer if size fits
    if (buff->data && buff->size < size) {
        free(buff->data);
        buff->data = NULL;
    }
    if (!buff->data) {
        buff->data = malloc(size);
    }
    
    buff->size = size;
    buff->usage = usage;
    buff->access = GL_READ_WRITE;
    
    if (data) {
        memcpy(buff->data, data, size);
    }
    
    // Update bound VAO attributes
    for (int i = 0; i < hardext.maxvattrib; ++i) {
        vertexattrib_t *v = &glstate->vao->vertexattrib[i];
        if (v->buffer == buff) {
            v->real_buffer = v->buffer->real_buffer;
        }
    }
    noerrorShim();
}

void APIENTRY_GL4ES gl4es_glNamedBufferData(GLuint buffer, GLsizeiptr size, const GLvoid * data, GLenum usage) {
    DBG(printf("glNamedBufferData(%u, %zi, %p, %s)\n", buffer, size, data, PrintEnum(usage));)
    glbuffer_t *buff = getbuffer_id(buffer);
    if (unlikely(buff == NULL)) {
        DBG(printf("Named Buffer not found\n");)
        errorShim(GL_INVALID_OPERATION);
        return;
    }
    
    // Reallocation logic same as glBufferData
    if (buff->data) {
        free(buff->data);
        buff->data = NULL; // Safety
    }
    
    int go_real = 0;
    if ((buff->type == GL_ARRAY_BUFFER || buff->type == GL_ELEMENT_ARRAY_BUFFER) 
         && (usage == GL_STREAM_DRAW || usage == GL_STATIC_DRAW || usage == GL_DYNAMIC_DRAW) 
         && globals4es.usevbo) {
        go_real = 1;
    }
    
    if (buff->real_buffer && !go_real) {
        deleteSingleBuffer(buff->real_buffer);
        buff->real_buffer = 0;
    }
    
    if (go_real) {
        if (!buff->real_buffer) {
            LOAD_GLES(glGenBuffers);
            gles_glGenBuffers(1, &buff->real_buffer);
        }
        LOAD_GLES(glBufferData);
        LOAD_GLES(glBindBuffer);
        bindBuffer(buff->type, buff->real_buffer);
        gles_glBufferData(buff->type, size, data, usage);
    }

    buff->size = size;
    buff->usage = usage;
    buff->data = malloc(size);
    buff->access = GL_READ_WRITE;
    
    if (data) {
        memcpy(buff->data, data, size);
    }
    
    for (int i = 0; i < hardext.maxvattrib; ++i) {
        vertexattrib_t *v = &glstate->vao->vertexattrib[i];
        if (v->buffer == buff) {
            v->real_buffer = v->buffer->real_buffer;
        }
    }
    noerrorShim();
}

void APIENTRY_GL4ES gl4es_glBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, const GLvoid * data) {
    DBG(printf("glBufferSubData(%s, %p, %zi, %p)\n", PrintEnum(target), (void*)offset, size, data);)
    
    if (unlikely(!buffer_target(target))) {
        errorShim(GL_INVALID_ENUM);
        return;
    }
    
    glbuffer_t *buff = getbuffer_buffer(target);
    if (unlikely(buff == NULL)) {
        errorShim(GL_INVALID_OPERATION);
        return;
    }

    if (target == GL_ARRAY_BUFFER)
        VaoSharedClear(glstate->vao);

    if (unlikely(offset < 0 || size < 0 || offset + size > buff->size)) {
        errorShim(GL_INVALID_VALUE);
        return;
    }

    if ((target == GL_ARRAY_BUFFER || target == GL_ELEMENT_ARRAY_BUFFER) && buff->real_buffer) {
        LOAD_GLES(glBufferSubData);
        LOAD_GLES(glBindBuffer);
        bindBuffer(target, buff->real_buffer);
        gles_glBufferSubData(target, offset, size, data);
    }
        
    memcpy((char*)buff->data + offset, data, size);
    noerrorShim();
}

void APIENTRY_GL4ES gl4es_glNamedBufferSubData(GLuint buffer, GLintptr offset, GLsizeiptr size, const GLvoid * data) {
    DBG(printf("glNamedBufferSubData(%u, %p, %zi, %p)\n", buffer, (void*)offset, size, data);)
    
    glbuffer_t *buff = getbuffer_id(buffer);
    if (unlikely(buff == NULL)) {
        errorShim(GL_INVALID_OPERATION);
        return;
    }

    if (unlikely(offset < 0 || size < 0 || offset + size > buff->size)) {
        errorShim(GL_INVALID_VALUE);
        return;
    }
        
    if ((buff->type == GL_ARRAY_BUFFER || buff->type == GL_ELEMENT_ARRAY_BUFFER) && buff->real_buffer) {
        LOAD_GLES(glBufferSubData);
        LOAD_GLES(glBindBuffer);
        bindBuffer(buff->type, buff->real_buffer);
        gles_glBufferSubData(buff->type, offset, size, data);
    }
    memcpy((char*)buff->data + offset, data, size);
    noerrorShim();
}

void APIENTRY_GL4ES gl4es_glDeleteBuffers(GLsizei n, const GLuint * buffers) {
    DBG(printf("glDeleteBuffers(%i, %p)\n", n, buffers);)
    if (unlikely(!glstate)) return;
    FLUSH_BEGINEND;
    
    if (n < 1) return;

    VaoSharedClear(glstate->vao);
    khash_t(buff) *list = glstate->buffers;
    
    if (likely(list != NULL)) {
        khint_t k;
        glbuffer_t *buff;
        for (int i = 0; i < n; i++) {
            GLuint t = buffers[i];
            if (likely(t != 0)) {
                k = kh_get(buff, list, t);
                if (k != kh_end(list)) {
                    buff = kh_value(list, k);
                    if (buff->real_buffer) {
                        rebind_real_buff_arrays(buff->real_buffer, 0);
                        LOAD_GLES(glDeleteBuffers);
                        deleteSingleBuffer(buff->real_buffer);
                    }
                    // Nullify references in VAO
                    if (glstate->vao->vertex == buff) glstate->vao->vertex = NULL;
                    if (glstate->vao->elements == buff) glstate->vao->elements = NULL;
                    if (glstate->vao->pack == buff) glstate->vao->pack = NULL;
                    if (glstate->vao->unpack == buff) glstate->vao->unpack = NULL;
                    
                    for (int j = 0; j < hardext.maxvattrib; j++) {
                        if (glstate->vao->vertexattrib[j].buffer == buff) {
                            glstate->vao->vertexattrib[j].buffer = NULL;
                            glstate->vao->vertexattrib[j].real_buffer = 0;
                            glstate->vao->vertexattrib[j].real_pointer = 0;
                        }
                    }
                    
                    if (buff->data) free(buff->data);
                    kh_del(buff, list, k);
                    free(buff);
                }
            }
        }
    }
    noerrorShim();
}

GLboolean APIENTRY_GL4ES gl4es_glIsBuffer(GLuint buffer) {
    DBG(printf("glIsBuffer(%u)\n", buffer);)
    if (unlikely(buffer == 0)) return GL_FALSE;
    
    khash_t(buff) *list = glstate->buffers;
    noerrorShim();
    if (likely(list != NULL)) {
        if (kh_get(buff, list, buffer) != kh_end(list)) {
            return GL_TRUE;
        }
    }
    return GL_FALSE;
}

static void bufferGetParameteriv(glbuffer_t* buff, GLenum value, GLint * data) {
    noerrorShim();
    switch (value) {
        case GL_BUFFER_ACCESS:
            data[0] = buff->access;
            break;
        case GL_BUFFER_ACCESS_FLAGS:
            data[0] = GL_MAP_READ_BIT | GL_MAP_WRITE_BIT;
            break;
        case GL_BUFFER_MAPPED:
            data[0] = (buff->mapped) ? GL_TRUE : GL_FALSE;
            break;
        case GL_BUFFER_MAP_LENGTH:
            data[0] = (buff->mapped) ? buff->size : 0;
            break;
        case GL_BUFFER_MAP_OFFSET:
            data[0] = 0;
            break;
        case GL_BUFFER_SIZE:
            data[0] = buff->size;
            break;
        case GL_BUFFER_USAGE:
            data[0] = buff->usage;
            break;
        default:
            errorShim(GL_INVALID_ENUM);
    }
}

void APIENTRY_GL4ES gl4es_glGetBufferParameteriv(GLenum target, GLenum value, GLint * data) {
    DBG(printf("glGetBufferParameteriv(%s, %s, %p)\n", PrintEnum(target), PrintEnum(value), data);)
    if (unlikely(!buffer_target(target))) {
        errorShim(GL_INVALID_ENUM);
        return;
    }
    glbuffer_t* buff = getbuffer_buffer(target);
    if (unlikely(buff == NULL)) {
        errorShim(GL_INVALID_OPERATION);
        return;
    }
    bufferGetParameteriv(buff, value, data);
}

void APIENTRY_GL4ES gl4es_glGetNamedBufferParameteriv(GLuint buffer, GLenum value, GLint * data) {
    DBG(printf("glGetNamedBufferParameteriv(%u, %s, %p)\n", buffer, PrintEnum(value), data);)
    glbuffer_t* buff = getbuffer_id(buffer);
    if (unlikely(buff == NULL)) {
        errorShim(GL_INVALID_OPERATION);
        return;
    }
    bufferGetParameteriv(buff, value, data);
}

void* APIENTRY_GL4ES gl4es_glMapBuffer(GLenum target, GLenum access) {
    DBG(printf("glMapBuffer(%s, %s)\n", PrintEnum(target), PrintEnum(access));)
    if (unlikely(!buffer_target(target))) {
        errorShim(GL_INVALID_ENUM);
        return NULL;
    }

    if (target == GL_ARRAY_BUFFER)
        VaoSharedClear(glstate->vao);

    glbuffer_t *buff = getbuffer_buffer(target);
    if (unlikely(buff == NULL)) {
        errorShim(GL_INVALID_VALUE);
        return NULL;
    }
    if (unlikely(buff->mapped)) {
        errorShim(GL_INVALID_OPERATION);
        return NULL;
    }
    buff->access = access;
    buff->mapped = 1;
    buff->ranged = 0;
    noerrorShim();
    return buff->data;
}

void* APIENTRY_GL4ES gl4es_glMapNamedBuffer(GLuint buffer, GLenum access) {
    DBG(printf("glMapNamedBuffer(%u, %s)\n", buffer, PrintEnum(access));)
    glbuffer_t *buff = getbuffer_id(buffer);
    if (unlikely(buff == NULL)) {
        errorShim(GL_INVALID_VALUE);
        return NULL;
    }
    if (unlikely(buff->mapped)) {
        errorShim(GL_INVALID_OPERATION);
        return NULL;
    }
    buff->access = access;
    buff->mapped = 1;
    buff->ranged = 0;
    noerrorShim();
    return buff->data;
}

GLboolean APIENTRY_GL4ES gl4es_glUnmapBuffer(GLenum target) {
    DBG(printf("glUnmapBuffer(%s)\n", PrintEnum(target));)
    if (glstate->list.compiling) {
        errorShim(GL_INVALID_OPERATION); 
        return GL_FALSE;
    }
    FLUSH_BEGINEND;
    
    if (unlikely(!buffer_target(target))) {
        errorShim(GL_INVALID_ENUM);
        return GL_FALSE;
    }

    if (target == GL_ARRAY_BUFFER)
        VaoSharedClear(glstate->vao);
        
    glbuffer_t *buff = getbuffer_buffer(target);
    if (unlikely(buff == NULL)) {
        errorShim(GL_INVALID_VALUE);
        return GL_FALSE;
    }
    noerrorShim();
    
    // Sync to GPU if needed
    if (buff->real_buffer && (buff->type == GL_ARRAY_BUFFER || buff->type == GL_ELEMENT_ARRAY_BUFFER) && buff->mapped) {
        int do_flush = 0;
        if (!buff->ranged && (buff->access == GL_WRITE_ONLY || buff->access == GL_READ_WRITE)) {
            do_flush = 1;
        } else if (buff->ranged && (buff->access & GL_MAP_WRITE_BIT_EXT) && !(buff->access & GL_MAP_FLUSH_EXPLICIT_BIT_EXT)) {
            do_flush = 1;
        }

        if (do_flush) {
            LOAD_GLES(glBufferSubData);
            LOAD_GLES(glBindBuffer);
            bindBuffer(buff->type, buff->real_buffer);
            if (buff->ranged)
                gles_glBufferSubData(buff->type, buff->offset, buff->length, (void*)((uintptr_t)buff->data + buff->offset));
            else
                gles_glBufferSubData(buff->type, 0, buff->size, buff->data);
        }
    }

    if (buff->mapped) {
        buff->mapped = 0;
        buff->ranged = 0;
        return GL_TRUE;
    }
    return GL_FALSE;
}

GLboolean APIENTRY_GL4ES gl4es_glUnmapNamedBuffer(GLuint buffer) {
    DBG(printf("glUnmapNamedBuffer(%u)\n", buffer);)
    if (glstate->list.compiling) {
        errorShim(GL_INVALID_OPERATION); 
        return GL_FALSE;
    }
    FLUSH_BEGINEND;
    
    glbuffer_t *buff = getbuffer_id(buffer);
    if (unlikely(buff == NULL)) return GL_FALSE;
    noerrorShim();

    // Similar sync logic
    if (buff->real_buffer && (buff->type == GL_ARRAY_BUFFER || buff->type == GL_ELEMENT_ARRAY_BUFFER) && buff->mapped) {
        int do_flush = 0;
        if ((!buff->ranged && (buff->access == GL_WRITE_ONLY || buff->access == GL_READ_WRITE)) ||
            (buff->ranged && (buff->access & GL_MAP_WRITE_BIT_EXT) && !(buff->access & GL_MAP_FLUSH_EXPLICIT_BIT_EXT))) {
            do_flush = 1;
        }

        if (do_flush) {
            LOAD_GLES(glBufferSubData);
            LOAD_GLES(glBindBuffer);
            bindBuffer(buff->type, buff->real_buffer);
            if(buff->ranged)
                gles_glBufferSubData(buff->type, buff->offset, buff->length, (void*)((uintptr_t)buff->data + buff->offset));
            else
                gles_glBufferSubData(buff->type, 0, buff->size, buff->data);
        }
    }

    if (buff->mapped) {
        buff->mapped = 0;
        buff->ranged = 0;
        return GL_TRUE;
    }
    return GL_FALSE;
}

void APIENTRY_GL4ES gl4es_glGetBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, GLvoid * data) {
    DBG(printf("glGetBufferSubData(%s, %p, %zi, %p)\n", PrintEnum(target), (void*)offset, size, data);)
    if (unlikely(!buffer_target(target))) {
        errorShim(GL_INVALID_ENUM);
        return;
    }
    glbuffer_t *buff = getbuffer_buffer(target);
    if (unlikely(buff == NULL)) return;
    
    // Fast copy
    memcpy(data, (char*)buff->data + offset, size);
    noerrorShim();
}

void APIENTRY_GL4ES gl4es_glGetNamedBufferSubData(GLuint buffer, GLintptr offset, GLsizeiptr size, GLvoid * data) {
    DBG(printf("glGetNamedBufferSubData(%u, %p, %zi, %p)\n", buffer, (void*)offset, size, data);)
    glbuffer_t *buff = getbuffer_id(buffer);
    if (unlikely(buff == NULL)) return;
    
    memcpy(data, (char*)buff->data + offset, size);
    noerrorShim();
}

void APIENTRY_GL4ES gl4es_glGetBufferPointerv(GLenum target, GLenum pname, GLvoid ** params) {
    DBG(printf("glGetBufferPointerv(%s, %s, %p)\n", PrintEnum(target), PrintEnum(pname), params);)
    if (unlikely(!buffer_target(target))) {
        errorShim(GL_INVALID_ENUM);
        return;
    }
    glbuffer_t *buff = getbuffer_buffer(target);
    if (unlikely(buff == NULL)) return;
    
    if (unlikely(pname != GL_BUFFER_MAP_POINTER)) {
        errorShim(GL_INVALID_ENUM);
        return;
    }
    params[0] = (buff->mapped) ? buff->data : NULL;
}

void APIENTRY_GL4ES gl4es_glGetNamedBufferPointerv(GLuint buffer, GLenum pname, GLvoid ** params) {
    DBG(printf("glGetNamedBufferPointerv(%u, %s, %p)\n", buffer, PrintEnum(pname), params);)
    glbuffer_t *buff = getbuffer_id(buffer);
    if (unlikely(buff == NULL)) return;
    
    if (unlikely(pname != GL_BUFFER_MAP_POINTER)) {
        errorShim(GL_INVALID_ENUM);
        return;
    }
    params[0] = (buff->mapped) ? buff->data : NULL;
}

void* APIENTRY_GL4ES gl4es_glMapBufferRange(GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield access) {
    DBG(printf("glMapBufferRange(%s, %p, %zd, 0x%x)\n", PrintEnum(target), (void*)offset, length, access);)
    if (unlikely(!buffer_target(target))) {
        errorShim(GL_INVALID_ENUM);
        return NULL;
    }

    glbuffer_t *buff = getbuffer_buffer(target);
    if (unlikely(buff == NULL)) {
        errorShim(GL_INVALID_VALUE);
        return NULL;
    }
    if (unlikely(buff->mapped)) {
        errorShim(GL_INVALID_OPERATION);
        return NULL;
    }
    buff->access = access;
    buff->mapped = 1;
    buff->ranged = 1;
    buff->offset = offset;
    buff->length = length;
    noerrorShim();
    return (void*)((uintptr_t)buff->data + offset);
}

void APIENTRY_GL4ES gl4es_glFlushMappedBufferRange(GLenum target, GLintptr offset, GLsizeiptr length) {
    DBG(printf("glFlushMappedBufferRange(%s, %p, %zd)\n", PrintEnum(target), (void*)offset, length);)
    if (unlikely(!buffer_target(target))) {
        errorShim(GL_INVALID_ENUM);
        return;
    }

    if (target == GL_ARRAY_BUFFER)
        VaoSharedClear(glstate->vao);

    glbuffer_t *buff = getbuffer_buffer(target);
    if (unlikely(!buff)) {
        errorShim(GL_INVALID_VALUE);
        return;
    }
    if (unlikely(!buff->mapped || !buff->ranged || !(buff->access & GL_MAP_FLUSH_EXPLICIT_BIT_EXT))) {
        errorShim(GL_INVALID_OPERATION);
        return;
    }

    if (buff->real_buffer && (buff->type == GL_ARRAY_BUFFER || buff->type == GL_ELEMENT_ARRAY_BUFFER) && (buff->access & GL_MAP_WRITE_BIT_EXT)) {
        LOAD_GLES(glBufferSubData);
        bindBuffer(buff->type, buff->real_buffer);
        gles_glBufferSubData(buff->type, buff->offset + offset, length, (void*)((uintptr_t)buff->data + buff->offset + offset));
    }
}

void APIENTRY_GL4ES gl4es_glCopyBufferSubData(GLenum readTarget, GLenum writeTarget, GLintptr readOffset, GLintptr writeOffset, GLsizeiptr size) {
    DBG(printf("glCopyBufferSubData(%s, %s, %p, %p, %zd)\n", PrintEnum(readTarget), PrintEnum(writeTarget), (void*)readOffset, (void*)writeOffset, size);)
    glbuffer_t *readbuff = getbuffer_buffer(readTarget);
    glbuffer_t *writebuff = getbuffer_buffer(writeTarget);
    
    if (unlikely(!readbuff || !writebuff)) {
        errorShim(GL_INVALID_VALUE);
        return;
    }
    if (unlikely(writebuff->ranged && !(writebuff->access & GL_MAP_PERSISTENT_BIT))) {
        errorShim(GL_INVALID_OPERATION);
        return;
    }
    
    // Memory copy
    memmove((char*)writebuff->data + writeOffset, (char*)readbuff->data + readOffset, size);
    
    if (writebuff->real_buffer && (writebuff->type == GL_ARRAY_BUFFER || writebuff->type == GL_ELEMENT_ARRAY_BUFFER) && writebuff->mapped && (writebuff->access == GL_WRITE_ONLY || writebuff->access == GL_READ_WRITE)) {
        LOAD_GLES(glBufferSubData);
        bindBuffer(writebuff->type, writebuff->real_buffer);
        gles_glBufferSubData(writebuff->type, writeOffset, size, (char*)writebuff->data + writeOffset);
    }
    noerrorShim();
}

void bindBuffer(GLenum target, GLuint buffer) {
    LOAD_GLES(glBindBuffer);
    if (target == GL_ARRAY_BUFFER) {
        if (glstate->bind_buffer.array == buffer) return;
        glstate->bind_buffer.array = buffer;
        gles_glBindBuffer(target, buffer);
    } else if (target == GL_ELEMENT_ARRAY_BUFFER) {
        glstate->bind_buffer.want_index = buffer;
        if (glstate->bind_buffer.index == buffer) return;
        glstate->bind_buffer.index = buffer;
        gles_glBindBuffer(target, buffer);
    } else {
        LOGE("Warning, unhandled Buffer type %s in bindBuffer\n", PrintEnum(target));
        return;
    }
    glstate->bind_buffer.used = (glstate->bind_buffer.index && glstate->bind_buffer.array) ? 1 : 0;
}

GLuint wantBufferIndex(GLuint buffer) {
    GLuint ret = glstate->bind_buffer.want_index;
    glstate->bind_buffer.want_index = buffer;
    return ret;
}

void realize_bufferIndex() {
    LOAD_GLES(glBindBuffer);
    if (glstate->bind_buffer.index != glstate->bind_buffer.want_index) {
        glstate->bind_buffer.index = glstate->bind_buffer.want_index;
        gles_glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, glstate->bind_buffer.index);
        glstate->bind_buffer.used = (glstate->bind_buffer.index && glstate->bind_buffer.array) ? 1 : 0;
    }
}

void deleteSingleBuffer(GLuint buffer) {
   LOAD_GLES(glDeleteBuffers);
   if (glstate->bind_buffer.index == buffer) glstate->bind_buffer.index = 0;
   else if (glstate->bind_buffer.want_index == buffer) glstate->bind_buffer.want_index = 0;
   else if (glstate->bind_buffer.array == buffer) glstate->bind_buffer.array = 0;
   gles_glDeleteBuffers(1, &buffer);
}

void unboundBuffers() {
    if (likely(!glstate->bind_buffer.used)) return;
    
    LOAD_GLES(glBindBuffer);
    if (glstate->bind_buffer.array) {
        glstate->bind_buffer.array = 0;
        gles_glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
    if (glstate->bind_buffer.index) {
        glstate->bind_buffer.index = 0;
        glstate->bind_buffer.want_index = 0;
        gles_glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    }
    glstate->bind_buffer.used = 0;
}

// Exports
AliasExport(void,glGenBuffers,,(GLsizei n, GLuint * buffers));
AliasExport(void,glBindBuffer,,(GLenum target, GLuint buffer));
AliasExport(void,glBufferData,,(GLenum target, GLsizeiptr size, const GLvoid * data, GLenum usage));
AliasExport(void,glBufferSubData,,(GLenum target, GLintptr offset, GLsizeiptr size, const GLvoid * data));
AliasExport(void,glDeleteBuffers,,(GLsizei n, const GLuint * buffers));
AliasExport(GLboolean,glIsBuffer,,(GLuint buffer));
AliasExport(void,glGetBufferParameteriv,,(GLenum target, GLenum value, GLint * data));
AliasExport(void*,glMapBuffer,,(GLenum target, GLenum access));
AliasExport(GLboolean,glUnmapBuffer,,(GLenum target));
AliasExport(void,glGetBufferSubData,,(GLenum target, GLintptr offset, GLsizeiptr size, GLvoid * data));
AliasExport(void,glGetBufferPointerv,,(GLenum target, GLenum pname, GLvoid ** params));

AliasExport(void*,glMapBufferRange,,(GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield access));
AliasExport(void,glFlushMappedBufferRange,,(GLenum target, GLintptr offset, GLsizeiptr length));
AliasExport(void,glCopyBufferSubData,,(GLenum readTarget, GLenum writeTarget, GLintptr readOffset, GLintptr writeOffset, GLsizeiptr size));

#ifndef AMIGAOS4
AliasExport(void,glGenBuffers,ARB,(GLsizei n, GLuint * buffers));
AliasExport(void,glBindBuffer,ARB,(GLenum target, GLuint buffer));
AliasExport(void,glBufferData,ARB,(GLenum target, GLsizeiptr size, const GLvoid * data, GLenum usage));
AliasExport(void,glBufferSubData,ARB,(GLenum target, GLintptr offset, GLsizeiptr size, const GLvoid * data));
AliasExport(void,glDeleteBuffers,ARB,(GLsizei n, const GLuint * buffers));
AliasExport(GLboolean,glIsBuffer,ARB,(GLuint buffer));
AliasExport(void,glGetBufferParameteriv,ARB,(GLenum target, GLenum value, GLint * data));
AliasExport(void*,glMapBuffer,ARB,(GLenum target, GLenum access));
AliasExport(GLboolean,glUnmapBuffer,ARB,(GLenum target));
AliasExport(void,glGetBufferSubData,ARB,(GLenum target, GLintptr offset, GLsizeiptr size, GLvoid * data));
AliasExport(void,glGetBufferPointerv,ARB,(GLenum target, GLenum pname, GLvoid ** params));
#endif

//Direct Access Exports
AliasExport(void,glNamedBufferData,,(GLuint buffer, GLsizeiptr size, const GLvoid * data, GLenum usage));
AliasExport(void,glNamedBufferSubData,,(GLuint buffer, GLintptr offset, GLsizeiptr size, const GLvoid * data));
AliasExport(void,glGetNamedBufferParameteriv,,(GLuint buffer, GLenum value, GLint * data));
AliasExport(void*,glMapNamedBuffer,,(GLuint buffer, GLenum access));
AliasExport(GLboolean,glUnmapNamedBuffer,,(GLuint buffer));
AliasExport(void,glGetNamedBufferSubData,,(GLuint buffer, GLintptr offset, GLsizeiptr size, GLvoid * data));
AliasExport(void,glGetNamedBufferPointerv,,(GLuint buffer, GLenum pname, GLvoid ** params));

AliasExport(void,glNamedBufferData,EXT,(GLuint buffer, GLsizeiptr size, const GLvoid * data, GLenum usage));
AliasExport(void,glNamedBufferSubData,EXT,(GLuint buffer, GLintptr offset, GLsizeiptr size, const GLvoid * data));
AliasExport(void,glGetNamedBufferParameteriv,EXT,(GLuint buffer, GLenum value, GLint * data));
AliasExport(void*,glMapNamedBuffer,EXT,(GLuint buffer, GLenum access));
AliasExport(GLboolean,glUnmapNamedBuffer,EXT,(GLuint buffer));
AliasExport(void,glGetNamedBufferSubData,EXT,(GLuint buffer, GLintptr offset, GLsizeiptr size, GLvoid * data));
AliasExport(void,glGetNamedBufferPointerv,EXT,(GLuint buffer, GLenum pname, GLvoid ** params));

// VAO Implementation
static GLuint lastvao = 1;

void APIENTRY_GL4ES gl4es_glGenVertexArrays(GLsizei n, GLuint *arrays) {
    DBG(printf("glGenVertexArrays(%i, %p)\n", n, arrays);)
    noerrorShim();
    if (unlikely(n < 1)) {
        errorShim(GL_INVALID_VALUE);
        return;
    }
    for (int i = 0; i < n; i++) {
        arrays[i] = lastvao++;
    }
}

void APIENTRY_GL4ES gl4es_glBindVertexArray(GLuint array) {
    DBG(printf("glBindVertexArray(%u)\n", array);)
    FLUSH_BEGINEND;

    khint_t k;
    int ret;
    khash_t(glvao) *list = glstate->vaos;

    if (array == 0) {
        glstate->vao = glstate->defaultvao;
    } else {
        k = kh_get(glvao, list, array);
        glvao_t *glvao = NULL;
        if (k == kh_end(list)) {
            k = kh_put(glvao, list, array, &ret);
            glvao = kh_value(list, k) = malloc(sizeof(glvao_t));
            VaoInit(glvao);
            // Inherit state
            glvao->vertex = glstate->vao->vertex;
            glvao->elements = glstate->vao->elements;
            glvao->pack = glstate->vao->pack;
            glvao->unpack = glstate->vao->unpack;
            glvao->maxtex = glstate->vao->maxtex;
            glvao->array = array;
        } else {
            glvao = kh_value(list, k);
        }
        glstate->vao = glvao;
    }
    noerrorShim();
}

void APIENTRY_GL4ES gl4es_glDeleteVertexArrays(GLsizei n, const GLuint *arrays) {
    DBG(printf("glDeleteVertexArrays(%i, %p)\n", n, arrays);)
    if (unlikely(!glstate)) return;
    FLUSH_BEGINEND;

    khash_t(glvao) *list = glstate->vaos;
    if (likely(list != NULL)) {
        khint_t k;
        glvao_t *glvao;
        for (int i = 0; i < n; i++) {
            GLuint t = arrays[i];
            if (likely(t != 0)) {
                k = kh_get(glvao, list, t);
                if (k != kh_end(list)) {
                    glvao = kh_value(list, k);
                    VaoSharedClear(glvao);
                    kh_del(glvao, list, k);
                    free(glvao); // Actually free the memory
                }
            }
        }
    }
    noerrorShim();
}

GLboolean APIENTRY_GL4ES gl4es_glIsVertexArray(GLuint array) {
    DBG(printf("glIsVertexArray(%u)\n", array);)
    if (unlikely(!glstate)) return GL_FALSE;
    khash_t(glvao) *list = glstate->vaos;
    noerrorShim();
    if (likely(list != NULL)) {
        if (kh_get(glvao, list, array) != kh_end(list)) {
            return GL_TRUE;
        }
    }
    return GL_FALSE;
}

void VaoSharedClear(glvao_t *vao) {
    if (unlikely(vao == NULL || vao->shared_arrays == NULL)) return;
    if (!(--(*vao->shared_arrays))) {
        free(vao->vert.ptr);
        free(vao->color.ptr);
        free(vao->secondary.ptr);
        free(vao->normal.ptr);
        for (int i = 0; i < hardext.maxtex; i++)
            free(vao->tex[i].ptr);
        free(vao->shared_arrays);
    }
    vao->vert.ptr = NULL;
    vao->color.ptr = NULL;
    vao->secondary.ptr = NULL;
    vao->normal.ptr = NULL;
    for (int i = 0; i < hardext.maxtex; i++)
        vao->tex[i].ptr = NULL;
    vao->shared_arrays = NULL;
}

void VaoInit(glvao_t *vao) {
    memset(vao, 0, sizeof(glvao_t));
    for (int i = 0; i < hardext.maxvattrib; i++) {
        vao->vertexattrib[i].size = 4;
        vao->vertexattrib[i].type = GL_FLOAT;
    }
}

// Exports
AliasExport(void,glGenVertexArrays,,(GLsizei n, GLuint *arrays));
AliasExport(void,glBindVertexArray,,(GLuint array));
AliasExport(void,glDeleteVertexArrays,,(GLsizei n, const GLuint *arrays));
AliasExport(GLboolean,glIsVertexArray,,(GLuint array));