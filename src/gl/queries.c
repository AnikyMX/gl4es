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

// --- [CRITICAL FIX] KEMBALIKAN FUNGSI GET_CLOCK ---
// Fungsi ini dibutuhkan oleh glstate.c. Jangan dihapus!
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
// --------------------------------------------------

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

// 1. GEN QUERIES
void APIENTRY_GL4ES gl4es_glGenQueries(GLsizei n, GLuint * ids) {
    // NO FLUSH
    if (n<1) return;
    for (int i=0; i<n; i++) {
        ids[i] = ++glstate->queries.last_query;
        int ret;
        khint_t k = kh_put(queries, glstate->queries.querylist, ids[i], &ret);
        glquery_t *query = kh_value(glstate->queries.querylist, k) = calloc(1, sizeof(glquery_t));
        query->id = ids[i];
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
    // NO FLUSH
    for(int i=0; i<n; ++i) {
        khint_t k = kh_get(queries, glstate->queries.querylist, ids[i]);
        if (k != kh_end(glstate->queries.querylist)){
            glquery_t *s = kh_value(glstate->queries.querylist, k);
            kh_del(queries, glstate->queries.querylist, k);
            free(s);
        }
    }
}

// 4. BEGIN QUERY
void APIENTRY_GL4ES gl4es_glBeginQuery(GLenum target, GLuint id) {
    // NO FLUSH
    glquery_t *query = find_query(id);
    if(!query) return;
    
    query->target = target;
    query->active = 1;
    // Kita simpan waktu start (walaupun fake, biar rapi)
    query->start = get_clock(); 
    noerrorShim();
}

// 5. END QUERY
void APIENTRY_GL4ES gl4es_glEndQuery(GLenum target) {
    // NO FLUSH
    glquery_t *q;
    kh_foreach_value(glstate->queries.querylist, q,
        if(q->active && q->target==target) {
            q->active = 0;
            break;
        }
    );
    noerrorShim();
}

// 6. GET QUERY OBJECT (THE FAKE ANSWER)
void APIENTRY_GL4ES gl4es_glGetQueryObjectuiv(GLuint id, GLenum pname, GLuint* params) {
    // NO FLUSH
    if (pname == GL_QUERY_RESULT_AVAILABLE) {
        *params = GL_TRUE; // Selalu SIAP
    } else {
        *params = 100; // Selalu TERLIHAT (Non-Zero)
    }
    noerrorShim();
}

// Wrappers Lainnya
void APIENTRY_GL4ES gl4es_glGetQueryiv(GLenum target, GLenum pname, GLint* params) {
    *params = 0;
}
void APIENTRY_GL4ES gl4es_glGetQueryObjectiv(GLuint id, GLenum pname, GLint* params) {
    if (pname == GL_QUERY_RESULT_AVAILABLE) *params = GL_TRUE;
    else *params = 100;
}
void APIENTRY_GL4ES gl4es_glQueryCounter(GLuint id, GLenum target) {}
void APIENTRY_GL4ES gl4es_glGetQueryObjecti64v(GLuint id, GLenum pname, GLint64 * params) {
    if (pname == GL_QUERY_RESULT_AVAILABLE) *params = GL_TRUE;
    else *params = 100;
}
void APIENTRY_GL4ES gl4es_glGetQueryObjectui64v(GLuint id, GLenum pname, GLuint64 * params) {
    if (pname == GL_QUERY_RESULT_AVAILABLE) *params = GL_TRUE;
    else *params = 100;
}

// Direct wrapper
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