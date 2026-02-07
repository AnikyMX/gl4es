#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifndef _WIN32
#include <unistd.h>
#else
#include <stdio.h>
#include <direct.h>
#define getcwd(a,b) _getcwd(a,b)
#define snprintf _snprintf
#endif

#include "../../version.h"
#include "../glx/glx_gbm.h"
#include "../glx/streaming.h"
#include "build_info.h"
#include "debug.h"
#include "loader.h"
#include "logs.h"
#include "fpe_cache.h"
#include "init.h"
#include "envvars.h"

#if defined(__EMSCRIPTEN__) || defined(__APPLE__)
#define NO_INIT_CONSTRUCTOR
#endif

// Optimasi Branch Prediction
#ifndef LIKELY
#define LIKELY(x)   __builtin_expect(!!(x), 1)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)
#endif

void gl_init();
void gl_close();

#ifdef GL4ES_COMPILE_FOR_USE_IN_SHARED_LIB
#ifdef AMIGAOS4
void agl_reset_internals();
#endif
void fpe_shader_reset_internals();
#endif

globals4es_t globals4es = {0};

// NEON/VFP Optimization for Cortex-A53
#if defined(__arm__) || defined(__aarch64__)
static void fast_math() {
   // Enable Flush-to-Zero mode for floating point (Speedup on ARM)
#ifdef __aarch64__
   uint64_t fpcr;
   __asm__ __volatile__ (
     "mrs %0, fpcr\n"
     "orr %0, %0, #(1<<24)\n" // FZ (Flush-to-zero)
     "msr fpcr, %0\n"
     : "=r"(fpcr));
#else
   int v = 0;
   __asm__ __volatile__ (
     "vmrs %0, fpscr\n"
     "orr  %0, #((1<<25)|(1<<24))\n" // default NaN, flush-to-zero
     "vmsr fpscr, %0\n"
     : "=&r"(v));
#endif
}
#endif

#ifndef DEFAULT_ES
#define DEFAULT_ES 2 // Force ES2.0 for Android
#endif

void load_libs();
void glx_init();

static int inited = 0;

EXPORT
void set_getmainfbsize(void (APIENTRY_GL4ES  *new_getMainFBSize)(int* w, int* h)) {
    gl4es_getMainFBSize = (void*)new_getMainFBSize;
}

EXPORT
void set_getprocaddress(void *(APIENTRY_GL4ES  *new_proc_address)(const char *)) {
    gles_getProcAddress = new_proc_address;
}

