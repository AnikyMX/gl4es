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
#include "logs.h"  // ← TAMBAH INI untuk LOGD

#ifdef _WIN32
#ifdef _WINBASE_
#define GSM_CAST(c) ((LPFILETIME)c)
#else
__declspec(dllimport)
void __stdcall GetSystemTimeAsFileTime(unsigned __int64*);
#define GSM_CAST(c) ((__int64*)c)
#endif
#endif

// OpenGL ES extension constants
#ifndef GL_ANY_SAMPLES_PASSED_EXT
#define GL_ANY_SAMPLES_PASSED_EXT 0x8C2F
#endif
#ifndef GL_ANY_SAMPLES_PASSED_CONSERVATIVE_EXT
#define GL_ANY_SAMPLES_PASSED_CONSERVATIVE_EXT 0x8D6A
#endif
#ifndef GL_QUERY_RESULT_EXT
#define GL_QUERY_RESULT_EXT 0x8866
#endif
#ifndef GL_QUERY_RESULT_AVAILABLE_EXT
#define GL_QUERY_RESULT_AVAILABLE_EXT 0x8867
#endif
#ifndef GL_CURRENT_QUERY_EXT
#define GL_CURRENT_QUERY_EXT 0x8865
#endif

KHASH_MAP_IMPL_INT(queries, glquery_t *);

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

void del_querie(GLuint querie) {
    khint_t k;
    khash_t(queries) *list = glstate->queries.querylist;
    k = kh_get(queries, list, querie);
    glquery_t* s = NULL;
    if (k != kh_end(list)){
        s = kh_value(list, k);
        kh_del(queries, list, k);
    }
    if(s) {
        if(s->use_hardware && s->gles_id) {
            LOAD_GLES_EXT(glDeleteQueriesEXT);
            if(gles_glDeleteQueriesEXT) {
                SHUT_LOGD("[QUERY] Deleting hardware query ID=%u, GLES_ID=%u\n", querie, s->gles_id);
                gles_glDeleteQueriesEXT(1, &s->gles_id);
            }
        }
        free(s);
    }
}

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

void APIENTRY_GL4ES gl4es_glGenQueries(GLsizei n, GLuint * ids) {
    FLUSH_BEGINEND;
    noerrorShim();
    if (n<1) {
        errorShim(GL_INVALID_VALUE);
        return;
    }
    for (int i=0; i<n; i++) {
        ids[i] = new_query(++glstate->queries.last_query);
    }
    SHUT_LOGD("[QUERY] glGenQueries(n=%d) -> IDs: %u...\n", n, ids[0]);
}

GLboolean APIENTRY_GL4ES gl4es_glIsQuery(GLuint id) {
    if(glstate->list.compiling) {errorShim(GL_INVALID_OPERATION); return GL_FALSE;}
    FLUSH_BEGINEND;
    glquery_t *querie = find_query(id);
    return querie ? GL_TRUE : GL_FALSE;
}

void APIENTRY_GL4ES gl4es_glDeleteQueries(GLsizei n, const GLuint* ids) {
    FLUSH_BEGINEND;
    if(n<0) {
        errorShim(GL_INVALID_VALUE);
        return;
    }
    noerrorShim();
    if(!n)
        return;
    SHUT_LOGD("[QUERY] glDeleteQueries(n=%d)\n", n);
    for(int i=0; i<n; ++i)
        del_querie(ids[i]);
}

