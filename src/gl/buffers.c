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


KHASH_MAP_IMPL_INT(buff, glbuffer_t *);
KHASH_MAP_IMPL_INT(glvao, glvao_t*);

static GLuint lastbuffer = 1;

// Utility function to bind / unbind a particular buffer

glbuffer_t** BUFF(GLenum target) {
 switch(target) {
     case GL_ARRAY_BUFFER:
        return &glstate->vao->vertex;
        break;
     case GL_ELEMENT_ARRAY_BUFFER:
        return &glstate->vao->elements;
        break;
     case GL_PIXEL_PACK_BUFFER:
        return &glstate->vao->pack;
        break;
     case GL_PIXEL_UNPACK_BUFFER:
        return &glstate->vao->unpack;
        break;
     default:
       LOGD("Warning, unknown buffer target 0x%04X\n", target);
 }
 return (glbuffer_t**)NULL;
}

void unbind_buffer(GLenum target) {
    glbuffer_t **t = BUFF(target);
    if (t)
		*t=(glbuffer_t*)NULL;
}
void bind_buffer(GLenum target, glbuffer_t* buff) {
    glbuffer_t ** t = BUFF(target);
    if (t)
		*t = buff;
}
glbuffer_t* getbuffer_buffer(GLenum target) {
    glbuffer_t ** t = BUFF(target);
    if (t)
		return *t;
    return NULL;
}
glbuffer_t* getbuffer_id(GLuint buffer) {
    if(!buffer)
        return NULL;
    khint_t k;
    khash_t(buff) *list = glstate->buffers;
    k = kh_get(buff, list, buffer);
    if (k == kh_end(list))
        return NULL;
    return kh_value(list, k);
}

int buffer_target(GLenum target) {
	if (target==GL_ARRAY_BUFFER)
		return 1;
	if (target==GL_ELEMENT_ARRAY_BUFFER)
		return 1;
	if (target==GL_PIXEL_PACK_BUFFER)
		return 1;
	if (target==GL_PIXEL_UNPACK_BUFFER)
		return 1;
	return 0;
}

void rebind_real_buff_arrays(int old_buffer, int new_buffer) {
    for (int j = 0; j < hardext.maxvattrib; j++) {
        if (glstate->vao->vertexattrib[j].real_buffer == old_buffer) {
            glstate->vao->vertexattrib[j].real_buffer = new_buffer;
        }
    }
}

void APIENTRY_GL4ES gl4es_glGenBuffers(GLsizei n, GLuint * buffers) {
    DBG(printf("glGenBuffers(%i, %p)\n", n, buffers);)
	noerrorShim();
    if (n<1) {
		errorShim(GL_INVALID_VALUE);
        return;
    }
    khash_t(buff) *list = glstate->buffers;
    for (int i=0; i<n; i++) {   // create buffer, and check uniqueness...
        int b;
        while(getbuffer_id(b=lastbuffer++));
        buffers[i] = b;
        // create the buffer
        khint_t k;
   	    int ret;
        k = kh_put(buff, list, b, &ret);
        glbuffer_t *buff = kh_value(list, k) = malloc(sizeof(glbuffer_t));
        buff->buffer = b;
        buff->type = 0; // no target for now
        buff->data = NULL; // OPTIMIZED: Always NULL init
        buff->usage = GL_STATIC_DRAW;
        buff->size = 0;
        buff->access = GL_READ_WRITE;
        buff->mapped = 0;
        buff->real_buffer = 0;
    }
}

