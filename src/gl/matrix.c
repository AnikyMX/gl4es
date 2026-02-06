#include "matrix.h"

#include "../glx/hardext.h"
#include "debug.h"
#include "fpe.h"
#include "gl4es.h"
#include "glstate.h"
#include "init.h"
#include "loader.h"
#include <math.h>
#include <string.h>

//#define DEBUG
#ifdef DEBUG
#define DBG(a) a
#else
#define DBG(a)
#endif

// Optimization macros for Branch Prediction on ARM Cortex-A53
#define likely(x)       __builtin_expect(!!(x), 1)
#define unlikely(x)     __builtin_expect(!!(x), 0)

void alloc_matrix(matrixstack_t **matrixstack, int depth) {
    *matrixstack = (matrixstack_t*)malloc(sizeof(matrixstack_t));
    (*matrixstack)->top = 0;
    (*matrixstack)->identity = 0;
    // Align memory to 16 bytes for NEON SIMD operations if possible
    (*matrixstack)->stack = (GLfloat*)aligned_alloc(16, sizeof(GLfloat)*depth*16);
    if (!(*matrixstack)->stack) {
        // Fallback if aligned_alloc fails or not present
        (*matrixstack)->stack = (GLfloat*)malloc(sizeof(GLfloat)*depth*16);
    }
}

#define TOP(A) (glstate->A->stack+(glstate->A->top*16))

// Optimized: Prioritize GL_MODELVIEW as it's the most used mode in Minecraft
static inline GLfloat* update_current_mat() {
    if (likely(glstate->matrix_mode == GL_MODELVIEW)) {
        return TOP(modelview_matrix);
    }
    switch(glstate->matrix_mode) {
        case GL_PROJECTION:
            return TOP(projection_matrix);
        case GL_TEXTURE:
            return TOP(texture_matrix[glstate->texture.active]);
        default:
            if(glstate->matrix_mode>=GL_MATRIX0_ARB && glstate->matrix_mode<GL_MATRIX0_ARB+MAX_ARB_MATRIX)
                return TOP(arb_matrix[glstate->matrix_mode-GL_MATRIX0_ARB]);
            return NULL;
    }
}

static int update_current_identity(int I) {
    if (likely(glstate->matrix_mode == GL_MODELVIEW)) {
        return glstate->modelview_matrix->identity = (I)?1:is_identity(TOP(modelview_matrix));
    }
    switch(glstate->matrix_mode) {
        case GL_PROJECTION:
            return glstate->projection_matrix->identity = (I)?1:is_identity(TOP(projection_matrix));
        case GL_TEXTURE:
            return glstate->texture_matrix[glstate->texture.active]->identity = (I)?1:is_identity(TOP(texture_matrix[glstate->texture.active]));
        default:
            if(glstate->matrix_mode>=GL_MATRIX0_ARB && glstate->matrix_mode<GL_MATRIX0_ARB+MAX_ARB_MATRIX)
                return glstate->arb_matrix[glstate->matrix_mode-GL_MATRIX0_ARB]->identity = (I)?1:is_identity(TOP(arb_matrix[glstate->matrix_mode-GL_MATRIX0_ARB]));
        return 0;
    }
}

static int send_to_hardware() {
    if(hardext.esversion > 1)
        return 0;
    
    // Switch reordered for probability
    switch(glstate->matrix_mode) {
        case GL_MODELVIEW:
            return 1;
        case GL_PROJECTION:
            return 1;
        case GL_TEXTURE:
            return (globals4es.texmat)?1:0;
    }
    return 0;
}