void APIENTRY_GL4ES gl4es_glBeginQuery(GLenum target, GLuint id) {
    FLUSH_BEGINEND;
    
    SHUT_LOGD("[QUERY] glBeginQuery(target=0x%04X, id=%u)\n", target, id);
    
    glquery_t *query = find_query(id);
    if(!query) {
        khint_t k;
        int ret;
        khash_t(queries) *list = glstate->queries.querylist;
        k = kh_put(queries, list, id, &ret);
        query = kh_value(list, k) = calloc(1, sizeof(glquery_t));
        query->id = id;
        query->gles_id = 0;
        query->use_hardware = 0;
        SHUT_LOGD("[QUERY] Created new query object id=%u\n", id);
    }
    
    if(query->active || find_query_target(target)) {
        SHUT_LOGD("[QUERY] ERROR: Query already active or target in use\n");
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
            SHUT_LOGD("[QUERY] ERROR: Invalid target 0x%04X\n", target);
            errorShim(GL_INVALID_ENUM);
            return;
    }
    
    query->target = target;
    query->num = 0;
    query->active = 1;
    
    // Hardware path
    if(hardext.occlusionquery && 
       (target == GL_SAMPLES_PASSED || 
        target == GL_ANY_SAMPLES_PASSED || 
        target == GL_ANY_SAMPLES_PASSED_CONSERVATIVE)) {
        
        SHUT_LOGD("[QUERY] Attempting hardware path...\n");
        SHUT_LOGD("[QUERY] hardext.occlusionquery = %d\n", hardext.occlusionquery);
        
        LOAD_GLES_EXT(glGenQueriesEXT);
        LOAD_GLES_EXT(glBeginQueryEXT);
        
        SHUT_LOGD("[QUERY] glGenQueriesEXT ptr = %p\n", gles_glGenQueriesEXT);
        SHUT_LOGD("[QUERY] glBeginQueryEXT ptr = %p\n", gles_glBeginQueryEXT);
        
        if(gles_glGenQueriesEXT && gles_glBeginQueryEXT) {
            if(!query->gles_id) {
                gles_glGenQueriesEXT(1, &query->gles_id);
                SHUT_LOGD("[QUERY] Created GLES query object: GLES_ID=%u\n", query->gles_id);
            }
            
            GLenum gles_target;
            if(target == GL_SAMPLES_PASSED) {
                gles_target = GL_ANY_SAMPLES_PASSED_EXT;
                SHUT_LOGD("[QUERY] Mapping GL_SAMPLES_PASSED -> GL_ANY_SAMPLES_PASSED_EXT\n");
            } else {
                gles_target = target;
            }
            
            SHUT_LOGD("[QUERY] Calling gles_glBeginQueryEXT(0x%04X, %u)\n", gles_target, query->gles_id);
            gles_glBeginQueryEXT(gles_target, query->gles_id);
            
            // Check for errors
            LOAD_GLES(glGetError);
            GLenum err = gles_glGetError();
            if(err != GL_NO_ERROR) {
                SHUT_LOGD("[QUERY] ERROR from GLES backend: 0x%04X\n", err);
                query->use_hardware = 0;
                query->start = get_clock() - glstate->queries.start;
            } else {
                query->use_hardware = 1;
                SHUT_LOGD("[QUERY] Hardware query started successfully!\n");
            }
            
            noerrorShim();
            return;
        } else {
            SHUT_LOGD("[QUERY] GLES functions not available, falling back to software\n");
        }
    } else {
        SHUT_LOGD("[QUERY] Software path (hardext.occlusionquery=%d, target=0x%04X)\n", 
             hardext.occlusionquery, target);
    }
    
    // Software fallback
    query->start = get_clock() - glstate->queries.start;
    query->use_hardware = 0;
    SHUT_LOGD("[QUERY] Using software stub\n");
    noerrorShim();
}