void APIENTRY_GL4ES gl4es_glBindBuffer(GLenum target, GLuint buffer) {
    DBG(printf("glBindBuffer(%s, %u)\n", PrintEnum(target), buffer);)
    // Flush removed because we are going Native/Direct
    // FLUSH_BEGINEND; 

   	khint_t k;
   	int ret;
	khash_t(buff) *list = glstate->buffers;
	if (!buffer_target(target)) {
		errorShim(GL_INVALID_ENUM);
		return;
	}
    // if buffer = 0 => unbind buffer!
    if (buffer == 0) {
        // unbind buffer
        bindBuffer(target, 0);
        unbind_buffer(target);
    } else {
        // search for an existing buffer
        k = kh_get(buff, list, buffer);
        glbuffer_t *buff = NULL;
        if (k == kh_end(list)){
            k = kh_put(buff, list, buffer, &ret);
            buff = kh_value(list, k) = malloc(sizeof(glbuffer_t));
            buff->buffer = buffer;
            buff->type = target;
            buff->data = NULL; // OPTIMIZED
            buff->usage = GL_STATIC_DRAW;
            buff->size = 0;
            buff->access = GL_READ_WRITE;
            buff->mapped = 0;
            buff->real_buffer = 0;
        } else {
            buff = kh_value(list, k);
            buff->type = target;
        }
        bind_buffer(target, buff);
        
        // OPTIMIZATION: If we have a real buffer, bind it immediately to driver
        if(buff->real_buffer) {
             bindBuffer(target, buff->real_buffer);
        }
    }
    noerrorShim();
}

void APIENTRY_GL4ES gl4es_glBufferData(GLenum target, GLsizeiptr size, const GLvoid * data, GLenum usage) {
    DBG(printf("glBufferData(%s, %zi, %p, %s)\n", PrintEnum(target), size, data, PrintEnum(usage));)
	if (!buffer_target(target)) {
		errorShim(GL_INVALID_ENUM);
		return;
	}
    glbuffer_t *buff = getbuffer_buffer(target);
    if (buff==NULL) {
		errorShim(GL_INVALID_OPERATION);
        LOGE("Warning, null buffer for target=0x%04X for glBufferData\n", target);
        return;
    }
    
    // OPTIMIZED: Always use Real VBO (Hardware Mode)
    // We removed the checks for "globals4es.usevbo" because for Android/Termux we MUST use VBOs.
    if(!buff->real_buffer) {
        LOAD_GLES(glGenBuffers);
        gles_glGenBuffers(1, &buff->real_buffer);
    }

    // Direct Upload to GPU
    LOAD_GLES(glBufferData);
    LOAD_GLES(glBindBuffer);
    bindBuffer(target, buff->real_buffer);
    gles_glBufferData(target, size, data, usage);
    DBG(printf(" => real VBO %d uploaded (Native)\n", buff->real_buffer);)
        
    // MEMORY OPTIMIZATION:
    // We DO NOT allocate shadow copy in RAM (buff->data).
    // This saves MBs of RAM for Minecraft Chunks.
    if (buff->data) {
        free(buff->data);
        buff->data = NULL;
    }

    buff->size = size;
    buff->usage = usage;
    buff->access = GL_READ_WRITE;
    
    // Update VAO linkage if necessary
    for (int i=0; i<hardext.maxvattrib; ++i) {
        vertexattrib_t *v = &glstate->vao->vertexattrib[i];
        if( v->buffer == buff ) {
		    v->real_buffer = v->buffer->real_buffer;
        }
    }
    noerrorShim();
}

void APIENTRY_GL4ES gl4es_glNamedBufferData(GLuint buffer, GLsizeiptr size, const GLvoid * data, GLenum usage) {
    DBG(printf("glNamedBufferData(%u, %zi, %p, %s)\n", buffer, size, data, PrintEnum(usage));)
    glbuffer_t *buff = getbuffer_id(buffer);
    if (buff==NULL) {
        DBG(printf("Named Buffer not found\n");)
		errorShim(GL_INVALID_OPERATION);
        return;
    }

    // Force Real VBO
    if(!buff->real_buffer) {
        LOAD_GLES(glGenBuffers);
        gles_glGenBuffers(1, &buff->real_buffer);
    }
    
    // Direct Upload
    LOAD_GLES(glBufferData);
    LOAD_GLES(glBindBuffer);
    // Use buff->type if set, otherwise assume GL_ARRAY_BUFFER for generic upload
    GLenum target = (buff->type)?buff->type:GL_ARRAY_BUFFER;
    bindBuffer(target, buff->real_buffer);
    gles_glBufferData(target, size, data, usage);

    // Free RAM copy
    if (buff->data) {
        free(buff->data);
        buff->data = NULL;
    }

    buff->size = size;
    buff->usage = usage;
    buff->access = GL_READ_WRITE;

    for (int i=0; i<hardext.maxvattrib; ++i) {
        vertexattrib_t *v = &glstate->vao->vertexattrib[i];
        if( v->buffer == buff ) {
		    v->real_buffer = v->buffer->real_buffer;
        }
    }
    noerrorShim();
}

