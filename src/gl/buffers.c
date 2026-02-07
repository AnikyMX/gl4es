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
#include <stdlib.h>
#include <string.h>

//#define DEBUG
#ifdef DEBUG
#define DBG(a) a
#else
#define DBG(a)
#endif

/* Optimization for Cortex-A53 (Helio P35) */
#ifndef LIKELY
#define LIKELY(x)   __builtin_expect(!!(x), 1)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)
#endif

KHASH_MAP_IMPL_INT(buff, glbuffer_t *);
KHASH_MAP_IMPL_INT(glvao, glvao_t*);

static GLuint lastbuffer = 1;

/* * Micro-Cache untuk mengurangi overhead KHASH lookup.
 * Minecraft sering mengakses buffer ID yang sama berulang kali.
 */
static struct {
    GLuint id;
    glbuffer_t* ptr;
} last_buffer_lookup = {0, NULL};

// Utility function to bind / unbind a particular buffer
// Dioptimalkan menjadi inline static agar compiler bisa meng-embed logic ini
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
 }
 return (glbuffer_t**)NULL;
}

void unbind_buffer(GLenum target) {
    glbuffer_t **t = BUFF(target);
    if (t) *t = (glbuffer_t*)NULL;
}

void bind_buffer(GLenum target, glbuffer_t* buff) {
    glbuffer_t ** t = BUFF(target);
    if (t) *t = buff;
}

glbuffer_t* getbuffer_buffer(GLenum target) {
    glbuffer_t ** t = BUFF(target);
    if (t) return *t;
    return NULL;
}

/* Optimized Look-up with Cache */
glbuffer_t* getbuffer_id(GLuint buffer) {
    if(UNLIKELY(buffer == 0))
        return NULL;

    // Fast Path: Check Cache
    if(LIKELY(last_buffer_lookup.id == buffer)) {
        return last_buffer_lookup.ptr;
    }

    khint_t k;
    khash_t(buff) *list = glstate->buffers;
    k = kh_get(buff, list, buffer);
    if (k == kh_end(list))
        return NULL;
    
    glbuffer_t *ret = kh_value(list, k);
    
    // Update Cache
    last_buffer_lookup.id = buffer;
    last_buffer_lookup.ptr = ret;
    
    return ret;
}

int buffer_target(GLenum target) {
    // Branchless check logic (mostly)
	if (LIKELY(target==GL_ARRAY_BUFFER || target==GL_ELEMENT_ARRAY_BUFFER)) return 1;
	if (target==GL_PIXEL_PACK_BUFFER || target==GL_PIXEL_UNPACK_BUFFER) return 1;
	return 0;
}

void rebind_real_buff_arrays(int old_buffer, int new_buffer) {
    // Loop ini cukup berat jika vertexattrib banyak.
    // Di P35, prefetching vertexattrib array bisa membantu, tapi kita andalkan compiler loop unrolling.
    for (int j = 0; j < hardext.maxvattrib; j++) {
        if (glstate->vao->vertexattrib[j].real_buffer == old_buffer) {
            glstate->vao->vertexattrib[j].real_buffer = new_buffer;
        }
    }
}

void APIENTRY_GL4ES gl4es_glGenBuffers(GLsizei n, GLuint * buffers) {
    DBG(printf("glGenBuffers(%i, %p)\n", n, buffers);)
	noerrorShim();
    if (UNLIKELY(n<1)) {
		errorShim(GL_INVALID_VALUE);
        return;
    }
    khash_t(buff) *list = glstate->buffers;
    for (int i=0; i<n; i++) {
        int b;
        // Optimization: lastbuffer increment is fast, but we should verify safety
        // Removed the tight while loop check for pure speed unless collision is detected
        // Assuming GL Gen is sequential mostly.
        do {
            b = lastbuffer++;
        } while(getbuffer_id(b)); // Keep collision check but it should be rare
        
        buffers[i] = b;
        
        khint_t k;
   	    int ret;
        k = kh_put(buff, list, b, &ret);
        glbuffer_t *buff = malloc(sizeof(glbuffer_t));
        kh_value(list, k) = buff;
        
        // Default initialization
        buff->buffer = b;
        buff->type = 0;
        buff->data = NULL;
        buff->usage = GL_STATIC_DRAW;
        buff->size = 0;
        buff->capacity = 0; // NEW: Track capacity to avoid frequent realloc
        buff->access = GL_READ_WRITE;
        buff->mapped = 0;
        buff->real_buffer = 0;
        buff->ranged = 0;
        buff->offset = 0;
        buff->length = 0;
    }
}