void init_matrix(glstate_t* glstate) {
DBG(printf("init_matrix(%p)\n", glstate);)
    alloc_matrix(&glstate->projection_matrix, MAX_STACK_PROJECTION);
    set_identity(TOP(projection_matrix));
    glstate->projection_matrix->identity = 1;
    
    alloc_matrix(&glstate->modelview_matrix, MAX_STACK_MODELVIEW);
    set_identity(TOP(modelview_matrix));
    glstate->modelview_matrix->identity = 1;
    
    glstate->texture_matrix = (matrixstack_t**)malloc(sizeof(matrixstack_t*)*MAX_TEX);
    glstate->arb_matrix = (matrixstack_t**)malloc(sizeof(matrixstack_t*)*MAX_ARB_MATRIX);
    
    set_identity(glstate->mvp_matrix);
    glstate->mvp_matrix_dirty = 0;
    set_identity(glstate->inv_mv_matrix);
    glstate->inv_mv_matrix_dirty = 0;
    
    // Normal matrix init
    memset(glstate->normal_matrix, 0, 9*sizeof(GLfloat));
    glstate->normal_matrix[0] = glstate->normal_matrix[4] = glstate->normal_matrix[8] = 1.0f;
    glstate->normal_matrix_dirty = 1;
    
    for (int i=0; i<MAX_TEX; i++) {
        alloc_matrix(&glstate->texture_matrix[i], MAX_STACK_TEXTURE);
        set_identity(TOP(texture_matrix[i]));
        glstate->texture_matrix[i]->identity = 1;
    }
    for (int i=0; i<MAX_ARB_MATRIX; i++) {
        alloc_matrix(&glstate->arb_matrix[i], MAX_STACK_ARB_MATRIX);
        set_identity(TOP(arb_matrix[i]));
        glstate->arb_matrix[i]->identity = 1;
    }
}

void set_fpe_textureidentity() {
    // Branchless optimization attempt or simple ternary
    glstate->fpe_state->texture[glstate->texture.active].texmat = (glstate->texture_matrix[glstate->texture.active]->identity) ? 0 : 1;
}

void APIENTRY_GL4ES gl4es_glMatrixMode(GLenum mode) {
DBG(printf("glMatrixMode(%s), list=%p\n", PrintEnum(mode), glstate->list.active);)
    noerrorShim();
    // Quick check for redundant state change which is common in MC
    if (glstate->matrix_mode == mode) {
        if (glstate->list.active && glstate->list.pending && glstate->matrix_mode==GL_MODELVIEW) {
             return;
        }
        // Even if mode is same, we might need to record it if compiling list? 
        // Original logic says "if list active... and same mode... return". 
        // We stick to original logic but make it cleaner.
    }

    PUSH_IF_COMPILING(glMatrixMode);

    if(unlikely(!((mode==GL_MODELVIEW) || (mode==GL_PROJECTION) || (mode==GL_TEXTURE) || (mode>=GL_MATRIX0_ARB && mode<(GL_MATRIX0_ARB+MAX_ARB_MATRIX))))) {
        errorShim(GL_INVALID_ENUM);
        return;
    }
    
    if(glstate->matrix_mode != mode) {
        glstate->matrix_mode = mode;
        LOAD_GLES_FPE(glMatrixMode);
        gles_glMatrixMode(mode);
    }
}

void APIENTRY_GL4ES gl4es_glPushMatrix(void) {
DBG(printf("glPushMatrix(), list=%p\n", glstate->list.active);)
    if (unlikely(glstate->list.active && !glstate->list.pending)) {
        PUSH_IF_COMPILING(glPushMatrix);
    }
    
    noerrorShim();
    
    // Optimized switch for ModelView priority
    if (likely(glstate->matrix_mode == GL_MODELVIEW)) {
        if(glstate->modelview_matrix->top+1 < MAX_STACK_MODELVIEW) {
            GLfloat *src = TOP(modelview_matrix);
            memcpy(src+16, src, 16*sizeof(GLfloat));
            glstate->modelview_matrix->top++;
        } else errorShim(GL_STACK_OVERFLOW);
        return;
    }

    switch(glstate->matrix_mode) {
        #define P(A, B) if(glstate->A->top+1<MAX_STACK_##B) { \
            memcpy(TOP(A)+16, TOP(A), 16*sizeof(GLfloat)); \
            glstate->A->top++; \
        } else errorShim(GL_STACK_OVERFLOW)
        case GL_PROJECTION:
            P(projection_matrix, PROJECTION);
            break;
        case GL_TEXTURE:
            P(texture_matrix[glstate->texture.active], TEXTURE);
            break;
        default:
            if(glstate->matrix_mode>=GL_MATRIX0_ARB && glstate->matrix_mode<GL_MATRIX0_ARB+MAX_ARB_MATRIX) {
                P(arb_matrix[glstate->matrix_mode-GL_MATRIX0_ARB], ARB_MATRIX);
            } else {
                errorShim(GL_INVALID_OPERATION);
            }
        #undef P
    }
}