void APIENTRY_GL4ES gl4es_glBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, const GLvoid * data) {
    DBG(printf("glBufferSubData(%s, %p, %zi, %p)\n", PrintEnum(target), (void*)offset, size, data);)
	if (!buffer_target(target)) {
		errorShim(GL_INVALID_ENUM);
		return;
	}
    glbuffer_t *buff = getbuffer_buffer(target);
    if (buff==NULL) {
		errorShim(GL_INVALID_OPERATION);
        return;
    }

    if (offset<0 || size<0 || offset+size>buff->size) {
        errorShim(GL_INVALID_VALUE);
        return;
    }

    // Direct SubData Upload
    if(buff->real_buffer) {
        LOAD_GLES(glBufferSubData);
        LOAD_GLES(glBindBuffer);
        bindBuffer(target, buff->real_buffer);
        gles_glBufferSubData(target, offset, size, data);
    }
    
    // REMOVED: memcpy((char*)buff->data + offset, data, size);
    // We don't keep RAM copy anymore.
    
    noerrorShim();
}

void APIENTRY_GL4ES gl4es_glNamedBufferSubData(GLuint buffer, GLintptr offset, GLsizeiptr size, const GLvoid * data) {
    DBG(printf("glNamedBufferSubData(%u, %p, %zi, %p)\n", buffer, (void*)offset, size, data);)
    glbuffer_t *buff = getbuffer_id(buffer);
    if (buff==NULL) {
		errorShim(GL_INVALID_OPERATION);
        return;
    }

    if (offset<0 || size<0 || offset+size>buff->size) {
        errorShim(GL_INVALID_VALUE);
        return;
    }
        
    if(buff->real_buffer) {
        GLenum target = (buff->type)?buff->type:GL_ARRAY_BUFFER;
        LOAD_GLES(glBufferSubData);
        LOAD_GLES(glBindBuffer);
        bindBuffer(target, buff->real_buffer);
        gles_glBufferSubData(target, offset, size, data);
    }
    // REMOVED: memcpy to RAM
    noerrorShim();
}

void APIENTRY_GL4ES gl4es_glDeleteBuffers(GLsizei n, const GLuint * buffers) {
    DBG(printf("glDeleteBuffers(%i, %p)\n", n, buffers);)
    if(!glstate) return;
    // FLUSH_BEGINEND removed
    
    VaoSharedClear(glstate->vao);
	khash_t(buff) *list = glstate->buffers;
    if (list) {
        khint_t k;
        glbuffer_t *buff;
        for (int i = 0; i < n; i++) {
            GLuint t = buffers[i];
            DBG(printf("\t deleting %d\n", t);)
            if (t) {    // don't allow to remove default one
                k = kh_get(buff, list, t);
                if (k != kh_end(list)) {
                    buff = kh_value(list, k);
                    if(buff->real_buffer) {
                        rebind_real_buff_arrays(buff->real_buffer, 0);  // unbind
                        LOAD_GLES(glDeleteBuffers);
                        deleteSingleBuffer(buff->real_buffer);
                    }
                    if (glstate->vao->vertex == buff) glstate->vao->vertex = NULL;
                    if (glstate->vao->elements == buff) glstate->vao->elements = NULL;
                    if (glstate->vao->pack == buff) glstate->vao->pack = NULL;
                    if (glstate->vao->unpack == buff) glstate->vao->unpack = NULL;
                    
                    for (int j = 0; j < hardext.maxvattrib; j++)
                        if (glstate->vao->vertexattrib[j].buffer == buff) {
                            glstate->vao->vertexattrib[j].buffer = NULL;
                            glstate->vao->vertexattrib[j].real_buffer = 0;
                        }
                    
                    if (buff->data) free(buff->data);
                    kh_del(buff, list, k);
                    free(buff);
                }
            }
        }
    }
    DBG(printf("\t done\n");)
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
	if (!buffer_target(target)) {
		errorShim(GL_INVALID_ENUM);
		return;
	}
	glbuffer_t* buff = getbuffer_buffer(target);
	if (buff==NULL) {
		errorShim(GL_INVALID_OPERATION);
		return;
	}
    bufferGetParameteriv(buff, value, data);
}
void APIENTRY_GL4ES gl4es_glGetNamedBufferParameteriv(GLuint buffer, GLenum value, GLint * data) {
    DBG(printf("glGetNamedBufferParameteriv(%u, %s, %p)\n", buffer, PrintEnum(value), data);)
	glbuffer_t* buff = getbuffer_id(buffer);
	if (buff==NULL) {
		errorShim(GL_INVALID_OPERATION);
		return;
	}
    bufferGetParameteriv(buff, value, data);
}

