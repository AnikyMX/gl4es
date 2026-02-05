/*
 * Refactored fpe_cache.c for GL4ES
 * Optimized for on ARMv8
 * - Ultra-fast 64-bit FNV-1a Hashing for FPE States
 * - Buffered I/O for PSA (Precompiled Shader Archive)
 * - Branch prediction hints for hot paths
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h> // Required for uint64_t

#include "../glx/hardext.h"
#include "init.h"
#include "logs.h"
#include "debug.h"
#include "program.h"

#include "fpe.h"

#define fpe_state_t fpe_state_t
#define fpe_fpe_t fpe_fpe_t
#define kh_fpecachelist_t kh_fpecachelist_t
#include "fpe_cache.h"
#undef fpe_state_t
#undef fpe_fpe_t
#undef kh_fpecachelist_t

#ifndef fpe_cache_t
#   define fpe_cache_t kh_fpecachelist_t
#endif

// Branch prediction macros
#ifndef likely
#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)
#endif

//#define DEBUG
#ifdef DEBUG
#pragma GCC optimize 0
#define DBG(a) a
#else
#define DBG(a)
#endif

static const char PSA_SIGN[] = "GL4ES PrecompiledShaderArchive";
#define CACHE_VERSION 112

// OPTIMIZATION: High-performance 64-bit hashing for ARMv8
// Minecraft changes FPE state frequently; this function is a hotspot.
static kh_inline khint_t _hash_fpe(fpe_state_t *p)
{
    const uint64_t* p64 = (const uint64_t*)p;
    khint_t h = 2166136261U; // FNV-1a Offset Basis
    size_t len = sizeof(fpe_state_t) / 8;
    size_t i = 0;

    // Process 8 bytes at a time (Auto-vectorized by Clang/GCC)
    for (; i < len; ++i) {
        uint64_t v = p64[i];
        // Mix lower and upper 32-bits separately to maximize entropy
        h = (h ^ (v & 0xFFFFFFFF)) * 16777619U;
        h = (h ^ (v >> 32)) * 16777619U;
    }

    // Handle remaining bytes (tail)
    const uint8_t* p8 = (const uint8_t*)(p64 + i);
    size_t rem = sizeof(fpe_state_t) % 8;
    for (i = 0; i < rem; ++i) {
        h = (h ^ p8[i]) * 16777619U;
    }

    return h;
}

#define kh_fpe_hash_func(key) _hash_fpe(key)

#define kh_fpe_hash_equal(a, b) (memcmp(a, b, sizeof(fpe_state_t)) == 0)

#define KHASH_MAP_INIT_FPE(name, khval_t)                                \
    KHASH_INIT(name, kh_fpe_t, khval_t, 1, kh_fpe_hash_func, kh_fpe_hash_equal)

#define kh_fpecachelist_t kh_fpecachelist_s
KHASH_MAP_INIT_FPE(fpecachelist, fpe_fpe_t *);
#undef kh_fpecachelist_t

// ********* Cache handling *********

fpe_cache_t* fpe_NewCache() {
    khash_t(fpecachelist) *cache = kh_init(fpecachelist);
    return cache;
}

void fpe_disposeCache(fpe_cache_t* cache, int freeprog) {
    if(!cache) return;
    fpe_fpe_t *m;
    kh_foreach_value(cache, m, 
        if(freeprog) {
            if(m->glprogram)
                gl4es_glDeleteProgram(m->glprogram->id);
        }
        free(m);
    )
    kh_destroy(fpecachelist, cache);
}

fpe_fpe_t *fpe_GetCache(fpe_cache_t *cur, fpe_state_t *state, int fixed) {
    khint_t k;
    int r;

    // Fast path: Check hash map first
    k = kh_get(fpecachelist, cur, state);
    if(likely(k != kh_end(cur))) {
        return kh_value(cur, k);
    } else {
        // Slow path: Allocate new entry
        fpe_fpe_t *n = (fpe_fpe_t*)calloc(1, sizeof(fpe_fpe_t));
        if (unlikely(!n)) return NULL; // Safety check
        
        memcpy(&n->state, state, sizeof(fpe_state_t));
        k = kh_put(fpecachelist, cur, &n->state, &r);
        kh_value(cur, k) = n;
        return n;
    }
}

typedef struct psa_s {
    fpe_state_t state;
    GLenum      format;
    int         size;
    void* prog;
} psa_t;

KHASH_MAP_INIT_FPE(psalist, psa_t *);

// Precompiled Shader Archive
typedef struct gl4es_psa_s {
    int             size;
    int             dirty;
    kh_psalist_t* cache;    
} gl4es_psa_t;

static gl4es_psa_t *psa = NULL;
static char *psa_name = NULL;

// OPTIMIZATION: Large buffer for I/O operations to reduce syscalls
#define PSA_IO_BUF_SIZE (64 * 1024)

void fpe_readPSA()
{
    if(!psa || !psa_name)
        return;
        
    FILE *f = fopen(psa_name, "rb");
    if(!f) return;

    // Use a large buffer for reading to speed up startup time
    char *io_buf = malloc(PSA_IO_BUF_SIZE);
    if (io_buf) setvbuf(f, io_buf, _IOFBF, PSA_IO_BUF_SIZE);

    char tmp[sizeof(PSA_SIGN)];
    if(fread(tmp, sizeof(PSA_SIGN), 1, f)!=1) {
        if(io_buf) free(io_buf); fclose(f); return; // too short
    }
    if(strcmp(tmp, PSA_SIGN)!=0) {
        if(io_buf) free(io_buf); fclose(f); return; // bad signature
    }
    int version = 0;
    if(fread(&version, sizeof(version), 1, f)!=1) {
        if(io_buf) free(io_buf); fclose(f); return;
    }
    if(version!=CACHE_VERSION) {
        if(io_buf) free(io_buf); fclose(f); return; // unsupported version
    }
    int sz_fpe = 0;
    if(fread(&sz_fpe, sizeof(sz_fpe), 1, f)!=1) {
        if(io_buf) free(io_buf); fclose(f); return;
    }
    if(sz_fpe!=sizeof(fpe_state_t)) {
        if(io_buf) free(io_buf); fclose(f); return; // struct size mismatch
    }
    int n = 0;
    if(fread(&n, sizeof(n), 1, f)!=1) {
        if(io_buf) free(io_buf); fclose(f); return;
    }
    
    for (int i=0; i<n; ++i) {
        psa_t *p = (psa_t*)calloc(1, sizeof(psa_t));
        
        // Batch checking? No, structure variable size makes it hard.
        // But setvbuf above handles the buffering for us.
        if(fread(&p->state, sizeof(p->state), 1, f)!=1 ||
           fread(&p->format, sizeof(p->format), 1, f)!=1 ||
           fread(&p->size, sizeof(p->size), 1, f)!=1) {
            free(p);
            if(io_buf) free(io_buf); fclose(f); return;
        }
        
        if (p->size > 0) {
            p->prog = malloc(p->size);
            if(fread(p->prog, p->size, 1, f)!=1) {
                free(p->prog);
                free(p);
                if(io_buf) free(io_buf); fclose(f); return;
            }
        } else {
            p->prog = NULL;
        }

        int ret;
        khint_t k = kh_put(psalist, psa->cache, &p->state, &ret);
        kh_value(psa->cache, k) = p;
        psa->size = kh_size(psa->cache);
    }
    
    if(io_buf) free(io_buf);
    fclose(f);
    SHUT_LOGD("Loaded a PSA with %d Precompiled Programs\n", psa->size);
}

void fpe_writePSA()
{
    if(!psa || !psa_name)
        return;
    if(!psa->dirty)
        return; // no need
        
    FILE *f = fopen(psa_name, "wb");
    if(!f) return;

    // Use large buffer for writing
    char *io_buf = malloc(PSA_IO_BUF_SIZE);
    if (io_buf) setvbuf(f, io_buf, _IOFBF, PSA_IO_BUF_SIZE);

    if(fwrite(PSA_SIGN, sizeof(PSA_SIGN), 1, f)!=1) {
        if(io_buf) free(io_buf); fclose(f); return;
    }
    int version = CACHE_VERSION;
    if(fwrite(&version, sizeof(version), 1, f)!=1) {
        if(io_buf) free(io_buf); fclose(f); return;
    }
    int sz_fpe = sizeof(fpe_state_t);
    if(fwrite(&sz_fpe, sizeof(sz_fpe), 1, f)!=1) {
        if(io_buf) free(io_buf); fclose(f); return;
    }
    if(fwrite(&psa->size, sizeof(psa->size), 1, f)!=1) {
        if(io_buf) free(io_buf); fclose(f); return;
    }
    psa_t *p;
    kh_foreach_value(psa->cache, p, 
        if(fwrite(&p->state, sizeof(p->state), 1, f)!=1) {
            if(io_buf) free(io_buf); fclose(f); return;
        }
        if(fwrite(&p->format, sizeof(p->format), 1, f)!=1) {
            if(io_buf) free(io_buf); fclose(f); return;
        }
        if(fwrite(&p->size, sizeof(p->size), 1, f)!=1) {
            if(io_buf) free(io_buf); fclose(f); return;
        }
        if(p->size > 0 && p->prog) {
            if(fwrite(p->prog, p->size, 1, f)!=1) {
                if(io_buf) free(io_buf); fclose(f); return;
            }
        }
    );
    
    if(io_buf) free(io_buf);
    fclose(f);
    SHUT_LOGD("Saved a PSA with %d Precompiled Programs\n", psa->size);
}

void fpe_InitPSA(const char* name)
{
    if(psa)
        return; // already inited
    psa = (gl4es_psa_t*)calloc(1, sizeof(gl4es_psa_t));
    psa->cache = kh_init(psalist);
    psa_name = strdup(name);
}

void fpe_FreePSA()
{
    if(!psa)
        return; // nothing to init
    
    psa_t *m;
    kh_foreach_value(psa->cache, m, 
        if(m->prog) free(m->prog);
        free(m);
    )
    kh_destroy(psalist, psa->cache);

    free(psa);
    psa = NULL;
    if(psa_name) free(psa_name);
    psa_name = NULL;
}

int fpe_GetProgramPSA(GLuint program, fpe_state_t* state)
{
    if(!psa)
        return 0;
    // if state contains custom vertex of fragment shader, then ignore
    if(state->vertex_prg_enable || state->fragment_prg_enable)
        return 0;
    
    khint_t k = kh_get(psalist, psa->cache, state);
    if(likely(k==kh_end(psa->cache)))
        return 0; // not here
        
    psa_t *p = kh_value(psa->cache, k);
    // try to load...
    return gl4es_useProgramBinary(program, p->size, p->format, p->prog);
}

void fpe_AddProgramPSA(GLuint program, fpe_state_t* state)
{
    if(!psa)
        return;
    // if state contains custom vertex of fragment shader, then ignore
    if(state->vertex_prg_enable || state->fragment_prg_enable)
        return;
        
    psa->dirty = 1;
    psa_t *p = (psa_t*)calloc(1, sizeof(psa_t));
    memcpy(&p->state, state, sizeof(p->state));

    int l = gl4es_getProgramBinary(program, &p->size, &p->format, &p->prog);
    if(l==0) { // there was an error...
        if(p->prog) free(p->prog);
        free(p);
        return;
    }
    // add program
    int ret;
    khint_t k = kh_put(psalist, psa->cache, &p->state, &ret);
    if(!ret) {
        psa_t *p2 = kh_value(psa->cache, k);
        if(p2->prog) free(p2->prog);
        p2->prog = NULL;
        free(p2);
    }
    kh_value(psa->cache, k) = p;
    // all done
    psa->size = kh_size(psa->cache);
}