void APIENTRY_GL4ES gl4es_glEndQuery(GLenum target) {
    FLUSH_BEGINEND;
    
    SHUT_LOGD("[QUERY] glEndQuery(target=0x%04X)\n", target);
    
    glquery_t *query = find_query_target(target);
    if(!query) {
        SHUT_LOGD("[QUERY] ERROR: No active query for target 0x%04X\n", target);
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
    
    query->active = 0;
    
    if(query->use_hardware) {
        SHUT_LOGD("[QUERY] Ending hardware query...\n");
        LOAD_GLES_EXT(glEndQueryEXT);
        
        if(gles_glEndQueryEXT) {
            GLenum gles_target;
            if(target == GL_SAMPLES_PASSED) {
                gles_target = GL_ANY_SAMPLES_PASSED_EXT;
            } else {
                gles_target = target;
            }
            
            gles_glEndQueryEXT(gles_target);
            SHUT_LOGD("[QUERY] Hardware query ended\n");
            noerrorShim();
            return;
        }
    }
    
    query->start = (get_clock() - glstate->queries.start) - query->start;
    SHUT_LOGD("[QUERY] Software query ended\n");
    noerrorShim();
}

void APIENTRY_GL4ES gl4es_glGetQueryiv(GLenum target, GLenum pname, GLint* params) {
    FLUSH_BEGINEND;
    
    SHUT_LOGD("[QUERY] glGetQueryiv(target=0x%04X, pname=0x%04X)\n", target, pname);
    
    noerrorShim();
    
    switch (pname) {
        case GL_CURRENT_QUERY: {
            glquery_t *q = find_query_target(target);
            *params = q ? q->id : 0;
            SHUT_LOGD("[QUERY] GL_CURRENT_QUERY = %d\n", *params);
            break;
        }
        
        case GL_QUERY_COUNTER_BITS: {
            // GL_QUERY_COUNTER_BITS does NOT require an active query
            // This is the critical detection test that OptiFine performs!
            switch(target) {
                case GL_TIME_ELAPSED:
                case GL_TIMESTAMP:
                    *params = 32;
                    break;
                    
                case GL_SAMPLES_PASSED:
                case GL_ANY_SAMPLES_PASSED:
                case GL_ANY_SAMPLES_PASSED_CONSERVATIVE:
                    if(hardext.occlusionquery) {
                        *params = 32;  // Report 32 bits (compatible with desktop GL)
                    } else {
                        *params = 0;   // No hardware support
                    }
                    break;
                    
                case GL_PRIMITIVES_GENERATED:
                case GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN:
                    *params = 0;
                    break;
                    
                default:
                    errorShim(GL_INVALID_ENUM);
                    return;
            }
            SHUT_LOGD("[QUERY] GL_QUERY_COUNTER_BITS for target 0x%04X = %d (hardext.occlusionquery=%d)\n", 
                 target, *params, hardext.occlusionquery);
            break;
        }
        
        default:
            SHUT_LOGD("[QUERY] ERROR: Invalid pname 0x%04X\n", pname);
            errorShim(GL_INVALID_ENUM);
            return;
    }
}

void APIENTRY_GL4ES gl4es_glGetQueryObjectiv(GLuint id, GLenum pname, GLint* params) {
    FLUSH_BEGINEND;
    
    SHUT_LOGD("[QUERY] glGetQueryObjectiv(id=%u, pname=0x%04X)\n", id, pname);

    glquery_t *query = find_query(id);
    if(!query) {
        SHUT_LOGD("[QUERY] ERROR: Query %u not found\n", id);
        errorShim(GL_INVALID_OPERATION);
        return;
    }
    
    switch (pname) {
        case GL_QUERY_RESULT_AVAILABLE:
            if(query->use_hardware) {
                LOAD_GLES_EXT(glGetQueryObjectuivEXT);
                if(gles_glGetQueryObjectuivEXT) {
                    GLuint available;
                    gles_glGetQueryObjectuivEXT(query->gles_id, GL_QUERY_RESULT_AVAILABLE_EXT, &available);
                    *params = (GLint)available;
                    SHUT_LOGD("[QUERY] Hardware GL_QUERY_RESULT_AVAILABLE = %d\n", *params);
                } else {
                    *params = GL_TRUE;
                    SHUT_LOGD("[QUERY] Software GL_QUERY_RESULT_AVAILABLE = TRUE\n");
                }
            } else {
                *params = GL_TRUE;
                SHUT_LOGD("[QUERY] Stub GL_QUERY_RESULT_AVAILABLE = TRUE\n");
            }
            break;
            
        case GL_QUERY_RESULT_NO_WAIT:
        case GL_QUERY_RESULT:
            if(query->use_hardware) {
                LOAD_GLES_EXT(glGetQueryObjectuivEXT);
                if(gles_glGetQueryObjectuivEXT) {
                    GLuint gles_result;
                    gles_glGetQueryObjectuivEXT(query->gles_id, GL_QUERY_RESULT_EXT, &gles_result);
                    *params = gles_result ? 1 : 0;
                    SHUT_LOGD("[QUERY] Hardware GL_QUERY_RESULT = %d (raw GLES=%u)\n", *params, gles_result);
                } else {
                    *params = 0;
                    SHUT_LOGD("[QUERY] Software GL_QUERY_RESULT = 0\n");
                }
            } else {
                *params = (query->target==GL_TIME_ELAPSED) ? (GLint)query->start : query->num;
                SHUT_LOGD("[QUERY] Stub GL_QUERY_RESULT = %d\n", *params);
            }
            break;
            
        default:
            errorShim(GL_INVALID_ENUM);
            return;
    }
    noerrorShim();
}

void APIENTRY_GL4ES gl4es_glGetQueryObjectuiv(GLuint id, GLenum pname, GLuint* params) {
    FLUSH_BEGINEND;
    
    SHUT_LOGD("[QUERY] glGetQueryObjectuiv(id=%u, pname=0x%04X)\n", id, pname);

    glquery_t *query = find_query(id);
    if(!query) {
        SHUT_LOGD("[QUERY] ERROR: Query %u not found\n", id);
        errorShim(GL_INVALID_OPERATION);
        return;
    }

    switch (pname) {
        case GL_QUERY_RESULT_AVAILABLE:
            if(query->use_hardware) {
                LOAD_GLES_EXT(glGetQueryObjectuivEXT);
                if(gles_glGetQueryObjectuivEXT) {
                    gles_glGetQueryObjectuivEXT(query->gles_id, GL_QUERY_RESULT_AVAILABLE_EXT, params);
                    SHUT_LOGD("[QUERY] Hardware GL_QUERY_RESULT_AVAILABLE = %u\n", *params);
                } else {
                    *params = GL_TRUE;
                    SHUT_LOGD("[QUERY] Software GL_QUERY_RESULT_AVAILABLE = TRUE\n");
                }
            } else {
                *params = GL_TRUE;
                SHUT_LOGD("[QUERY] Stub GL_QUERY_RESULT_AVAILABLE = TRUE\n");
            }
            break;
            
        case GL_QUERY_RESULT_NO_WAIT:
        case GL_QUERY_RESULT:
            if(query->use_hardware) {
                LOAD_GLES_EXT(glGetQueryObjectuivEXT);
                if(gles_glGetQueryObjectuivEXT) {
                    GLuint gles_result;
                    gles_glGetQueryObjectuivEXT(query->gles_id, GL_QUERY_RESULT_EXT, &gles_result);
                    *params = gles_result ? 1 : 0;
                    SHUT_LOGD("[QUERY] Hardware GL_QUERY_RESULT = %u (raw GLES=%u)\n", *params, gles_result);
                } else {
                    *params = 0;
                    SHUT_LOGD("[QUERY] Software GL_QUERY_RESULT = 0\n");
                }
            } else {
                *params = (query->target==GL_TIME_ELAPSED) ? (GLuint)query->start : (GLuint)query->num;
                SHUT_LOGD("[QUERY] Stub GL_QUERY_RESULT = %u\n", *params);
            }
            break;
            
        default:
            errorShim(GL_INVALID_ENUM);
            return;
    }
    noerrorShim();
}

void APIENTRY_GL4ES gl4es_glQueryCounter(GLuint id, GLenum target)
{
    FLUSH_BEGINEND;

    glquery_t *query = find_query(id);
    if(!query) {
        errorShim(GL_INVALID_OPERATION);
        return;
    }
    if(query->active) {
        errorShim(GL_INVALID_OPERATION);
        return;
    }
    if(target!=GL_TIMESTAMP) {
        errorShim(GL_INVALID_ENUM);
        return;
    }
    query->target = target;
    // should finish first?
    query->start = get_clock() - glstate->queries.start;
    noerrorShim();
}

void APIENTRY_GL4ES gl4es_glGetQueryObjecti64v(GLuint id, GLenum pname, GLint64 * params)
{
    FLUSH_BEGINEND;

    glquery_t *query = find_query(id);
    if(!query) {
        errorShim(GL_INVALID_OPERATION);
        return;
    }

    switch (pname) {
        case GL_QUERY_RESULT_AVAILABLE:
            if(query->use_hardware) {
                LOAD_GLES_EXT(glGetQueryObjectuivEXT);
                if(gles_glGetQueryObjectuivEXT) {
                    GLuint available;
                    gles_glGetQueryObjectuivEXT(query->gles_id, GL_QUERY_RESULT_AVAILABLE_EXT, &available);
                    *params = (GLint64)available;
                } else {
                    *params = GL_TRUE;
                }
            } else {
                *params = GL_TRUE;
            }
            break;
            
        case GL_QUERY_RESULT_NO_WAIT:
        case GL_QUERY_RESULT:
            if(query->use_hardware) {
                LOAD_GLES_EXT(glGetQueryObjectuivEXT);
                if(gles_glGetQueryObjectuivEXT) {
                    GLuint gles_result;
                    gles_glGetQueryObjectuivEXT(query->gles_id, GL_QUERY_RESULT_EXT, &gles_result);
                    *params = gles_result ? 1 : 0;
                } else {
                    *params = 0;
                }
            } else {
                *params = (query->target==GL_TIME_ELAPSED) ? (GLint64)query->start : (GLint64)query->num;
            }
            break;
            
        default:
            errorShim(GL_INVALID_ENUM);
            return;
    }
    noerrorShim();
}

void APIENTRY_GL4ES gl4es_glGetQueryObjectui64v(GLuint id, GLenum pname, GLuint64 * params)
{
    FLUSH_BEGINEND;

    glquery_t *query = find_query(id);
    if(!query) {
        errorShim(GL_INVALID_OPERATION);
        return;
    }

    switch (pname) {
        case GL_QUERY_RESULT_AVAILABLE:
            if(query->use_hardware) {
                LOAD_GLES_EXT(glGetQueryObjectuivEXT);
                if(gles_glGetQueryObjectuivEXT) {
                    GLuint available;
                    gles_glGetQueryObjectuivEXT(query->gles_id, GL_QUERY_RESULT_AVAILABLE_EXT, &available);
                    *params = (GLuint64)available;
                } else {
                    *params = GL_TRUE;
                }
            } else {
                *params = GL_TRUE;
            }
            break;
            
        case GL_QUERY_RESULT_NO_WAIT:
        case GL_QUERY_RESULT:
            if(query->use_hardware) {
                LOAD_GLES_EXT(glGetQueryObjectuivEXT);
                if(gles_glGetQueryObjectuivEXT) {
                    GLuint gles_result;
                    gles_glGetQueryObjectuivEXT(query->gles_id, GL_QUERY_RESULT_EXT, &gles_result);
                    *params = gles_result ? 1 : 0;
                } else {
                    *params = 0;
                }
            } else {
                *params = (query->target==GL_TIME_ELAPSED) ? (GLuint64)query->start : (GLuint64)query->num;
            }
            break;
            
        default:
            errorShim(GL_INVALID_ENUM);
            return;
    }
    noerrorShim();
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