void APIENTRY_GL4ES gl4es_glPopMatrix(void) {
DBG(printf("glPopMatrix(), list=%p\n", glstate->list.active);)
    // Fast check for Display List compilation optimization
    if (unlikely(glstate->list.active 
     && !(glstate->list.compiling)
     && (globals4es.beginend) 
     && glstate->matrix_mode==GL_MODELVIEW
     && !(glstate->polygon_mode==GL_LINE) 
     && glstate->list.pending)) {
        if(memcmp(TOP(modelview_matrix)-16, TOP(modelview_matrix), 16*sizeof(GLfloat))==0) {
            --glstate->modelview_matrix->top;
            return;
        }
    }
    PUSH_IF_COMPILING(glPopMatrix);
    
    noerrorShim();

    // Handling GL_MODELVIEW first for performance
    if (likely(glstate->matrix_mode == GL_MODELVIEW)) {
        if(glstate->modelview_matrix->top) {
            --glstate->modelview_matrix->top;
            glstate->modelview_matrix->identity = is_identity(TOP(modelview_matrix));
            if (send_to_hardware()) {
                LOAD_GLES(glLoadMatrixf); 
                gles_glLoadMatrixf(TOP(modelview_matrix)); 
            }
            glstate->mvp_matrix_dirty = 1;
            glstate->inv_mv_matrix_dirty = 1;
            glstate->normal_matrix_dirty = 1;
        } else errorShim(GL_STACK_UNDERFLOW);
        return;
    }

    switch(glstate->matrix_mode) {
        #define P(A) if(glstate->A->top) { \
            --glstate->A->top; \
            glstate->A->identity = is_identity(update_current_mat()); \
            if (send_to_hardware()) {LOAD_GLES(glLoadMatrixf); gles_glLoadMatrixf(update_current_mat()); } \
        } else errorShim(GL_STACK_UNDERFLOW)
        case GL_PROJECTION:
            P(projection_matrix);
            glstate->mvp_matrix_dirty = 1;
            break;
        case GL_TEXTURE:
            P(texture_matrix[glstate->texture.active]);
            if(glstate->fpe_state)
                set_fpe_textureidentity();
            break;
        default:
            if(glstate->matrix_mode>=GL_MATRIX0_ARB && glstate->matrix_mode<GL_MATRIX0_ARB+MAX_ARB_MATRIX) {
                P(arb_matrix[glstate->matrix_mode-GL_MATRIX0_ARB]);
            } else {
                errorShim(GL_INVALID_OPERATION);
            }
        #undef P
    }
}

void APIENTRY_GL4ES gl4es_glLoadMatrixf(const GLfloat * m) {
DBG(printf("glLoadMatrix(%f, %f, %f, %f, %f, %f, %f...), list=%p\n", m[0], m[1], m[2], m[3], m[4], m[5], m[6], glstate->list.active);)
    if (unlikely(glstate->list.active)) {
        if(glstate->list.pending) gl4es_flush();
        else {
            NewStage(glstate->list.active, STAGE_MATRIX);
            glstate->list.active->matrix_op = 1;
            memcpy(glstate->list.active->matrix_val, m, 16*sizeof(GLfloat));
            return;
        }
    }
    
    // Direct memcpy preferred
    memcpy(update_current_mat(), m, 16*sizeof(GLfloat));
    
    const int id = update_current_identity(0);
    
    // Flag updates
    if(likely(glstate->matrix_mode==GL_MODELVIEW)) {
        glstate->normal_matrix_dirty = glstate->inv_mv_matrix_dirty = 1;
        glstate->mvp_matrix_dirty = 1;
    } else if(glstate->matrix_mode==GL_PROJECTION) {
        glstate->mvp_matrix_dirty = 1;
    } else if((glstate->matrix_mode==GL_TEXTURE) && glstate->fpe_state) {
        set_fpe_textureidentity();
    }

    if(send_to_hardware()) {
        LOAD_GLES(glLoadMatrixf);
        LOAD_GLES(glLoadIdentity);
        if(id) gles_glLoadIdentity();
        else gles_glLoadMatrixf(m);
    }
}