// OPTIMIZED: Transient Mapping Strategy
// We allocate memory ONLY when mapped, and free it immediately after unmap.
void* APIENTRY_GL4ES gl4es_glMapBuffer(GLenum target, GLenum access) {
    DBG(printf("glMapBuffer(%s, %s)\n", PrintEnum(target), PrintEnum(access));)
	if (!buffer_target(target)) {
		errorShim(GL_INVALID_ENUM);
		return (void*)NULL;
	}

    glbuffer_t *buff = getbuffer_buffer(target);
    if (buff==NULL) {
        errorShim(GL_INVALID_VALUE);
		return NULL;
    }
    if(buff->mapped) {
        errorShim(GL_INVALID_OPERATION);
        return NULL;
    }
	buff->access = access;
	buff->mapped = 1;
    buff->ranged = 0;
    
    // Create Temporary Buffer for User Interaction
    if(!buff->data) {
        buff->data = malloc(buff->size);
    }
    
	noerrorShim();
	return buff->data;
}

void* APIENTRY_GL4ES gl4es_glMapNamedBuffer(GLuint buffer, GLenum access) {
    DBG(printf("glMapNamedBuffer(%u, %s)\n", buffer, PrintEnum(access));)

	glbuffer_t *buff = getbuffer_id(buffer);
	if (buff==NULL) {
        errorShim(GL_INVALID_VALUE);
		return NULL;
    }
    if(buff->mapped) {
        errorShim(GL_INVALID_OPERATION);
        return NULL;
    }
	buff->access = access;
	buff->mapped = 1;
    buff->ranged = 0;
    
    // Create Temporary Buffer
    if(!buff->data) {
        buff->data = malloc(buff->size);
    }
    
	noerrorShim();
	return buff->data;
}

GLboolean APIENTRY_GL4ES gl4es_glUnmapBuffer(GLenum target) {
    DBG(printf("glUnmapBuffer(%s)\n", PrintEnum(target));)
    // No Flush needed for Native
        
	if (!buffer_target(target)) {
		errorShim(GL_INVALID_ENUM);
		return GL_FALSE;
	}

    glbuffer_t *buff = getbuffer_buffer(target);
    if (buff==NULL) {
        errorShim(GL_INVALID_VALUE);
		return GL_FALSE;
    }
	noerrorShim();
    
    // Upload the Temporary Buffer to GPU
    if(buff->real_buffer && buff->mapped && buff->data) {
        LOAD_GLES(glBufferSubData);
        LOAD_GLES(glBindBuffer);
        bindBuffer(buff->type, buff->real_buffer);
        // Upload the whole buffer (since we don't know what changed in MapBuffer)
        gles_glBufferSubData(buff->type, 0, buff->size, buff->data);
    }

    // CLEANUP: Free the temporary RAM to save memory
    if(buff->data) {
        free(buff->data);
        buff->data = NULL;
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
        
	glbuffer_t *buff = getbuffer_id(buffer);
	if (buff==NULL)
		return GL_FALSE;
	noerrorShim();
    
    // Upload
    if(buff->real_buffer && buff->mapped && buff->data) {
        // Assume default type if unknown
        GLenum target = (buff->type)?buff->type:GL_ARRAY_BUFFER;
        LOAD_GLES(glBufferSubData);
        LOAD_GLES(glBindBuffer);
        bindBuffer(target, buff->real_buffer);
        gles_glBufferSubData(target, 0, buff->size, buff->data);
    }
    
    // CLEANUP
    if(buff->data) {
        free(buff->data);
        buff->data = NULL;
    }

	if (buff->mapped) {
		buff->mapped = 0;
        buff->ranged = 0;
		return GL_TRUE;
	}
	return GL_FALSE;
}

void APIENTRY_GL4ES gl4es_glGetBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, GLvoid * data) {
    // STUB: Reading back from GPU on GLES2/Native is extremely slow.
    // For performance, we assume Write-Only usage (standard for rendering).
    // If we implemented this, we would kill the FPS.
    // Returning 0/Empty avoids the crash but data will be invalid if game relies on readback.
    DBG(printf("glGetBufferSubData ignored for performance\n");)
	noerrorShim();
}