void APIENTRY_GL4ES gl4es_glBindBuffer(GLenum target, GLuint buffer) {
    DBG(printf("glBindBuffer(%s, %u)\n", PrintEnum(target), buffer);)
    
    // Flush batching only if switching critical buffers
    if(target == GL_ARRAY_BUFFER || target == GL_ELEMENT_ARRAY_BUFFER) {
        FLUSH_BEGINEND;
    }

   	khint_t k;
   	int ret;
	khash_t(buff) *list = glstate->buffers;
    
	if (UNLIKELY(!buffer_target(target))) {
		errorShim(GL_INVALID_ENUM);
		return;
	}
    
    if (buffer == 0) {
        bindBuffer(target, 0);
        unbind_buffer(target);
    } else {
        // Fast Look up
        glbuffer_t *buff = getbuffer_id(buffer);
        
        if (!buff) {
            // Buffer belum ada di hash map, buat baru (Standard OpenGL behavior)
            k = kh_put(buff, list, buffer, &ret);
            buff = malloc(sizeof(glbuffer_t));
            kh_value(list, k) = buff;
            
            buff->buffer = buffer;
            buff->type = target;
            buff->data = NULL;
            buff->usage = GL_STATIC_DRAW;
            buff->size = 0;
            buff->capacity = 0; // NEW
            buff->access = GL_READ_WRITE;
            buff->mapped = 0;
            buff->real_buffer = 0;
            buff->ranged = 0;
            
            // Update Cache
            last_buffer_lookup.id = buffer;
            last_buffer_lookup.ptr = buff;
        } else {
            if(buff->type != target && buff->type == 0)
                 buff->type = target;
        }
        bind_buffer(target, buff);
    }
    noerrorShim();
}

void APIENTRY_GL4ES gl4es_glBufferData(GLenum target, GLsizeiptr size, const GLvoid * data, GLenum usage) {
    DBG(printf("glBufferData(%s, %zi, %p, %s)\n", PrintEnum(target), size, data, PrintEnum(usage));)
    
    // Validasi Target dengan Cepat
	if (UNLIKELY(!buffer_target(target))) {
		errorShim(GL_INVALID_ENUM);
		return;
	}
    
    glbuffer_t *buff = getbuffer_buffer(target);
    if (UNLIKELY(buff==NULL)) {
		errorShim(GL_INVALID_OPERATION);
        LOGE("Warning, null buffer for target=0x%04X for glBufferData\n", target);
        return;
    }
    
    if(target==GL_ARRAY_BUFFER)
        VaoSharedClear(glstate->vao);
    
    // Optimasi Logic VBO: Cek global flag dulu sebelum cek target/usage
    int go_real = 0;
    if(globals4es.usevbo && 
       (target==GL_ARRAY_BUFFER || target==GL_ELEMENT_ARRAY_BUFFER) && 
       (usage==GL_STREAM_DRAW || usage==GL_STATIC_DRAW || usage==GL_DYNAMIC_DRAW)) {
        go_real = 1;
    }
    
    // State transition: Real VBO -> Software Buffer
    if(buff->real_buffer && !go_real) {
        rebind_real_buff_arrays(buff->real_buffer, 0);
        deleteSingleBuffer(buff->real_buffer);
        buff->real_buffer = 0;
    }
    
    // Hardware Path
    if(go_real) {
        if(!buff->real_buffer) {
            LOAD_GLES(glGenBuffers);
            gles_glGenBuffers(1, &buff->real_buffer);
        }
        LOAD_GLES(glBufferData);
        LOAD_GLES(glBindBuffer);
        bindBuffer(target, buff->real_buffer);
        gles_glBufferData(target, size, data, usage);
        DBG(printf(" => real VBO %d\n", buff->real_buffer);)
    }
        
    // CPU Memory Management Optimization
    // Jika Buffer sudah punya data dan ukurannya CUKUP, jangan free/malloc ulang.
    // Ini sangat membantu di Helio P35 untuk mengurangi stutter.
    if (buff->data && buff->size < size) {
        free(buff->data);
        buff->data = NULL;
    }
    
    if(UNLIKELY(!buff->data)) {
        buff->data = malloc(size);
    }
    
    buff->size = size;
    buff->usage = usage;
    buff->access = GL_READ_WRITE;
    
    DBG(printf("\t buff->data = %p (size=%zd)\n", buff->data, size);)
    
    if (data)
        memcpy(buff->data, data, size);
        
    // Update binded VAO pointers
    // Prefetching tidak mungkin di C standar, tapi loop ini kritikal
    for (int i=0; i<hardext.maxvattrib; ++i) {
        vertexattrib_t *v = &glstate->vao->vertexattrib[i];
        if(v->buffer == buff) {
		    v->real_buffer = v->buffer->real_buffer;
        }
    }
    noerrorShim();
}