void APIENTRY_GL4ES gl4es_glMultMatrixf(const GLfloat * m) {
DBG(printf("glMultMatrix(%f, %f, %f, %f, %f, %f, %f...), list=%p\n", m[0], m[1], m[2], m[3], m[4], m[5], m[6], glstate->list.active);)
    if (unlikely(glstate->list.active)) {
        if(glstate->list.pending) gl4es_flush();
        else {
            if(glstate->list.active->stage == STAGE_MATRIX) {
                matrix_mul(glstate->list.active->matrix_val, m, glstate->list.active->matrix_val);
                return;
            }
            NewStage(glstate->list.active, STAGE_MATRIX);
            glstate->list.active->matrix_op = 2;
            memcpy(glstate->list.active->matrix_val, m, 16*sizeof(GLfloat));
            return;
        }
    }
    
    GLfloat *current_mat = update_current_mat();
    matrix_mul(current_mat, m, current_mat);
    
    const int id = update_current_identity(0);
    
    if(likely(glstate->matrix_mode==GL_MODELVIEW)) {
        glstate->normal_matrix_dirty = glstate->inv_mv_matrix_dirty = 1;
        glstate->mvp_matrix_dirty = 1;
    } else if(glstate->matrix_mode==GL_PROJECTION) {
        glstate->mvp_matrix_dirty = 1;
    } else if((glstate->matrix_mode==GL_TEXTURE) && glstate->fpe_state) {
        set_fpe_textureidentity();
    }
    
    if(send_to_hardware()) {
        LOAD_GLES(glLoadMatrixf);
        LOAD_GLES(glLoadIdentity);
        if(id) gles_glLoadIdentity();
        else gles_glLoadMatrixf(current_mat);
    }
}

void APIENTRY_GL4ES gl4es_glLoadIdentity(void) {
DBG(printf("glLoadIdentity(), list=%p\n", glstate->list.active);)
    if (unlikely(glstate->list.active)) {
        if(glstate->list.pending) gl4es_flush();
        else {
            NewStage(glstate->list.active, STAGE_MATRIX);
            glstate->list.active->matrix_op = 1;
            set_identity(glstate->list.active->matrix_val);
            return;
        }
    }
    set_identity(update_current_mat());
    update_current_identity(1);
    
    if(likely(glstate->matrix_mode==GL_MODELVIEW)) {
        glstate->normal_matrix_dirty = glstate->inv_mv_matrix_dirty = 1;
        glstate->mvp_matrix_dirty = 1;
    } else if(glstate->matrix_mode==GL_PROJECTION) {
        glstate->mvp_matrix_dirty = 1;
    } else if((glstate->matrix_mode==GL_TEXTURE) && glstate->fpe_state) {
        set_fpe_textureidentity();
    }
    
    if(send_to_hardware()) {
        LOAD_GLES(glLoadIdentity);
        gles_glLoadIdentity();
    }
}