void APIENTRY_GL4ES gl4es_glGetNamedBufferSubData(GLuint buffer, GLintptr offset, GLsizeiptr size, GLvoid * data) {
    DBG(printf("glGetNamedBufferSubData ignored for performance\n");)
	noerrorShim();
}

void APIENTRY_GL4ES gl4es_glGetBufferPointerv(GLenum target, GLenum pname, GLvoid ** params) {
    DBG(printf("glGetBufferPointerv(%s, %s, %p)\n", PrintEnum(target), PrintEnum(pname), params);)
	if (!buffer_target(target)) {
		errorShim(GL_INVALID_ENUM);
		return;
	}
	glbuffer_t *buff = getbuffer_buffer(target);
	if (buff==NULL)
		return;
	if (pname != GL_BUFFER_MAP_POINTER) {
		errorShim(GL_INVALID_ENUM);
		return;
	}
	if (!buff->mapped) {
		params[0] = NULL;
	} else {
		params[0] = buff->data; // This is valid only between Map/Unmap
	}
}
void APIENTRY_GL4ES gl4es_glGetNamedBufferPointerv(GLuint buffer, GLenum pname, GLvoid ** params) {
    DBG(printf("glGetNamedBufferPointerv(%u, %s, %p)\n", buffer, PrintEnum(pname), params);)
	glbuffer_t *buff = getbuffer_id(buffer);
	if (buff==NULL)
		return;
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
	if (!buffer_target(target)) {
		errorShim(GL_INVALID_ENUM);
		return NULL;
	}

	glbuffer_t *buff = getbuffer_buffer(target);
	if (buff==NULL) {
        errorShim(GL_INVALID_VALUE);
		return NULL;
    }
    if(buff->mapped) {
        errorShim(GL_INVALID_OPERATION);
        return NULL;
    }
	buff->access = access;
	buff->mapped = 1;
    buff->ranged = 1;
    buff->offset = offset;
    buff->length = length;
    
    // Transient Allocation
    if(!buff->data) buff->data = malloc(buff->size); // We need full size to respect offsets easily

	noerrorShim();
    uintptr_t ret = (uintptr_t)buff->data;
    ret += offset;
	return (void*)ret;
}

void APIENTRY_GL4ES gl4es_glFlushMappedBufferRange(GLenum target, GLintptr offset, GLsizeiptr length)
{
    DBG(printf("glFlushMappedBufferRange(%s, %p, %zd)\n", PrintEnum(target), (void*)offset, length);)
	if (!buffer_target(target)) {
		errorShim(GL_INVALID_ENUM);
		return;
	}

    glbuffer_t *buff = getbuffer_buffer(target);
    if(!buff) {
        errorShim(GL_INVALID_VALUE);
        return;
    }
    
    if(buff->real_buffer && buff->mapped && buff->data) {
        LOAD_GLES(glBufferSubData);
        bindBuffer(buff->type, buff->real_buffer);
        // Direct upload of the range
        gles_glBufferSubData(buff->type, buff->offset+offset, length, (void*)((uintptr_t)buff->data+buff->offset+offset));
    }
}

// HAPUS BARIS INI (JANGAN DITULIS LAGI):
// extern void *eglGetProcAddress(const char *procname); 

