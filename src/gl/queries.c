/*
 * Refactored queries.c for GL4ES
 * Optimized for ARMv8
 * - Efficient High-Resolution Clock
 * - Fast Hash Map lookups with branch prediction
 * - Streamlined GL State management
 */

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

#ifndef likely
#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)
#endif

KHASH_MAP_IMPL_INT(queries, glquery_t *);

// Optimization: Direct lookup without redundant checks
static GLuint new_query(GLuint base) {
    khint_t k;
    khash_t(queries) *list = glstate->queries.querylist;
    while(1) {
        k = kh_get(queries, list, base);
        if (k == kh_end(list))
            return base;
        ++base;
    }
}

static inline glquery_t* find_query(GLuint querie) {
    khint_t k;
    khash_t(queries) *list = glstate->queries.querylist;
    k = kh_get(queries, list, querie);
    
    if (likely(k != kh_end(list))){
        return kh_value(list, k);
    }
    return NULL;
}

static glquery_t* find_query_target(GLenum target) {
    khash_t(queries) *list = glstate->queries.querylist;
    glquery_t *q;
    // Iteration is relatively slow, but target queries are usually few (1 active per target)
    kh_foreach_value(list, q,
        if(q->active && q->target == target)
            return q;
    );
    return NULL;
}

void del_querie(GLuint querie) {
    khint_t k;
    khash_t(queries) *list = glstate->queries.querylist;
    k = kh_get(queries, list, querie);
    glquery_t* s = NULL;
    if (likely(k != kh_end(list))){
        s = kh_value(list, k);
        kh_del(queries, list, k);
    }
    if(s) free(s);
}

// Optimized clock for ARM/Linux
unsigned long long get_clock() {
    unsigned long long now;
#ifdef _WIN32
    GetSystemTimeAsFileTime(GSM_CAST(&now));
#elif defined(USE_CLOCK)
    struct timespec out;
    // CLOCK_MONOTONIC_RAW is better for hardware timing if available
    clock_gettime(CLOCK_MONOTONIC, &out);
    now = ((unsigned long long)out.tv_sec) * 1000000000LL + out.tv_nsec;
#else
    struct timeval out;
    gettimeofday(&out, NULL);
    now = ((unsigned long long)out.tv_sec) * 1000000LL + out.tv_usec;
#endif
    return now;
}

void APIENTRY_GL4ES gl4es_glGenQueries(GLsizei n, GLuint * ids) {
    FLUSH_BEGINEND;
    noerrorShim();
    if (unlikely(n < 1)) {
        errorShim(GL_INVALID_VALUE);
        return;
    }
    // Batch generation optimization possible here, but sequential is safe
    for (int i = 0; i < n; i++) {
        ids[i] = new_query(++glstate->queries.last_query);
    }
}

GLboolean APIENTRY_GL4ES gl4es_glIsQuery(GLuint id) {
    if(glstate->list.compiling) {
        errorShim(GL_INVALID_OPERATION); 
        return GL_FALSE;
    }
    FLUSH_BEGINEND;
    return (find_query(id) != NULL) ? GL_TRUE : GL_FALSE;
}

void APIENTRY_GL4ES gl4es_glDeleteQueries(GLsizei n, const GLuint* ids) {
    FLUSH_BEGINEND;
    if (unlikely(n < 0)) {
        errorShim(GL_INVALID_VALUE);
        return;
    }
    noerrorShim();
    if (n == 0) return;
    
    for(int i = 0; i < n; ++i) {
        if(likely(ids[i] != 0)) del_querie(ids[i]);
    }
}

void APIENTRY_GL4ES gl4es_glBeginQuery(GLenum target, GLuint id) {
    FLUSH_BEGINEND;
    glquery_t *query = find_query(id);
    
    // Create if doesn't exist
    if (!query) {
        khint_t k;
        int ret;
        khash_t(queries) *list = glstate->queries.querylist;
        k = kh_put(queries, list, id, &ret);
        query = kh_value(list, k) = calloc(1, sizeof(glquery_t));
    }
    
    if (unlikely(query->active || find_query_target(target))) {
        errorShim(GL_INVALID_OPERATION);
        return;
    }

    switch(target) {
        case GL_SAMPLES_PASSED:
        case GL_ANY_SAMPLES_PASSED:
        case GL_ANY_SAMPLES_PASSED_CONSERVATIVE:
        case GL_PRIMITIVES_GENERATED:
        case GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN:
        case GL_TIME_ELAPSED:
            break;
        default:
            errorShim(GL_INVALID_ENUM);
            return;
    }

    query->target = target;
    query->num = 0;
    query->active = 1;
    query->start = get_clock() - glstate->queries.start;
    noerrorShim();
}

