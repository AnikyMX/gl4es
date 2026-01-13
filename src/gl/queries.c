#ifndef _WIN32
#ifdef USE_CLOCK
#include <time.h>
#else
#include <sys/time.h>
#endif
#endif

#include "queries.h"
#include "khash.h"
#include "gl4es.h"
#include "glstate.h"
#include "loader.h"

#ifdef _WIN32
#ifdef _WINBASE_
#define GSM_CAST(c) ((LPFILETIME)c)
#else
__declspec(dllimport)
void __stdcall GetSystemTimeAsFileTime(unsigned __int64*);
#define GSM_CAST(c) ((__int64*)c)
#endif
#endif

// --- MACRO DEFINITIONS ---
#ifndef GL_ANY_SAMPLES_PASSED
#define GL_ANY_SAMPLES_PASSED 0x8C2F
#endif
#ifndef GL_ANY_SAMPLES_PASSED_CONSERVATIVE
#define GL_ANY_SAMPLES_PASSED_CONSERVATIVE 0x8D6A
#endif
// -------------------------

// --- [CRITICAL] GET_CLOCK FUNCTION ---
unsigned long long get_clock() {
	unsigned long long now;
	#ifdef _WIN32
        GetSystemTimeAsFileTime(GSM_CAST(&now));
	#elif defined(USE_CLOCK)
	struct timespec out;
	clock_gettime(CLOCK_MONOTONIC_RAW, &out);
	now = ((unsigned long long)out.tv_sec)*1000000000LL + out.tv_nsec;
	#else
	struct timeval out;
	gettimeofday(&out, NULL);
	now = ((unsigned long long)out.tv_sec)*1000000LL + out.tv_usec;
	#endif
	return now;
}
// -------------------------------------

KHASH_MAP_IMPL_INT(queries, glquery_t *);

static glquery_t* find_query(GLuint querie) {
    khint_t k;
    khash_t(queries) *list = glstate->queries.querylist;
    k = kh_get(queries, list, querie);
    if (k != kh_end(list)){
        return kh_value(list, k);
    }
    return NULL;
}

// Helper untuk mencari query berdasarkan target
static glquery_t* find_query_target(GLenum target) {
    khash_t(queries) *list = glstate->queries.querylist;
	glquery_t *q;
    kh_foreach_value(list, q,
		if(q->active && q->target==target)
			return q;
	);
    return NULL;
}

// 1. GEN QUERIES
void APIENTRY_GL4ES gl4es_glGenQueries(GLsizei n, GLuint * ids) {
    // FLUSH REMOVED
    if (n<1) return;
    for (int i=0; i<n; i++) {
        ids[i] = ++glstate->queries.last_query;
        int ret;
        khint_t k = kh_put(queries, glstate->queries.querylist, ids[i], &ret);
        glquery_t *query = kh_value(glstate->queries.querylist, k) = calloc(1, sizeof(glquery_t));
        query->id = ids[i];
        
        // [OPTIMASI CACHE]
        // Set nilai awal 'num' jadi 1 (Visible).
        // Supaya saat pertama kali render, chunk tidak hilang (flicker).
        query->num = 1; 
    }
    noerrorShim();
}

// 2. IS QUERY
GLboolean APIENTRY_GL4ES gl4es_glIsQuery(GLuint id) {
    glquery_t *query = find_query(id);
    return (query) ? GL_TRUE : GL_FALSE;
}

// 3. DELETE QUERIES
void APIENTRY_GL4ES gl4es_glDeleteQueries(GLsizei n, const GLuint* ids) {
    // FLUSH REMOVED
    for(int i=0; i<n; ++i) {
        khint_t k = kh_get(queries, glstate->queries.querylist, ids[i]);
        if (k != kh_end(glstate->queries.querylist)){
            glquery_t *s = kh_value(glstate->queries.querylist, k);
            // Hapus di hardware juga untuk mencegah memory leak VRAM
            if(s->real_id) {
                LOAD_GLES(glDeleteQueries);
                gles_glDeleteQueries(1, &s->real_id);
            }
            kh_del(queries, glstate->queries.querylist, k);
            free(s);
        }
    }
}

// 4. BEGIN QUERY
void APIENTRY_GL4ES gl4es_glBeginQuery(GLenum target, GLuint id) {
    // FLUSH REMOVED
    glquery_t *query = find_query(id);
    if(!query) {
        // Fallback create if not exists
        khint_t k; int ret;
        k = kh_put(queries, glstate->queries.querylist, id, &ret);
        query = kh_value(glstate->queries.querylist, k) = calloc(1, sizeof(glquery_t));
        query->id = id;
        query->num = 1; // Default Visible
    }
    
    if(query->active || find_query_target(target)) {
        errorShim(GL_INVALID_OPERATION);
        return;
    }
    
    // --- HARDWARE INIT ---
    if(!query->real_id) {
        LOAD_GLES(glGenQueries);
        gles_glGenQueries(1, &query->real_id);
    }
    
    LOAD_GLES(glBeginQuery);
    
    // [FIX ENUM] Terjemahkan GL_SAMPLES_PASSED -> GL_ANY_SAMPLES_PASSED
    GLenum driver_target = target;
    if (target == GL_SAMPLES_PASSED) {
        driver_target = GL_ANY_SAMPLES_PASSED;
    }
    gles_glBeginQuery(driver_target, query->real_id);
    // ---------------------

    query->target = target;
    query->active = 1;
    // PENTING: Jangan reset query->num jadi 0 di sini!
    // Kita butuh nilai 'num' sebagai cache hasil frame lalu.
    
    noerrorShim();
}