void APIENTRY_GL4ES gl4es_glCopyBufferSubData(GLenum readTarget, GLenum writeTarget, GLintptr readOffset, GLintptr writeOffset, GLsizeiptr size)
{
    DBG(printf("glCopyBufferSubData(%s, %s, %p, %p, %zd)\n", PrintEnum(readTarget), PrintEnum(writeTarget), (void*)readOffset, (void*)writeOffset, size);)
	glbuffer_t *readbuff = getbuffer_buffer(readTarget);
	glbuffer_t *writebuff = getbuffer_buffer(writeTarget);
    if(!readbuff || !writebuff) {
        errorShim(GL_INVALID_VALUE);
        return;
    }
    
    // Hardware Copy (GLES 3.0+ feature, supported by GE8320)
    if(hardext.esversion >= 3 || globals4es.usevbo) { 
        // Kita gunakan pointer function static agar hanya mencari alamat sekali saja
        static void (*gles_glCopyBufferSubData)(GLenum, GLenum, GLintptr, GLintptr, GLsizeiptr) = NULL;
        
        if(!gles_glCopyBufferSubData) {
            // Langsung panggil eglGetProcAddress tanpa deklarasi extern
            gles_glCopyBufferSubData = (void*)eglGetProcAddress("glCopyBufferSubData");
        }

        if(gles_glCopyBufferSubData) {
            LOAD_GLES(glBindBuffer);
            bindBuffer(readTarget, readbuff->real_buffer);
            bindBuffer(writeTarget, writebuff->real_buffer);
            gles_glCopyBufferSubData(readTarget, writeTarget, readOffset, writeOffset, size);
        }
    }
    noerrorShim();
}

void bindBuffer(GLenum target, GLuint buffer)
{
    LOAD_GLES(glBindBuffer);
    if(target==GL_ARRAY_BUFFER) {
        if(glstate->bind_buffer.array == buffer)
            return;
        glstate->bind_buffer.array = buffer;
        gles_glBindBuffer(target, buffer);
        
    } else if (target==GL_ELEMENT_ARRAY_BUFFER) {
        glstate->bind_buffer.want_index = buffer;
        if(glstate->bind_buffer.index == buffer)
            return;
        glstate->bind_buffer.index = buffer;
        gles_glBindBuffer(target, buffer);
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
        glstate->bind_buffer.used = (glstate->bind_buffer.index && glstate->bind_buffer.array)?1:0;
    }
}

void deleteSingleBuffer(GLuint buffer) {
   LOAD_GLES(glDeleteBuffers);
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
    }
    if(glstate->bind_buffer.index) {
        glstate->bind_buffer.index = 0;
        glstate->bind_buffer.want_index = 0;
        gles_glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    }
    glstate->bind_buffer.used = 0;
}

// VAO Handling remains mostly standard as it's state tracking
static GLuint lastvao = 1;

void APIENTRY_GL4ES gl4es_glGenVertexArrays(GLsizei n, GLuint *arrays) {
	noerrorShim();
    if (n<1) {
		errorShim(GL_INVALID_VALUE);
        return;
    }
    for (int i=0; i<n; i++) {
        arrays[i] = lastvao++;
    }
}
void APIENTRY_GL4ES gl4es_glBindVertexArray(GLuint array) {
   	khint_t k;
   	int ret;
	khash_t(glvao) *list = glstate->vaos;
    if (array == 0) {
        glstate->vao = glstate->defaultvao;
    } else {
        k = kh_get(glvao, list, array);
        glvao_t *glvao = NULL;
        if (k == kh_end(list)){
            k = kh_put(glvao, list, array, &ret);
            glvao = kh_value(list, k) = malloc(sizeof(glvao_t));
            VaoInit(glvao);
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
    if(!glstate) return;

	khash_t(glvao) *list = glstate->vaos;
    if (list) {
        khint_t k;
        glvao_t *glvao;
        for (int i = 0; i < n; i++) {
            GLuint t = arrays[i];
            if (t) {
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
    if(!glstate) return GL_FALSE;
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
    if(vao==NULL || vao->shared_arrays==NULL)
        return;
    if(!(--(*vao->shared_arrays))) {
        free(vao->vert.ptr);
        free(vao->color.ptr);
        free(vao->secondary.ptr);
        free(vao->normal.ptr);
        for (int i=0; i<hardext.maxtex; i++)
            free(vao->tex[i].ptr);
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
// VAO Wrappers and aliases are handled above in standard flow.
AliasExport(void,glGenVertexArrays,,(GLsizei n, GLuint *arrays));
AliasExport(void,glBindVertexArray,,(GLuint array));
AliasExport(void,glDeleteVertexArrays,,(GLsizei n, const GLuint *arrays));
AliasExport(GLboolean,glIsVertexArray,,(GLuint array));