// OPTIMIZED: Direct matrix modification instead of slow multiplication
void APIENTRY_GL4ES gl4es_glTranslatef(GLfloat x, GLfloat y, GLfloat z) {
DBG(printf("glTranslatef(%f, %f, %f), list=%p\n", x, y, z, glstate->list.active);)
    if (unlikely(glstate->list.active)) {
        // Fallback for display lists to maintain correctness
        GLfloat tmp[16];
        set_identity(tmp);
        tmp[12+0] = x; tmp[12+1] = y; tmp[12+2] = z;
        gl4es_glMultMatrixf(tmp);
        return;
    }

    // Direct operation: M = M * T
    // Only the last column of M changes.
    // M_new[12] = M[0]*x + M[4]*y + M[8]*z + M[12]
    // ...
    GLfloat *m = update_current_mat();
    
    // Explicit vectorization hint for compiler
    m[12] = m[0]*x + m[4]*y + m[8]*z + m[12];
    m[13] = m[1]*x + m[5]*y + m[9]*z + m[13];
    m[14] = m[2]*x + m[6]*y + m[10]*z + m[14];
    m[15] = m[3]*x + m[7]*y + m[11]*z + m[15];

    // State updates
    update_current_identity(0);
    if(likely(glstate->matrix_mode==GL_MODELVIEW)) {
        glstate->normal_matrix_dirty = glstate->inv_mv_matrix_dirty = 1;
        glstate->mvp_matrix_dirty = 1;
    } else if(glstate->matrix_mode==GL_PROJECTION) {
        glstate->mvp_matrix_dirty = 1;
    }
    
    if(send_to_hardware()) {
        LOAD_GLES(glLoadMatrixf);
        gles_glLoadMatrixf(m);
    }
}

// OPTIMIZED: Direct matrix modification
void APIENTRY_GL4ES gl4es_glScalef(GLfloat x, GLfloat y, GLfloat z) {
DBG(printf("glScalef(%f, %f, %f), list=%p\n", x, y, z, glstate->list.active);)
    if (unlikely(glstate->list.active)) {
        GLfloat tmp[16];
        memset(tmp, 0, 16*sizeof(GLfloat));
        tmp[0] = x; tmp[5] = y; tmp[10] = z; tmp[15] = 1.0f;
        gl4es_glMultMatrixf(tmp);
        return;
    }

    GLfloat *m = update_current_mat();
    // Column 0 * x
    m[0] *= x; m[1] *= x; m[2] *= x; m[3] *= x;
    // Column 1 * y
    m[4] *= y; m[5] *= y; m[6] *= y; m[7] *= y;
    // Column 2 * z
    m[8] *= z; m[9] *= z; m[10] *= z; m[11] *= z;
    
    // State updates
    update_current_identity(0);
    if(likely(glstate->matrix_mode==GL_MODELVIEW)) {
        glstate->normal_matrix_dirty = glstate->inv_mv_matrix_dirty = 1;
        glstate->mvp_matrix_dirty = 1;
    } else if(glstate->matrix_mode==GL_PROJECTION) {
        glstate->mvp_matrix_dirty = 1;
    }

    if(send_to_hardware()) {
        LOAD_GLES(glLoadMatrixf);
        gles_glLoadMatrixf(m);
    }
}

void APIENTRY_GL4ES gl4es_glRotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z) {
DBG(printf("glRotatef(%f, %f, %f, %f), list=%p\n", angle, x, y, z, glstate->list.active);)
    if (angle == 0.0f) return;
    if ((x==0 && y==0 && z==0)) return;

    // Use optimized version logic even for Display Lists, 
    // but pass through MultMatrix logic for recording.
    
    GLfloat tmp[16];
    // Don't memset 0, set necessary values directly to avoid cache pollution
    
    // normalize x y z
    // Using reciprocal sqrt approximation if -ffast-math is on (default in build.yml)
    GLfloat l = 1.0f/sqrtf(x*x+y*y+z*z);
    x*=l; y*=l; z*=l;
    
    // calculate sin/cos
    angle *= 3.1415926535f/180.f;
    
    // Use sincosf (GNU extension, standard in NDK) for single instruction calc on ARMv8
    GLfloat s, c;
    #ifdef __USE_GNU
        sincosf(angle, &s, &c);
    #else
        s = sinf(angle);
        c = cosf(angle);
    #endif

    const GLfloat c1 = 1.0f-c;

    // Build the rotation matrix
    // Row-Major layout construction locally? No, OpenGL is Col-Major.
    // Index: 0, 1, 2, 3 (Col 0)
    
    tmp[0] = x*x*c1+c;    tmp[4] = x*y*c1-z*s;  tmp[8] = x*z*c1+y*s;   tmp[12] = 0.0f;
    tmp[1] = y*x*c1+z*s;  tmp[5] = y*y*c1+c;    tmp[9] = y*z*c1-x*s;   tmp[13] = 0.0f;
    tmp[2] = x*z*c1-y*s;  tmp[6] = y*z*c1+x*s;  tmp[10] = z*z*c1+c;    tmp[14] = 0.0f;
    tmp[3] = 0.0f;        tmp[7] = 0.0f;        tmp[11] = 0.0f;        tmp[15] = 1.0f;

    gl4es_glMultMatrixf(tmp);
}

