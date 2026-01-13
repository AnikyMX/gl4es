#include "queries.h"
#include "khash.h"
#include "gl4es.h"
#include "glstate.h"
#include "loader.h"

// Definisi Struktur Map
KHASH_MAP_IMPL_INT(queries, glquery_t *);

// Fungsi Helper Sederhana
static glquery_t* find_query(GLuint querie) {
    khint_t k;
    khash_t(queries) *list = glstate->queries.querylist;
    k = kh_get(queries, list, querie);
    if (k != kh_end(list)){
        return kh_value(list, k);
    }
    return NULL;
}

// 1. GEN QUERIES: Cuma bikin ID di CPU
void APIENTRY_GL4ES gl4es_glGenQueries(GLsizei n, GLuint * ids) {
    // NO FLUSH!
    if (n<1) return;
    for (int i=0; i<n; i++) {
        // Simple increment ID
        ids[i] = ++glstate->queries.last_query;
        // Kita daftarkan ke map
        int ret;
        khint_t k = kh_put(queries, glstate->queries.querylist, ids[i], &ret);
        glquery_t *query = kh_value(glstate->queries.querylist, k) = calloc(1, sizeof(glquery_t));
        query->id = ids[i];
    }
    noerrorShim();
}

// 2. IS QUERY
GLboolean APIENTRY_GL4ES gl4es_glIsQuery(GLuint id) {
    // NO FLUSH!
    glquery_t *query = find_query(id);
    return (query) ? GL_TRUE : GL_FALSE;
}

// 3. DELETE QUERIES
void APIENTRY_GL4ES gl4es_glDeleteQueries(GLsizei n, const GLuint* ids) {
    // NO FLUSH!
    for(int i=0; i<n; ++i) {
        khint_t k = kh_get(queries, glstate->queries.querylist, ids[i]);
        if (k != kh_end(glstate->queries.querylist)){
            glquery_t *s = kh_value(glstate->queries.querylist, k);
            kh_del(queries, glstate->queries.querylist, k);
            free(s);
        }
    }
}

// 4. BEGIN QUERY: Cuma tandai aktif
void APIENTRY_GL4ES gl4es_glBeginQuery(GLenum target, GLuint id) {
    // NO FLUSH! SANGAT PENTING!
    glquery_t *query = find_query(id);
    if(!query) return; // Should not happen if Gen called
    
    query->target = target;
    query->active = 1;
    noerrorShim();
}

// 5. END QUERY: Cuma tandai non-aktif
void APIENTRY_GL4ES gl4es_glEndQuery(GLenum target) {
    // NO FLUSH!
    // Kita cari query yang aktif dengan target ini (simple search)
    glquery_t *q;
    kh_foreach_value(glstate->queries.querylist, q,
        if(q->active && q->target==target) {
            q->active = 0;
            break;
        }
    );
    noerrorShim();
}

// 6. GET QUERY OBJECT: INI KUNCINYA
void APIENTRY_GL4ES gl4es_glGetQueryObjectuiv(GLuint id, GLenum pname, GLuint* params) {
    // NO FLUSH!
    
    // Kita selalu jawab: SUDAH SIAP & ADA HASILNYA
    if (pname == GL_QUERY_RESULT_AVAILABLE) {
        *params = GL_TRUE;
    } else {
        // Minecraft tanya: "Ada berapa pixel yang kelihatan?"
        // Kita jawab: "Ada 100!" (Angka sembarang > 0)
        // Efek: Minecraft menganggap chunk ini TERLIHAT -> Render.
        *params = 100; 
    }
    noerrorShim();
}

// Wrapper sisanya (Copy paste saja biar lengkap)
void APIENTRY_GL4ES gl4es_glGetQueryiv(GLenum target, GLenum pname, GLint* params) {
    *params = 0; // Dummy
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