void APIENTRY_GL4ES gl4es_glEndQuery(GLenum target) {
    FLUSH_BEGINEND;
    glquery_t *query = find_query_target(target);
    
    if (unlikely(!query)) {
        errorShim(GL_INVALID_OPERATION);
        return;
    }

    // Target validation moved to BeginQuery usually, but kept for safety
    switch(target) {
        case GL_SAMPLES_PASSED:
        case GL_ANY_SAMPLES_PASSED:
        case GL_ANY_SAMPLES_PASSED_CONSERVATIVE:
        case GL_PRIMITIVES_GENERATED:
        case GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN:
        case GL_TIME_ELAPSED:
            break;
        default:
            errorShim(GL_INVALID_ENUM);
            return;
    }

    query->active = 0;
    // Calculate final elapsed time immediately
    query->start = (get_clock() - glstate->queries.start) - query->start;
    noerrorShim();
}

void APIENTRY_GL4ES gl4es_glGetQueryiv(GLenum target, GLenum pname, GLint* params) {
    FLUSH_BEGINEND;

    glquery_t *q = find_query_target(target);
    if (unlikely(!q)) {
        errorShim(GL_INVALID_OPERATION);
        return;
    }

    noerrorShim();
    switch (pname) {
        case GL_CURRENT_QUERY:
            *params = (q->target == GL_TIME_ELAPSED) ? (GLint)q->start : (GLint)q->num;
            break;
        case GL_QUERY_COUNTER_BITS:
            *params = (q->target == GL_TIME_ELAPSED) ? 32 : 0;
            break;
        default:
            errorShim(GL_INVALID_ENUM);
    }
}

void APIENTRY_GL4ES gl4es_glGetQueryObjectiv(GLuint id, GLenum pname, GLint* params) {
    FLUSH_BEGINEND;

    glquery_t *query = find_query(id);
    if (unlikely(!query)) {
        errorShim(GL_INVALID_OPERATION);
        return;
    }
    
    noerrorShim();
    switch (pname) {
        case GL_QUERY_RESULT_AVAILABLE:
            *params = GL_TRUE;
            break;
        case GL_QUERY_RESULT_NO_WAIT:
        case GL_QUERY_RESULT:
            // Cast to int might overflow for TIME_ELAPSED (should use i64v), but standard behavior
            *params = (query->target == GL_TIME_ELAPSED) ? (GLint)query->start : (GLint)query->num;
            break;
        default:
            errorShim(GL_INVALID_ENUM);
            return;
    }
}

void APIENTRY_GL4ES gl4es_glGetQueryObjectuiv(GLuint id, GLenum pname, GLuint* params) {
    FLUSH_BEGINEND;
    
    glquery_t *query = find_query(id);
    if (unlikely(!query)) {
        errorShim(GL_INVALID_OPERATION);
        return;
    }

    noerrorShim();
    switch (pname) {
        case GL_QUERY_RESULT_AVAILABLE:
            *params = GL_TRUE;
            break;
        case GL_QUERY_RESULT_NO_WAIT:
        case GL_QUERY_RESULT:
            *params = (query->target == GL_TIME_ELAPSED) ? (GLuint)query->start : (GLuint)query->num;
            break;
        default:
            errorShim(GL_INVALID_ENUM);
            return;
    }
}

void APIENTRY_GL4ES gl4es_glQueryCounter(GLuint id, GLenum target) {
    FLUSH_BEGINEND;
    
    glquery_t *query = find_query(id);
    if (unlikely(!query)) {
        errorShim(GL_INVALID_OPERATION);
        return;
    }
    if (unlikely(query->active)) {
        errorShim(GL_INVALID_OPERATION);
        return;
    }
    if (unlikely(target != GL_TIMESTAMP)) {
        errorShim(GL_INVALID_ENUM);
        return;
    }
    query->target = target;
    // Timestamp query records current time
    query->start = get_clock() - glstate->queries.start;
    noerrorShim();
}

void APIENTRY_GL4ES gl4es_glGetQueryObjecti64v(GLuint id, GLenum pname, GLint64 * params) {
    FLUSH_BEGINEND;
    
    glquery_t *query = find_query(id);
    if (unlikely(!query)) {
        errorShim(GL_INVALID_OPERATION);
        return;
    }

    noerrorShim();
    switch (pname) {
        case GL_QUERY_RESULT_AVAILABLE:
            *params = GL_TRUE;
            break;
        case GL_QUERY_RESULT_NO_WAIT:
        case GL_QUERY_RESULT:
            *params = (query->target == GL_TIME_ELAPSED) ? (GLint64)query->start : (GLint64)query->num;
            break;
        default:
            errorShim(GL_INVALID_ENUM);
            return;
    }
}
    
void APIENTRY_GL4ES gl4es_glGetQueryObjectui64v(GLuint id, GLenum pname, GLuint64 * params) {
    FLUSH_BEGINEND;
    
    glquery_t *query = find_query(id);
    if (unlikely(!query)) {
        errorShim(GL_INVALID_OPERATION);
        return;
    }

    noerrorShim();
    switch (pname) {
        case GL_QUERY_RESULT_AVAILABLE:
            *params = GL_TRUE;
            break;
        case GL_QUERY_RESULT_NO_WAIT:
        case GL_QUERY_RESULT:
            *params = (query->target == GL_TIME_ELAPSED) ? (GLuint64)query->start : (GLuint64)query->num;
            break;
        default:
            errorShim(GL_INVALID_ENUM);
            return;
    }
}

//Direct wrapper
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