void APIENTRY_GL4ES gl4es_glOrthof(GLfloat left, GLfloat right, GLfloat bottom, GLfloat top, GLfloat nearVal, GLfloat farVal) {
DBG(printf("glOrthof(%f, %f, %f, %f, %f, %f), list=%p\n", left, right, top, bottom, nearVal, farVal, glstate->list.active);)
    GLfloat tmp[16];
    memset(tmp, 0, 16*sizeof(GLfloat));

    // Pre-calculate divisions to allow multiplication (faster on ARM Cortex-A53)
    float r_l = 1.0f / (right - left);
    float t_b = 1.0f / (top - bottom);
    float f_n = 1.0f / (farVal - nearVal);

    tmp[0] = 2.0f * r_l;
    tmp[12] = -(right + left) * r_l;
    
    tmp[5] = 2.0f * t_b;
    tmp[13] = -(top + bottom) * t_b;
    
    tmp[10] = -2.0f * f_n;
    tmp[14] = -(farVal + nearVal) * f_n;
    
    tmp[15] = 1.0f;

    gl4es_glMultMatrixf(tmp);
}

void APIENTRY_GL4ES gl4es_glFrustumf(GLfloat left,  GLfloat right, GLfloat bottom, GLfloat top, GLfloat nearVal, GLfloat farVal) {
DBG(printf("glFrustumf(%f, %f, %f, %f, %f, %f) list=%p\n", left, right, top, bottom, nearVal, farVal, glstate->list.active);)
    GLfloat tmp[16];
    memset(tmp, 0, 16*sizeof(GLfloat));

    float r_l = 1.0f / (right - left);
    float t_b = 1.0f / (top - bottom);
    float f_n = 1.0f / (farVal - nearVal);
    float n2 = 2.0f * nearVal;

    tmp[0] = n2 * r_l;
    tmp[8] = (right + left) * r_l;
    
    tmp[5] = n2 * t_b;
    tmp[9] = (top + bottom) * t_b;
    
    tmp[10] = -(farVal + nearVal) * f_n;
    tmp[14] = -(farVal * n2) * f_n; // Optimized: 2*far*near
    
    tmp[11] = -1.0f;

    gl4es_glMultMatrixf(tmp);
}

AliasExport(void,glMatrixMode,,(GLenum mode));
AliasExport_V(void,glPushMatrix);
AliasExport_V(void,glPopMatrix);
AliasExport(void,glLoadMatrixf,,(const GLfloat * m));
AliasExport(void,glMultMatrixf,,(const GLfloat * m));
AliasExport_V(void,glLoadIdentity);
AliasExport(void,glTranslatef,,(GLfloat x, GLfloat y, GLfloat z));
AliasExport(void,glScalef,,(GLfloat x, GLfloat y, GLfloat z));
AliasExport(void,glRotatef,,(GLfloat angle, GLfloat x, GLfloat y, GLfloat z));
AliasExport(void,glOrthof,,(GLfloat left, GLfloat right, GLfloat bottom, GLfloat top, GLfloat nearVal, GLfloat farVal));
AliasExport(void,glFrustumf,,(GLfloat left, GLfloat right, GLfloat bottom, GLfloat top, GLfloat nearVal, GLfloat farVal));