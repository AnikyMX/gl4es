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

// --- [FIX BUILD] KAMUS DEFINISI (VERSI REVISI) ---
// Kita hapus glFlush_PTR dari sini karena sudah ada di gles.h
#ifndef glGenQueries_PTR
typedef void (APIENTRY_GLES *glGenQueries_PTR)(GLsizei n, GLuint * ids);
typedef void (APIENTRY_GLES *glDeleteQueries_PTR)(GLsizei n, const GLuint * ids);
typedef void (APIENTRY_GLES *glIsQuery_PTR)(GLuint id);
typedef void (APIENTRY_GLES *glBeginQuery_PTR)(GLenum target, GLuint id);
typedef void (APIENTRY_GLES *glEndQuery_PTR)(GLenum target);
typedef void (APIENTRY_GLES *glGetQueryiv_PTR)(GLenum target, GLenum pname, GLint * params);
typedef void (APIENTRY_GLES *glGetQueryObjectiv_PTR)(GLuint id, GLenum pname, GLint * params);
typedef void (APIENTRY_GLES *glGetQueryObjectuiv_PTR)(GLuint id, GLenum pname, GLuint * params);
#endif
// --------------------------------------------------

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
// -------------------------

// --- [FIX LINKER] FUNGSI GET_CLOCK ---
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
    // NO FLUSH (Aman)
    if (n<1) return;
    for (int i=0; i<n; i++) {
        ids[i] = ++glstate->queries.last_query;
        int ret;
        khint_t k = kh_put(queries, glstate->queries.querylist, ids[i], &ret);
        glquery_t *query = kh_value(glstate->queries.querylist, k) = calloc(1, sizeof(glquery_t));
        query->id = ids[i];
        query->num = 1; // Default Visible
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
    // NO FLUSH (Aman)
    for(int i=0; i<n; ++i) {
        khint_t k = kh_get(queries, glstate->queries.querylist, ids[i]);
        if (k != kh_end(glstate->queries.querylist)){
            glquery_t *s = kh_value(glstate->queries.querylist, k);
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
    // NO FLUSH (Kita hapus rem tangan di sini)
    
    glquery_t *query = find_query(id);
    if(!query) {
        khint_t k; int ret;
        k = kh_put(queries, glstate->queries.querylist, id, &ret);
        query = kh_value(glstate->queries.querylist, k) = calloc(1, sizeof(glquery_t));
        query->id = id;
        query->num = 1; 
    }
    
    if(query->active || find_query_target(target)) {
        errorShim(GL_INVALID_OPERATION);
        return;
    }
    
    // Init Hardware
    if(!query->real_id) {
        LOAD_GLES(glGenQueries);
        gles_glGenQueries(1, &query->real_id);
    }
    
    LOAD_GLES(glBeginQuery);
    
    // Translasi ke Boolean (Support PowerVR)
    GLenum driver_target = target;
    if (target == GL_SAMPLES_PASSED) {
        driver_target = GL_ANY_SAMPLES_PASSED;
    }
    gles_glBeginQuery(driver_target, query->real_id);

    query->target = target;
    query->active = 1;
    noerrorShim();
}

// 5. END QUERY (INILAH KUNCINYA!)
void APIENTRY_GL4ES gl4es_glEndQuery(GLenum target) {
    // NO FLUSH_BEGINEND (Software flush dimatikan)
    
    glquery_t *query = find_query_target(target);
    if(!query) {
        errorShim(GL_INVALID_OPERATION);
        return;
    }
    
    LOAD_GLES(glEndQuery);
    GLenum driver_target = target;
    if (target == GL_SAMPLES_PASSED) {
        driver_target = GL_ANY_SAMPLES_PASSED;
    }
    gles_glEndQuery(driver_target);
    
    // [OPTIMASI POWERVR]
    // Kita "Dorong" perintah ke GPU supaya urutannya benar.
    // Kita pakai glFlush() yang sudah ada di library (tidak perlu typedef manual).
    LOAD_GLES(glFlush);
    gles_glFlush(); 
    
    query->active = 0;
    noerrorShim();
}

// 6. GET QUERY OBJECT (ASYNC HISTORY)
void APIENTRY_GL4ES gl4es_glGetQueryObjectuiv(GLuint id, GLenum pname, GLuint* params) {
    // NO FLUSH
    
    glquery_t *query = find_query(id);
    if(!query) {
        errorShim(GL_INVALID_OPERATION);
        return;
    }

    *params = query->num; // Default safety

    if(query->real_id) {
        LOAD_GLES(glGetQueryObjectuiv);
        
        if (pname == GL_QUERY_RESULT) {
            GLuint available = GL_FALSE;
            
            // Tanya GPU: "Sudah siap?"
            gles_glGetQueryObjectuiv(query->real_id, GL_QUERY_RESULT_AVAILABLE, &available);
            
            if (available == GL_TRUE) {
                // SIAP: Ambil hasil
                GLuint res = 0;
                gles_glGetQueryObjectuiv(query->real_id, GL_QUERY_RESULT, &res);
                query->num = res; 
                *params = res;
            } else {
                // BELUM SIAP: Jangan tunggu!
                // Kembalikan hasil terakhir (History).
                *params = query->num; 
            }
        } else {
            gles_glGetQueryObjectuiv(query->real_id, pname, params);
        }
    }
    noerrorShim();
}

// WRAPPERS LAIN
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

// EXPORT ALIAS
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