#ifdef NO_INIT_CONSTRUCTOR
EXPORT
#else
#if defined(_WIN32) || defined(__CYGWIN__)
#define BUILD_WINDOWS_DLL
static unsigned char dll_inited;
EXPORT
#endif
#if !defined(_MSC_VER) || defined(__clang__)
__attribute__((constructor(101)))
#endif
#endif
void initialize_gl4es() {
#ifdef BUILD_WINDOWS_DLL
    if(!dll_inited) return;
#endif
    // Singleton Check
    if(LIKELY(inited++)) return;

    // --- HELIO P35 & POWERVR GE8320 OPTIMIZATION PROFILE ---
    // Zero out everything first
    memset(&globals4es, 0, sizeof(globals4es));

    // 1. CORE PERFORMANCE FLAGS
    globals4es.mergelist = 1;   // Penting untuk Minecraft (banyak list kecil)
    globals4es.queries = 1;     // Minecraft butuh Occlusion Query
    globals4es.beginend = 1;    // Optimize glBegin/glEnd
    globals4es.deepbind = 1;    // Standard for Android linker
    
    // 2. DISABLE BLOAT
    globals4es.nobanner = 1;    // Hemat waktu startup (no print to stdout)
    globals4es.showfps = 0;     // Matikan overhead FPS counter
    globals4es.stacktrace = 0;  // Hemat CPU saat crash (fail fast)
    
    // 3. RENDER PATH (Android)
    globals4es.usefb = 1;
    globals4es.usefbo = 1;      // Wajib untuk PowerVR
    globals4es.usegbm = 0;
    
    // 4. POWERVR TWEAKS (The Secret Sauce)
    // GE8320 benci alokasi memori berulang dan alpha channel yang tidak perlu
    globals4es.recyclefbo = 1;  // RECYCLE FBO: Wajib! Mengurangi stutter inventory.
    globals4es.fbo_noalpha = 1; // Hemat bandwidth: Main screen tidak butuh Alpha channel.
    globals4es.fbounbind = 1;   // Workaround bug driver PowerVR tertentu.
    
    // 5. VERSION FORCING
    // Jangan biarkan GL4ES menebak-nebak. Kita paksa.
    globals4es.es = 2;          // Backend GLES 2.0
    globals4es.gl = 21;         // Export OpenGL 2.1 (Cukup untuk MC 1.16.5 ke bawah)
    
    // 6. TEXTURE & BANDWIDTH OPTIMIZATION
    globals4es.floattex = 1;    // Enable float texture (untuk shader)
    globals4es.automipmap = 1;  // Force automipmap (lebih halus)
    globals4es.texmat = 0;      // Handle texture matrix di hardware
    globals4es.potframebuffer = 0; // PowerVR support NPOT, jangan paksa POT
    globals4es.defaultwrap = 1; // CLAMP_TO_EDGE (Default GLES behavior, lebih cepat)
    
    // [CRITICAL] Bandwidth Saver for Helio P35
    globals4es.avoid24bits = 1; // Prefer 16-bit textures jika memungkinkan (RGB565)
    globals4es.compress = 1;    // Gunakan kompresi tekstur internal jika ada
    
    // 7. BATCHING CONFIGURATION
    // Ini adalah kunci FPS tinggi di CPU lemah
    globals4es.minbatch = 40;   // Jangan kirim draw call < 40 vertex
    globals4es.maxbatch = 1000; // Gabungkan hingga 1000 vertex
    
    // 8. VBO CONFIGURATION
    globals4es.usevbo = 1;      // Gunakan VBO standar
    
    // 9. SHADER TWEAKS
    globals4es.comments = 0;    // Hapus komentar shader (hemat memori/waktu compile)
    globals4es.normalize = 1;   // Safety net untuk shader Minecraft
    globals4es.silentstub = 1;  // Jangan log fungsi yang hilang (hemat I/O)
    
    // 10. SYSTEM TWEAKS
    globals4es.glxrecycle = 1;  // Recycle EGL Surface
    
    // --- END OF OPTIMIZATION PROFILE ---

    // Apply fast math if available
    #if defined(__arm__) || defined(__aarch64__)
    fast_math();
    #endif

    // Setup DRM Card (Fallback logic only)
    strcpy(globals4es.drmcard, "/dev/dri/card0");

    // Load libraries (GLES, EGL)
#if !defined(__EMSCRIPTEN__) && !defined(__APPLE__)
    load_libs();
#endif

    // Detect Hardware Capabilities
    // Kita set notest=0 agar GL4ES benar-benar membaca kapabilitas GPU PowerVR
    GetHardwareExtensions(0);

    // Initializers
#if !defined(NOX11)
    glx_init();
#endif

    gl_init();

#ifdef GL4ES_COMPILE_FOR_USE_IN_SHARED_LIB
    fpe_shader_reset_internals();
#endif

    // Log Summary (Hanya info vital)
    SHUT_LOGD("GL4ES Optimized for Helio P35 & PowerVR GE8320 Initialized.\n");
    SHUT_LOGD("Batching: %d-%d, VBO: %d, FBO Recycle: %d\n", 
        globals4es.minbatch, globals4es.maxbatch, globals4es.usevbo, globals4es.recyclefbo);
}

#ifndef NOX11
void FreeFBVisual();
#endif

#ifdef NO_INIT_CONSTRUCTOR
EXPORT
#else
#ifdef BUILD_WINDOWS_DLL
EXPORT // symmetric for init -- trivialize application code
#endif
#if !defined(_MSC_VER) || defined(__clang__)
__attribute__((destructor))
#endif
#endif
void close_gl4es() {
    #ifdef GL4ES_COMPILE_FOR_USE_IN_SHARED_LIB
        SHUT_LOGD("Shutdown requested\n");
        // Reference counting check
        if(--inited) return;
    #endif

    SHUT_LOGD("Shutting down GL4ES Optimized Profile\n");

    #ifndef NOX11
    FreeFBVisual();
    #endif

    // Clean internal GL state
    gl_close();

    // Persist Shader Cache (Critical for next-run performance on Cortex-A53)
    if (!globals4es.nopsa) {
        fpe_writePSA();
        fpe_FreePSA();
    }

    #if defined(GL4ES_COMPILE_FOR_USE_IN_SHARED_LIB) && defined(AMIGAOS4)
    os4CloseLib();
    #endif
}

#ifdef BUILD_WINDOWS_DLL
#if !defined(_MSC_VER) || defined(__clang__)
__attribute__((constructor(103)))
#endif
void dll_init_done()
{ dll_inited = 1; }
#endif

// MSVC specific constructor/destructor handling
#if defined(_MSC_VER) && !defined(NO_INIT_CONSTRUCTOR) && !defined(__clang__)
#pragma const_seg(".CRT$XCU")
void (*const gl4es_ctors[])() = { initialize_gl4es, dll_init_done };
#pragma const_seg(".CRT$XTX")
void (*const gl4es_dtor)() = close_gl4es;
#pragma const_seg()
#endif