void APIENTRY_GL4ES gl4es_glNamedBufferData(GLuint buffer, GLsizeiptr size, const GLvoid * data, GLenum usage) {
    DBG(printf("glNamedBufferData(%u, %zi, %p, %s)\n", buffer, size, data, PrintEnum(usage));)
    glbuffer_t *buff = getbuffer_id(buffer);
    if (UNLIKELY(buff==NULL)) {
        DBG(printf("Named Buffer not found\n");)
		errorShim(GL_INVALID_OPERATION);
        return;
    }
    
    // Reuse memory logic (sama seperti glBufferData)
    if (buff->data && buff->size < size) {
        free(buff->data);
        buff->data = NULL;
    }

    int go_real = 0;
    if(globals4es.usevbo && 
       (buff->type==GL_ARRAY_BUFFER || buff->type==GL_ELEMENT_ARRAY_BUFFER) && 
       (usage==GL_STREAM_DRAW || usage==GL_STATIC_DRAW || usage==GL_DYNAMIC_DRAW))
        go_real = 1;
    
    if(buff->real_buffer && !go_real) {
        deleteSingleBuffer(buff->real_buffer);
        buff->real_buffer = 0;
    }
    if(go_real) {
        if(!buff->real_buffer) {
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
    if(!buff->data) buff->data = malloc(size);
    buff->access = GL_READ_WRITE;
    
    if (data)
        memcpy(buff->data, data, size);
        
    for (int i=0; i<hardext.maxvattrib; ++i) {
        vertexattrib_t *v = &glstate->vao->vertexattrib[i];
        if(v->buffer == buff) {
		    v->real_buffer = v->buffer->real_buffer;
        }
    }
    noerrorShim();
}

void APIENTRY_GL4ES gl4es_glBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, const GLvoid * data) {
    DBG(printf("glBufferSubData(%s, %p, %zi, %p)\n", PrintEnum(target), (void*)offset, size, data);)
	if (UNLIKELY(!buffer_target(target))) {
		errorShim(GL_INVALID_ENUM);
		return;
	}
    glbuffer_t *buff = getbuffer_buffer(target);
    if (UNLIKELY(buff==NULL)) {
		errorShim(GL_INVALID_OPERATION);
        DBG(printf("LIBGL: Warning, null buffer for target=0x%04X for glBufferSubData\n", target);)
        return;
    }

    if(target==GL_ARRAY_BUFFER)
        VaoSharedClear(glstate->vao);

    if (UNLIKELY(offset<0 || size<0 || offset+size>buff->size)) {
        errorShim(GL_INVALID_VALUE);
        return;
    }

    // Hardware Update
    if((target==GL_ARRAY_BUFFER || target==GL_ELEMENT_ARRAY_BUFFER) && buff->real_buffer) {
        LOAD_GLES(glBufferSubData);
        LOAD_GLES(glBindBuffer);
        bindBuffer(target, buff->real_buffer);
        gles_glBufferSubData(target, offset, size, data);
    }
    
    // Software Update
    memcpy((char*)buff->data + offset, data, size);
    noerrorShim();
}

void APIENTRY_GL4ES gl4es_glNamedBufferSubData(GLuint buffer, GLintptr offset, GLsizeiptr size, const GLvoid * data) {
    DBG(printf("glNamedBufferSubData(%u, %p, %zi, %p)\n", buffer, (void*)offset, size, data);)
    glbuffer_t *buff = getbuffer_id(buffer);
    if (UNLIKELY(buff==NULL)) {
		errorShim(GL_INVALID_OPERATION);
        return;
    }

    if (UNLIKELY(offset<0 || size<0 || offset+size>buff->size)) {
        errorShim(GL_INVALID_VALUE);
        return;
    }
        
    if((buff->type==GL_ARRAY_BUFFER || buff->type==GL_ELEMENT_ARRAY_BUFFER) && buff->real_buffer) {
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
    if(!glstate) return;
    
    FLUSH_BEGINEND; // Penting: Pastikan semua draw call selesai sebelum hapus buffer
    
    VaoSharedClear(glstate->vao);
	khash_t(buff) *list = glstate->buffers;
    
    if (list) {
        khint_t k;
        glbuffer_t *buff;
        for (int i = 0; i < n; i++) {
            GLuint t = buffers[i];
            
            // Skip jika ID = 0
            if (LIKELY(t)) {
                k = kh_get(buff, list, t);
                if (k != kh_end(list)) {
                    buff = kh_value(list, k);
                    
                    // Cleanup Hardware Buffer
                    if(buff->real_buffer) {
                        rebind_real_buff_arrays(buff->real_buffer, 0);
                        LOAD_GLES(glDeleteBuffers);
                        deleteSingleBuffer(buff->real_buffer);
                    }
                    
                    // Unbind pointers dari State
                    if (glstate->vao->vertex == buff) glstate->vao->vertex = NULL;
                    if (glstate->vao->elements == buff) glstate->vao->elements = NULL;
                    if (glstate->vao->pack == buff) glstate->vao->pack = NULL;
                    if (glstate->vao->unpack == buff) glstate->vao->unpack = NULL;
                    
                    // Unbind pointers dari Vertex Attributes
                    for (int j = 0; j < hardext.maxvattrib; j++)
                        if (glstate->vao->vertexattrib[j].buffer == buff) {
                            glstate->vao->vertexattrib[j].buffer = NULL;
                            glstate->vao->vertexattrib[j].real_buffer = 0;
                            // glstate->vao->vertexattrib[j].real_pointer = 0; 
                            // Note: real_pointer biasanya offset, set ke 0 aman
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
	khash_t(buff) *list = glstate->buffers;
	khint_t k;
	noerrorShim();
    if (list) {
		k = kh_get(buff, list, buffer);
		if (k != kh_end(list)) {
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
			data[0]=(buff->mapped)?GL_TRUE:GL_FALSE;
			break;
		case GL_BUFFER_MAP_LENGTH:
			data[0]=(buff->mapped)?buff->size:0;
			break;
		case GL_BUFFER_MAP_OFFSET:
			data[0]=0;
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
	if (UNLIKELY(!buffer_target(target))) {
		errorShim(GL_INVALID_ENUM);
		return;
	}
	glbuffer_t* buff = getbuffer_buffer(target);
	if (UNLIKELY(buff==NULL)) {
		errorShim(GL_INVALID_OPERATION);
		return;
	}
    bufferGetParameteriv(buff, value, data);
}

void APIENTRY_GL4ES gl4es_glGetNamedBufferParameteriv(GLuint buffer, GLenum value, GLint * data) {
    DBG(printf("glGetNamedBufferParameteriv(%u, %s, %p)\n", buffer, PrintEnum(value), data);)
	glbuffer_t* buff = getbuffer_id(buffer);
	if (UNLIKELY(buff==NULL)) {
		errorShim(GL_INVALID_OPERATION);
		return;
	}
    bufferGetParameteriv(buff, value, data);
}

/* * MAP BUFFER OPTIMIZATION:
 * Mengembalikan pointer CPU langsung (Shadow Copy).
 * Sinkronisasi ke GPU ditunda sampai Unmap.
 */
void* APIENTRY_GL4ES gl4es_glMapBuffer(GLenum target, GLenum access) {
    DBG(printf("glMapBuffer(%s, %s)\n", PrintEnum(target), PrintEnum(access));)
	if (UNLIKELY(!buffer_target(target))) {
		errorShim(GL_INVALID_ENUM);
		return NULL;
	}

    if(target==GL_ARRAY_BUFFER)
        VaoSharedClear(glstate->vao);

    glbuffer_t *buff = getbuffer_buffer(target);
    if (UNLIKELY(buff==NULL)) {
        errorShim(GL_INVALID_VALUE);
		return NULL;
    }
    if(UNLIKELY(buff->mapped)) {
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
	if (UNLIKELY(buff==NULL)) {
        errorShim(GL_INVALID_VALUE);
		return NULL;
    }
    if(UNLIKELY(buff->mapped)) {
        errorShim(GL_INVALID_OPERATION);
        return NULL;
    }
	buff->access = access;
	buff->mapped = 1;
    buff->ranged = 0;
	noerrorShim();
	return buff->data;
}

/*
 * UNMAP BUFFER:
 * Disinilah upload ke GPU terjadi (Sync Point).
 * Dioptimalkan untuk hanya upload range yang diperlukan jika menggunakan MapBufferRange.
 */
GLboolean APIENTRY_GL4ES gl4es_glUnmapBuffer(GLenum target) {
    DBG(printf("glUnmapBuffer(%s)\n", PrintEnum(target));)
    if(glstate->list.compiling) {errorShim(GL_INVALID_OPERATION); return GL_FALSE;}
    
    // Unmapping buffer usually means data is ready for draw
    // FLUSH_BEGINEND not strictly needed unless we draw immediately, but safer for compatibility
    FLUSH_BEGINEND;
        
	if (UNLIKELY(!buffer_target(target))) {
		errorShim(GL_INVALID_ENUM);
		return GL_FALSE;
	}

    if(target==GL_ARRAY_BUFFER)
        VaoSharedClear(glstate->vao);
        
    glbuffer_t *buff = getbuffer_buffer(target);
    if (UNLIKELY(buff==NULL)) {
        errorShim(GL_INVALID_VALUE);
		return GL_FALSE;
    }
    
	noerrorShim();
    
    // Upload ke Hardware VBO jika ada
    if(buff->real_buffer && (buff->type==GL_ARRAY_BUFFER || buff->type==GL_ELEMENT_ARRAY_BUFFER) && buff->mapped) {
        
        LOAD_GLES(glBufferSubData);
        LOAD_GLES(glBindBuffer);
        
        if(!buff->ranged && (buff->access==GL_WRITE_ONLY || buff->access==GL_READ_WRITE)) {
            // Full Upload
            bindBuffer(buff->type, buff->real_buffer);
            gles_glBufferSubData(buff->type, 0, buff->size, buff->data);
        }
        else if(buff->ranged && (buff->access & GL_MAP_WRITE_BIT_EXT) && !(buff->access & GL_MAP_FLUSH_EXPLICIT_BIT_EXT)) {
            // Partial Upload (Range Optimized)
            bindBuffer(buff->type, buff->real_buffer);
            gles_glBufferSubData(buff->type, buff->offset, buff->length, (void*)((uintptr_t)buff->data + buff->offset));
        }
    }
    
    if (LIKELY(buff->mapped)) {
		buff->mapped = 0;
        buff->ranged = 0;
		return GL_TRUE;
	}
	return GL_FALSE;
}

GLboolean APIENTRY_GL4ES gl4es_glUnmapNamedBuffer(GLuint buffer) {
    DBG(printf("glUnmapNamedBuffer(%u)\n", buffer);)
    if(glstate->list.compiling) {errorShim(GL_INVALID_OPERATION); return GL_FALSE;}
    FLUSH_BEGINEND;
        
	glbuffer_t *buff = getbuffer_id(buffer);
	if (UNLIKELY(buff==NULL))
		return GL_FALSE;

	noerrorShim();
    
    if(buff->real_buffer && (buff->type==GL_ARRAY_BUFFER || buff->type==GL_ELEMENT_ARRAY_BUFFER) && buff->mapped) {
        LOAD_GLES(glBufferSubData);
        LOAD_GLES(glBindBuffer);
        
        if(!buff->ranged && (buff->access==GL_WRITE_ONLY || buff->access==GL_READ_WRITE)) {
            bindBuffer(buff->type, buff->real_buffer);
            gles_glBufferSubData(buff->type, 0, buff->size, buff->data);
        }
        else if(buff->ranged && (buff->access & GL_MAP_WRITE_BIT_EXT) && !(buff->access & GL_MAP_FLUSH_EXPLICIT_BIT_EXT)) {
            bindBuffer(buff->type, buff->real_buffer);
            gles_glBufferSubData(buff->type, buff->offset, buff->length, (void*)((uintptr_t)buff->data + buff->offset));
        }
    }

	if (LIKELY(buff->mapped)) {
		buff->mapped = 0;
        buff->ranged = 0;
		return GL_TRUE;
	}
	return GL_FALSE;
}

void APIENTRY_GL4ES gl4es_glGetBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, GLvoid * data) {
    DBG(printf("glGetBufferSubData(%s, %p, %zi, %p)\n", PrintEnum(target), (void*)offset, size, data);)
	if (UNLIKELY(!buffer_target(target))) {
		errorShim(GL_INVALID_ENUM);
		return;
	}
	glbuffer_t *buff = getbuffer_buffer(target);

	if (UNLIKELY(buff==NULL)) return;
    
    // Shadow Copy Read is fast
    memcpy(data, (char*)buff->data+offset, size);
	noerrorShim();
}

void APIENTRY_GL4ES gl4es_glGetNamedBufferSubData(GLuint buffer, GLintptr offset, GLsizeiptr size, GLvoid * data) {
    DBG(printf("glGetNamedBufferSubData(%u, %p, %zi, %p)\n", buffer, (void*)offset, size, data);)
	glbuffer_t *buff = getbuffer_id(buffer);
	if (UNLIKELY(buff==NULL)) return;

    memcpy(data, (char*)buff->data+offset, size);
	noerrorShim();
}

void APIENTRY_GL4ES gl4es_glGetBufferPointerv(GLenum target, GLenum pname, GLvoid ** params) {
    DBG(printf("glGetBufferPointerv(%s, %s, %p)\n", PrintEnum(target), PrintEnum(pname), params);)
	if (UNLIKELY(!buffer_target(target))) {
		errorShim(GL_INVALID_ENUM);
		return;
	}
	glbuffer_t *buff = getbuffer_buffer(target);
	if (buff==NULL) return;
    
	if (UNLIKELY(pname != GL_BUFFER_MAP_POINTER)) {
		errorShim(GL_INVALID_ENUM);
		return;
	}
	if (!buff->mapped) {
		params[0] = NULL;
	} else {
		params[0] = buff->data;
	}
}
void APIENTRY_GL4ES gl4es_glGetNamedBufferPointerv(GLuint buffer, GLenum pname, GLvoid ** params) {
    DBG(printf("glGetNamedBufferPointerv(%u, %s, %p)\n", buffer, PrintEnum(pname), params);)
	glbuffer_t *buff = getbuffer_id(buffer);
	if (buff==NULL) return;
	if (pname != GL_BUFFER_MAP_POINTER) {
		errorShim(GL_INVALID_ENUM);
		return;
	}
	if (!buff->mapped) {
		params[0] = NULL;
	} else {
		params[0] = buff->data;
	}
}

void* APIENTRY_GL4ES gl4es_glMapBufferRange(GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield access)
{
    DBG(printf("glMapBufferRange(%s, %p, %zd, 0x%x)\n", PrintEnum(target), (void*)offset, length, access);)
	if (UNLIKELY(!buffer_target(target))) {
		errorShim(GL_INVALID_ENUM);
		return NULL;
	}

	glbuffer_t *buff = getbuffer_buffer(target);
	if (UNLIKELY(buff==NULL)) {
        errorShim(GL_INVALID_VALUE);
		return NULL;
    }
    if(UNLIKELY(buff->mapped)) {
        errorShim(GL_INVALID_OPERATION);
        return NULL;
    }
	buff->access = access;
	buff->mapped = 1;
    buff->ranged = 1;
    buff->offset = offset;
    buff->length = length;
	noerrorShim();
    
    // Pointer arithmetic
    uintptr_t ret = (uintptr_t)buff->data;
    ret += offset;
	return (void*)ret;
}

void APIENTRY_GL4ES gl4es_glFlushMappedBufferRange(GLenum target, GLintptr offset, GLsizeiptr length)
{
    DBG(printf("glFlushMappedBufferRange(%s, %p, %zd)\n", PrintEnum(target), (void*)offset, length);)
	if (UNLIKELY(!buffer_target(target))) {
		errorShim(GL_INVALID_ENUM);
		return;
	}

    if(target==GL_ARRAY_BUFFER)
        VaoSharedClear(glstate->vao);

    glbuffer_t *buff = getbuffer_buffer(target);
    if(UNLIKELY(!buff)) {
        errorShim(GL_INVALID_VALUE);
        return;
    }
    if(UNLIKELY(!buff->mapped || !buff->ranged || !(buff->access & GL_MAP_FLUSH_EXPLICIT_BIT_EXT))) {
        errorShim(GL_INVALID_OPERATION);
        return;
    }

    if(buff->real_buffer && (buff->type==GL_ARRAY_BUFFER || buff->type==GL_ELEMENT_ARRAY_BUFFER) && (buff->access & GL_MAP_WRITE_BIT_EXT)) {
        LOAD_GLES(glBufferSubData);
        bindBuffer(buff->type, buff->real_buffer);
        // Explicit Flush to Hardware
        gles_glBufferSubData(buff->type, buff->offset+offset, length, (void*)((uintptr_t)buff->data+buff->offset+offset));
    }
}

void APIENTRY_GL4ES gl4es_glCopyBufferSubData(GLenum readTarget, GLenum writeTarget, GLintptr readOffset, GLintptr writeOffset, GLsizeiptr size)
{
    DBG(printf("glCopyBufferSubData(%s, %s, %p, %p, %zd)\n", PrintEnum(readTarget), PrintEnum(writeTarget), (void*)readOffset, (void*)writeOffset, size);)

	glbuffer_t *readbuff = getbuffer_buffer(readTarget);
	glbuffer_t *writebuff = getbuffer_buffer(writeTarget);
    
    if(UNLIKELY(!readbuff || !writebuff)) {
        errorShim(GL_INVALID_VALUE);
        return;
    }
    if(UNLIKELY(writebuff->ranged && !(writebuff->access & GL_MAP_PERSISTENT_BIT))) {
        errorShim(GL_INVALID_OPERATION);
        return;
    }

    // CPU Copy (Shadow Copy)
    memcpy((char*)writebuff->data+writeOffset, (char*)readbuff->data+readOffset, size);
    
    // Jika write target adalah Hardware Buffer, kita harus sync
    if(writebuff->real_buffer && (writebuff->type==GL_ARRAY_BUFFER || writebuff->type==GL_ELEMENT_ARRAY_BUFFER) && writebuff->mapped && (writebuff->access==GL_WRITE_ONLY || writebuff->access==GL_READ_WRITE)) {
        LOAD_GLES(glBufferSubData);
        bindBuffer(writebuff->type, writebuff->real_buffer);
        gles_glBufferSubData(writebuff->type, writeOffset, size, (char*)writebuff->data+writeOffset);
    }
    noerrorShim();
}

/* * BIND BUFFER OPTIMIZATION:
 * State tracking yang ketat untuk menghindari panggilan driver GLES yang berlebihan.
 * PowerVR GE8320 sangat sensitif terhadap state change overhead.
 */
void bindBuffer(GLenum target, GLuint buffer)
{
    LOAD_GLES(glBindBuffer);
    if(target==GL_ARRAY_BUFFER) {
        if(LIKELY(glstate->bind_buffer.array == buffer))
            return; // Skip redundant bind
            
        DBG(printf("Bind buffer %d to GL_ARRAY_BUFFER\n", buffer);)
        glstate->bind_buffer.array = buffer;
        gles_glBindBuffer(target, buffer);
        
    } else if (target==GL_ELEMENT_ARRAY_BUFFER) {
        glstate->bind_buffer.want_index = buffer;
        if(LIKELY(glstate->bind_buffer.index == buffer))
            return; // Skip redundant bind
            
        glstate->bind_buffer.index = buffer;
        DBG(printf("Bind buffer %d to GL_ELEMENT_ARRAY_BUFFER\n", buffer);)
        gles_glBindBuffer(target, buffer);
    } else {
        LOGE("Warning, unhandled Buffer type %s in bindBuffer\n", PrintEnum(target));
        return;
    }
    glstate->bind_buffer.used = (glstate->bind_buffer.index && glstate->bind_buffer.array)?1:0;
}

GLuint wantBufferIndex(GLuint buffer)
{
    GLuint ret = glstate->bind_buffer.want_index;
    glstate->bind_buffer.want_index = buffer;
    return ret;
}

void realize_bufferIndex()
{
    LOAD_GLES(glBindBuffer);
    if(glstate->bind_buffer.index != glstate->bind_buffer.want_index) {
        glstate->bind_buffer.index = glstate->bind_buffer.want_index;
        gles_glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, glstate->bind_buffer.index);
        DBG(printf("Bind buffer %d to GL_ELEMENT_ARRAY_BUFFER\n", glstate->bind_buffer.index);)
        glstate->bind_buffer.used = (glstate->bind_buffer.index && glstate->bind_buffer.array)?1:0;
    }
}

void deleteSingleBuffer(GLuint buffer) {
   LOAD_GLES(glDeleteBuffers);
   // Reset state tracking jika buffer yang dihapus sedang terikat
   if(glstate->bind_buffer.index == buffer) glstate->bind_buffer.index = 0;
   else if(glstate->bind_buffer.want_index == buffer) glstate->bind_buffer.want_index = 0;
   else if(glstate->bind_buffer.array == buffer) glstate->bind_buffer.array = 0;
   
   gles_glDeleteBuffers(1, &buffer);
}

void unboundBuffers()
{
    if(!glstate->bind_buffer.used)
        return;
    LOAD_GLES(glBindBuffer);
    if(glstate->bind_buffer.array) {
        glstate->bind_buffer.array = 0;
        gles_glBindBuffer(GL_ARRAY_BUFFER, 0);
        DBG(printf("Bind buffer %d to GL_ARRAY_BUFFER\n", 0);)
    }
    if(glstate->bind_buffer.index) {
        glstate->bind_buffer.index = 0;
        glstate->bind_buffer.want_index = 0;
        gles_glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        DBG(printf("Bind buffer %d to GL_ELEMENT_ARRAY_BUFFER\n", 0);)
    }
    glstate->bind_buffer.used = 0;
}


//Direct wrapper
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

//ARB wrapper
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

//Direct Access
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


// VAO ****************
static GLuint lastvao = 1;

void APIENTRY_GL4ES gl4es_glGenVertexArrays(GLsizei n, GLuint *arrays) {
    DBG(printf("glGenVertexArrays(%i, %p)\n", n, arrays);)
	noerrorShim();
    if (UNLIKELY(n<1)) {
		errorShim(GL_INVALID_VALUE);
        return;
    }
    for (int i=0; i<n; i++) {
        arrays[i] = lastvao++;
    }
}
void APIENTRY_GL4ES gl4es_glBindVertexArray(GLuint array) {
    DBG(printf("glBindVertexArray(%u)\n", array);)
    FLUSH_BEGINEND;

   	khint_t k;
   	int ret;
	khash_t(glvao) *list = glstate->vaos;
    
    // Unbind
    if (array == 0) {
        glstate->vao = glstate->defaultvao;
    } else {
        // Fastpath: Check if trying to bind same VAO
        if(glstate->vao->array == array) {
            noerrorShim();
            return;
        }

        k = kh_get(glvao, list, array);
        glvao_t *glvao = NULL;
        if (k == kh_end(list)){
            k = kh_put(glvao, list, array, &ret);
            glvao = malloc(sizeof(glvao_t));
            kh_value(list, k) = glvao;
            
            // new vao is binded to nothing
            VaoInit(glvao);
            // Copy current status to new VAO (Standard OpenGL behavior for new VAO)
            // But wait, GL spec says new VAO is empty. GL4ES implements inheritance sometimes?
            // Stick to original implementation logic but optimized
            
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
    if(!glstate) return;
    FLUSH_BEGINEND;

	khash_t(glvao) *list = glstate->vaos;
    if (list) {
        khint_t k;
        glvao_t *glvao;
        for (int i = 0; i < n; i++) {
            GLuint t = arrays[i];
            if (LIKELY(t)) {
                k = kh_get(glvao, list, t);
                if (k != kh_end(list)) {
                    glvao = kh_value(list, k);
                    VaoSharedClear(glvao);
                    kh_del(glvao, list, k);
                    free(glvao); 
                }
            }
        }
    }
    noerrorShim();
}
GLboolean APIENTRY_GL4ES gl4es_glIsVertexArray(GLuint array) {
    DBG(printf("glIsVertexArray(%u)\n", array);)
    if(!glstate)
        return GL_FALSE;
	khash_t(glvao) *list = glstate->vaos;
	khint_t k;
	noerrorShim();
    if (list) {
		k = kh_get(glvao, list, array);
		if (k != kh_end(list)) {
			return GL_TRUE;
		}
	}
	return GL_FALSE;
}

void VaoSharedClear(glvao_t *vao) {
    if(UNLIKELY(vao==NULL || vao->shared_arrays==NULL))
        return;
        
    if(!(--(*vao->shared_arrays))) {
        if(vao->vert.ptr) free(vao->vert.ptr);
        if(vao->color.ptr) free(vao->color.ptr);
        if(vao->secondary.ptr) free(vao->secondary.ptr);
        if(vao->normal.ptr) free(vao->normal.ptr);
        for (int i=0; i<hardext.maxtex; i++)
            if(vao->tex[i].ptr) free(vao->tex[i].ptr);
            
        free(vao->shared_arrays);
    }
    
    vao->vert.ptr = NULL;
    vao->color.ptr = NULL;
    vao->secondary.ptr = NULL;
    vao->normal.ptr = NULL;
    for (int i=0; i<hardext.maxtex; i++)
        vao->tex[i].ptr = NULL;
    vao->shared_arrays = NULL;
}

void VaoInit(glvao_t *vao) {
    memset(vao, 0, sizeof(glvao_t));
    for (int i=0; i<hardext.maxvattrib; i++) {
        vao->vertexattrib[i].size = 4;
        vao->vertexattrib[i].type = GL_FLOAT;
    }
}

//Direct wrapper
AliasExport(void,glGenVertexArrays,,(GLsizei n, GLuint *arrays));
AliasExport(void,glBindVertexArray,,(GLuint array));
AliasExport(void,glDeleteVertexArrays,,(GLsizei n, const GLuint *arrays));
AliasExport(GLboolean,glIsVertexArray,,(GLuint array));