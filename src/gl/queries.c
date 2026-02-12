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

// OpenGL ES extension constants (may not be in headers)
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
        // Delete hardware query object if it exists
        if(s->use_hardware && s->gles_id) {
            LOAD_GLES_EXT(glDeleteQueriesEXT);
            if(gles_glDeleteQueriesEXT) {
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
}

GLboolean APIENTRY_GL4ES gl4es_glIsQuery(GLuint id) {
    if(glstate->list.compiling) {errorShim(GL_INVALID_OPERATION); return GL_FALSE;}
    FLUSH_BEGINEND;
    glquery_t *querie = find_query(id);
    if(querie)
        return GL_TRUE;
    return GL_FALSE;
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
    for(int i=0; i<n; ++i)
        del_querie(ids[i]);
}

void APIENTRY_GL4ES gl4es_glBeginQuery(GLenum target, GLuint id) {
    FLUSH_BEGINEND;
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
    }
    if(query->active || find_query_target(target)) {
        errorShim(GL_INVALID_OPERATION);
        return;
    }
    
    // Validate target
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
    
    // ========== HARDWARE ACCELERATION PATH ==========
    // Use GL_EXT_occlusion_query_boolean for occlusion queries if available
    if(hardext.occlusionquery && 
       (target == GL_SAMPLES_PASSED || 
        target == GL_ANY_SAMPLES_PASSED || 
        target == GL_ANY_SAMPLES_PASSED_CONSERVATIVE)) {
        
        LOAD_GLES_EXT(glGenQueriesEXT);
        LOAD_GLES_EXT(glBeginQueryEXT);
        
        if(gles_glGenQueriesEXT && gles_glBeginQueryEXT) {
            // Create GLES query object if needed
            if(!query->gles_id) {
                gles_glGenQueriesEXT(1, &query->gles_id);
            }
            
            // Map OpenGL targets to OpenGL ES targets
            GLenum gles_target;
            if(target == GL_SAMPLES_PASSED) {
                // GL_SAMPLES_PASSED → GL_ANY_SAMPLES_PASSED_EXT
                // Note: This changes semantics from count to boolean,
                // but it's sufficient for visibility testing (Minecraft use case)
                gles_target = GL_ANY_SAMPLES_PASSED_EXT;
            } else {
                gles_target = target;
            }
            
            gles_glBeginQueryEXT(gles_target, query->gles_id);
            query->use_hardware = 1;
            noerrorShim();
            return;
        }
    }
    
    // ========== SOFTWARE FALLBACK PATH ==========
    query->start = get_clock() - glstate->queries.start;
    query->use_hardware = 0;
    noerrorShim();
}

void APIENTRY_GL4ES gl4es_glEndQuery(GLenum target) {
    FLUSH_BEGINEND;
    glquery_t *query = find_query_target(target);
    if(!query) {
        errorShim(GL_INVALID_OPERATION);
        return;
    }
    
    // Validate target
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
    
    // ========== HARDWARE ACCELERATION PATH ==========
    if(query->use_hardware) {
        LOAD_GLES_EXT(glEndQueryEXT);
        
        if(gles_glEndQueryEXT) {
            GLenum gles_target;
            if(target == GL_SAMPLES_PASSED) {
                gles_target = GL_ANY_SAMPLES_PASSED_EXT;
            } else {
                gles_target = target;
            }
            
            gles_glEndQueryEXT(gles_target);
            noerrorShim();
            return;
        }
    }
    
    // ========== SOFTWARE FALLBACK PATH ==========
    query->start = (get_clock() - glstate->queries.start) - query->start;
    noerrorShim();
}

void APIENTRY_GL4ES gl4es_glGetQueryiv(GLenum target, GLenum pname, GLint* params) {
    FLUSH_BEGINEND;

    glquery_t *q = find_query_target(target);
    
    // GL_CURRENT_QUERY doesn't require an active query
    if(pname == GL_CURRENT_QUERY) {
        *params = q ? q->id : 0;
        noerrorShim();
        return;
    }
    
    if(!q) {
        errorShim(GL_INVALID_OPERATION);
        return;
    }

    noerrorShim();
    switch (pname) {
        case GL_CURRENT_QUERY:
            *params = q->id;
            break;
        case GL_QUERY_COUNTER_BITS:
            if(q->target == GL_TIME_ELAPSED) {
                *params = 32;  // timestamp has precision
            } else if(q->use_hardware) {
                // Hardware boolean query - return 1 bit (0 or 1)
                *params = 1;
            } else {
                *params = 0;  // software stub has no counter
            }
            break;
        default:
            errorShim(GL_INVALID_ENUM);
    }
}

void APIENTRY_GL4ES gl4es_glGetQueryObjectiv(GLuint id, GLenum pname, GLint* params) {
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
                    *params = (GLint)available;
                } else {
                    *params = GL_TRUE;  // fallback
                }
            } else {
                *params = GL_TRUE;  // software stub always ready
            }
            break;
            
        case GL_QUERY_RESULT_NO_WAIT:
        case GL_QUERY_RESULT:
            if(query->use_hardware) {
                LOAD_GLES_EXT(glGetQueryObjectuivEXT);
                if(gles_glGetQueryObjectuivEXT) {
                    GLuint gles_result;
                    gles_glGetQueryObjectuivEXT(query->gles_id, GL_QUERY_RESULT_EXT, &gles_result);
                    
                    // Convert boolean result to integer
                    // GL_EXT_occlusion_query_boolean returns 0 or GL_TRUE
                    // We emulate a sample count: 0 = no samples, 1 = samples passed
                    // This is sufficient for Minecraft's visibility testing
                    *params = gles_result ? 1 : 0;
                } else {
                    *params = 0;
                }
            } else {
                // Software fallback
                *params = (query->target==GL_TIME_ELAPSED) ? (GLint)query->start : query->num;
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
                    gles_glGetQueryObjectuivEXT(query->gles_id, GL_QUERY_RESULT_AVAILABLE_EXT, params);
                } else {
                    *params = GL_TRUE;
                }
            } else {
                *params = GL_TRUE;  // software stub always ready
            }
            break;
            
        case GL_QUERY_RESULT_NO_WAIT:
        case GL_QUERY_RESULT:
            if(query->use_hardware) {
                LOAD_GLES_EXT(glGetQueryObjectuivEXT);
                if(gles_glGetQueryObjectuivEXT) {
                    GLuint gles_result;
                    gles_glGetQueryObjectuivEXT(query->gles_id, GL_QUERY_RESULT_EXT, &gles_result);
                    
                    // Convert boolean to integer count
                    // 0 = no samples visible, 1 = samples visible
                    *params = gles_result ? 1 : 0;
                } else {
                    *params = 0;
                }
            } else {
                // Software fallback
                *params = (query->target==GL_TIME_ELAPSED) ? (GLuint)query->start : (GLuint)query->num;
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