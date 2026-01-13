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

#ifndef GL_ANY_SAMPLES_PASSED
#define GL_ANY_SAMPLES_PASSED 0x8C2F
#endif
#ifndef GL_ANY_SAMPLES_PASSED_CONSERVATIVE
#define GL_ANY_SAMPLES_PASSED_CONSERVATIVE 0x8D6A
#endif

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

#ifdef _WIN32
#ifdef _WINBASE_
#define GSM_CAST(c) ((LPFILETIME)c)
#else
__declspec(dllimport)
void __stdcall GetSystemTimeAsFileTime(unsigned __int64*);
#define GSM_CAST(c) ((__int64*)c)
#endif
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
        if(s->real_id) {
            LOAD_GLES(glDeleteQueries);
            gles_glDeleteQueries(1, &s->real_id);
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
    //FLUSH_BEGINEND;
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
	//FLUSH_BEGINEND;
	glquery_t *querie = find_query(id);
	if(querie)
		return GL_TRUE;
	return GL_FALSE;
}

void APIENTRY_GL4ES gl4es_glDeleteQueries(GLsizei n, const GLuint* ids) {
    //FLUSH_BEGINEND;
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
    //FLUSH_BEGINEND;
    glquery_t *query = find_query(id);
    if(!query) {
        khint_t k;
        int ret;
        khash_t(queries) *list = glstate->queries.querylist;
        k = kh_put(queries, list, id, &ret);
        query = kh_value(list, k) = calloc(1, sizeof(glquery_t));
        query->id = id;
    }
    if(query->active || find_query_target(target)) {
        errorShim(GL_INVALID_OPERATION);
        return;
    }
    
    // --- HARDWARE QUERY IMPLEMENTATION ---
    if(!query->real_id) {
        LOAD_GLES(glGenQueries);
        gles_glGenQueries(1, &query->real_id);
    }
    
    LOAD_GLES(glBeginQuery);

    // [FIX 8 FPS] TRANSLASI WAJIB UNTUK GLES 3.0
    // Driver GLES 3.0 akan ERROR jika dikasih GL_SAMPLES_PASSED (0x8914).
    // Kita harus ubah ke GL_ANY_SAMPLES_PASSED (0x8C2F) atau CONSERVATIVE (0x8D6A).
    GLenum driver_target = target;
    if (target == GL_SAMPLES_PASSED) {
        // Gunakan Conservative jika didukung (biasanya lebih cepat), atau fallback ke standard
        driver_target = GL_ANY_SAMPLES_PASSED_CONSERVATIVE;
    }
    
    gles_glBeginQuery(driver_target, query->real_id);
    // -------------------------------------

    query->target = target;
    query->num = 0;
    query->active = 1;
    noerrorShim();
}

void APIENTRY_GL4ES gl4es_glEndQuery(GLenum target) {
    //FLUSH_BEGINEND;
    glquery_t *query = find_query_target(target);
    if(!query) {
        errorShim(GL_INVALID_OPERATION);
        return;
    }

    LOAD_GLES(glEndQuery);
    gles_glEndQuery(target);

    query->active = 0;
    noerrorShim();
}

void APIENTRY_GL4ES gl4es_glGetQueryiv(GLenum target, GLenum pname, GLint* params) {
    //FLUSH_BEGINEND;

	glquery_t *q = find_query_target(target);
	if(!q) {
		errorShim(GL_INVALID_OPERATION);
		return;
	}

	noerrorShim();
	switch (pname) {
		case GL_CURRENT_QUERY:
			*params = (q->target==GL_TIME_ELAPSED)?q->start:q->num;
			break;
		case GL_QUERY_COUNTER_BITS:
			*params = (q->target==GL_TIME_ELAPSED)?32:0;	//no counter...
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
    		*params = GL_TRUE;//GL_FALSE;
    		break;
		case GL_QUERY_RESULT_NO_WAIT:
    	case GL_QUERY_RESULT:
    		*params = (query->target==GL_TIME_ELAPSED)?query->start:query->num;
    		break;
    	default:
    		errorShim(GL_INVALID_ENUM);
			return;
    }
    noerrorShim();
}

void APIENTRY_GL4ES gl4es_glGetQueryObjectuiv(GLuint id, GLenum pname, GLuint* params) {
    // FLUSH_BEGINEND; // Tetap matikan flush
    
    glquery_t *query = find_query(id);
    if(!query) {
        errorShim(GL_INVALID_OPERATION);
        return;
    }

    // --- DIAGNOSTIC MODE: WRITE ONLY ---
    // Kita "Pura-pura" tidak tahu ada hardware query.
    // Kita langsung return "1" (Visible) agar Minecraft merender semuanya.
    // Tujuannya: Mengecek apakah glBeginQuery di background bikin lag atau tidak.
    
    if (pname == GL_QUERY_RESULT || pname == GL_QUERY_RESULT_NO_WAIT) {
        *params = 1; // Selalu Terlihat (Force Visible)
    } else if (pname == GL_QUERY_RESULT_AVAILABLE) {
        *params = GL_TRUE; // Selalu Siap
    } else {
        *params = 0;
    }
    
    // PENTING: Jangan panggil LOAD_GLES atau gles_glGetQueryObjectuiv di sini!
    // Biarkan GPU kerja sendiri, kita masa bodoh dengan hasilnya.
    
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
    		*params = GL_TRUE;//GL_FALSE;
    		break;
		case GL_QUERY_RESULT_NO_WAIT:
    	case GL_QUERY_RESULT:
    		*params = (query->target==GL_TIME_ELAPSED)?query->start:query->num;
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
    		*params = GL_TRUE;//GL_FALSE;
    		break;
		case GL_QUERY_RESULT_NO_WAIT:
    	case GL_QUERY_RESULT:
    		*params = (query->target==GL_TIME_ELAPSED)?query->start:query->num;
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