// 5. END QUERY
void APIENTRY_GL4ES gl4es_glEndQuery(GLenum target) {
    // FLUSH REMOVED
    glquery_t *query = find_query_target(target);
    if(!query) {
        errorShim(GL_INVALID_OPERATION);
        return;
    }
    
    LOAD_GLES(glEndQuery);
    
    // [FIX ENUM]
    GLenum driver_target = target;
    if (target == GL_SAMPLES_PASSED) {
        driver_target = GL_ANY_SAMPLES_PASSED;
    }
    gles_glEndQuery(driver_target);
    
    query->active = 0;
    noerrorShim();
}

// 6. GET QUERY OBJECT (LOGIKA UTAMA)
void APIENTRY_GL4ES gl4es_glGetQueryObjectuiv(GLuint id, GLenum pname, GLuint* params) {
    // FLUSH REMOVED (Sangat penting agar PowerVR tidak macet)
    
    glquery_t *query = find_query(id);
    if(!query) {
        errorShim(GL_INVALID_OPERATION);
        return;
    }

    // Default return value (Safety)
    *params = 0;

    if(query->real_id) {
        LOAD_GLES(glGetQueryObjectuiv);
        
        if (pname == GL_QUERY_RESULT) {
            GLuint available = GL_FALSE;
            
            // 1. Cek dulu, apakah GPU sudah selesai?
            gles_glGetQueryObjectuiv(query->real_id, GL_QUERY_RESULT_AVAILABLE, &available);
            
            if (available == GL_TRUE) {
                // 2A. Jika SIAP: Ambil hasil asli
                GLuint res = 0;
                gles_glGetQueryObjectuiv(query->real_id, GL_QUERY_RESULT, &res);
                
                // Simpan ke cache (query->num) untuk masa depan
                query->num = res; 
                *params = res;
            } else {
                // 2B. Jika BELUM SIAP (Stalling):
                // JANGAN TUNGGU. Kembalikan hasil TERAKHIR yang kita tahu (Cache).
                // Ini membuat frame rate tetap tinggi.
                *params = query->num; 
            }
        } else {
            // Untuk pname lain (misal AVAILABLE), teruskan saja
            gles_glGetQueryObjectuiv(query->real_id, pname, params);
        }
    } else {
        // Fallback jika hardware tidak init
        *params = query->num;
    }
    noerrorShim();
}

// Wrappers Lainnya (Dummy / Standard)
void APIENTRY_GL4ES gl4es_glGetQueryiv(GLenum target, GLenum pname, GLint* params) {
    *params = 0;
}
void APIENTRY_GL4ES gl4es_glGetQueryObjectiv(GLuint id, GLenum pname, GLint* params) {
    gl4es_glGetQueryObjectuiv(id, pname, (GLuint*)params);
}
void APIENTRY_GL4ES gl4es_glQueryCounter(GLuint id, GLenum target) {}
void APIENTRY_GL4ES gl4es_glGetQueryObjecti64v(GLuint id, GLenum pname, GLint64 * params) {
    GLuint p;
    gl4es_glGetQueryObjectuiv(id, pname, &p);
    *params = p;
}
void APIENTRY_GL4ES gl4es_glGetQueryObjectui64v(GLuint id, GLenum pname, GLuint64 * params) {
    GLuint p;
    gl4es_glGetQueryObjectuiv(id, pname, &p);
    *params = p;
}

// EXPORT ALIAS (Wajib ada)
AliasExport(void,glGenQueries,,(GLsizei n, GLuint * ids));
AliasExport(GLboolean,glIsQuery,,(GLuint id));
AliasExport(void,glDeleteQueries,,(GLsizei n, const GLuint* ids));
AliasExport(void,glBeginQuery,,(GLenum target, GLuint id));
AliasExport(void,glEndQuery,,(GLenum target));
AliasExport(void,glGetQueryiv,,(GLenum target, GLenum pname, GLint* params));
AliasExport(void,glGetQueryObjectiv,,(GLuint id, GLenum pname, GLint* params));
AliasExport(void,glGetQueryObjectuiv,,(GLuint id, GLenum pname, GLuint* params));
AliasExport(void,glQueryCounter,,(GLuint id, GLenum target));
AliasExport(void,glGetQueryObjecti64v,,(GLuint id, GLenum pname, GLint64 * params));
AliasExport(void,glGetQueryObjectui64v,,(GLuint id, GLenum pname, GLuint64 * params));

// ARB wrapper
AliasExport(void,glGenQueries,ARB,(GLsizei n, GLuint * ids));
AliasExport(GLboolean,glIsQuery,ARB,(GLuint id));
AliasExport(void,glDeleteQueries,ARB,(GLsizei n, const GLuint* ids));
AliasExport(void,glBeginQuery,ARB,(GLenum target, GLuint id));
AliasExport(void,glEndQuery,ARB,(GLenum target));
AliasExport(void,glGetQueryiv,ARB,(GLenum target, GLenum pname, GLint* params));
AliasExport(void,glGetQueryObjectiv,ARB,(GLuint id, GLenum pname, GLint* params));
AliasExport(void,glGetQueryObjectuiv,ARB,(GLuint id, GLenum pname, GLuint* params));
AliasExport(void,glQueryCounter,ARB,(GLuint id, GLenum target));