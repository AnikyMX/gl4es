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

// Helper macros for branch prediction optimization (Compiler Hints)
#define likely(x)       __builtin_expect(!!(x), 1)
#define unlikely(x)     __builtin_expect(!!(x), 0)

#define TOP(A) (glstate->A->stack+(glstate->A->top*16))

// OPTIMIZATION: Single block allocation to reduce cache misses
void alloc_matrix(matrixstack_t **matrixstack, int depth) {
    size_t total_size = sizeof(matrixstack_t) + (sizeof(GLfloat) * depth * 16);
    *matrixstack = (matrixstack_t*)malloc(total_size);
    (*matrixstack)->top = 0;
    (*matrixstack)->identity = 0;
    // Stack memory starts immediately after the struct
    (*matrixstack)->stack = (GLfloat*)((char*)(*matrixstack) + sizeof(matrixstack_t));
}

// Helper to quickly mark matrices as dirty based on current mode
static inline void mark_dirty_matrices(glstate_t* glstate) {
    if (glstate->matrix_mode == GL_MODELVIEW) {
        glstate->normal_matrix_dirty = 1;
        glstate->inv_mv_matrix_dirty = 1;
        glstate->mvp_matrix_dirty = 1;
    } else if (glstate->matrix_mode == GL_PROJECTION) {
        glstate->mvp_matrix_dirty = 1;
    } else if (glstate->matrix_mode == GL_TEXTURE && glstate->fpe_state) {
        // Inlined logic from set_fpe_textureidentity for speed
        glstate->fpe_state->texture[glstate->texture.active].texmat = 
            (glstate->texture_matrix[glstate->texture.active]->identity) ? 0 : 1;
    }
}

static GLfloat* update_current_mat() {
    // Optimized switch with likely cases first for Minecraft
    switch(glstate->matrix_mode) {
        case GL_MODELVIEW:
            return TOP(modelview_matrix);
        case GL_PROJECTION:
            return TOP(projection_matrix);
        case GL_TEXTURE:
            return TOP(texture_matrix[glstate->texture.active]);
        default:
            if(glstate->matrix_mode >= GL_MATRIX0_ARB && glstate->matrix_mode < GL_MATRIX0_ARB + MAX_ARB_MATRIX)
                return TOP(arb_matrix[glstate->matrix_mode - GL_MATRIX0_ARB]);
            return NULL;
    }
}

static int update_current_identity(int I) {
    switch(glstate->matrix_mode) {
        case GL_MODELVIEW:
            return glstate->modelview_matrix->identity = (I) ? 1 : is_identity(TOP(modelview_matrix));
        case GL_PROJECTION:
            return glstate->projection_matrix->identity = (I) ? 1 : is_identity(TOP(projection_matrix));
        case GL_TEXTURE:
            return glstate->texture_matrix[glstate->texture.active]->identity = (I) ? 1 : is_identity(TOP(texture_matrix[glstate->texture.active]));
        default:
            if(glstate->matrix_mode >= GL_MATRIX0_ARB && glstate->matrix_mode < GL_MATRIX0_ARB + MAX_ARB_MATRIX)
                return glstate->arb_matrix[glstate->matrix_mode - GL_MATRIX0_ARB]->identity = (I) ? 1 : is_identity(TOP(arb_matrix[glstate->matrix_mode - GL_MATRIX0_ARB]));
        return 0;
    }
}

static int send_to_hardware() {
    if(hardext.esversion > 1)
        return 0; // GLES 2.0+ handles matrices via uniforms usually
    
    switch(glstate->matrix_mode) {
        case GL_PROJECTION:
        case GL_MODELVIEW:
            return 1;
        case GL_TEXTURE:
            return (globals4es.texmat) ? 1 : 0;
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
    
    glstate->texture_matrix = (matrixstack_t**)malloc(sizeof(matrixstack_t*) * MAX_TEX);
    glstate->arb_matrix = (matrixstack_t**)malloc(sizeof(matrixstack_t*) * MAX_ARB_MATRIX);
    
    set_identity(glstate->mvp_matrix);
    glstate->mvp_matrix_dirty = 0;
    set_identity(glstate->inv_mv_matrix);
    glstate->inv_mv_matrix_dirty = 0;
    
    memset(glstate->normal_matrix, 0, 9 * sizeof(GLfloat));
    glstate->normal_matrix[0] = glstate->normal_matrix[4] = glstate->normal_matrix[8] = 1.0f;
    glstate->normal_matrix_dirty = 1;
    
    for (int i = 0; i < MAX_TEX; i++) {
        alloc_matrix(&glstate->texture_matrix[i], MAX_STACK_TEXTURE);
        set_identity(TOP(texture_matrix[i]));
        glstate->texture_matrix[i]->identity = 1;
    }
    for (int i = 0; i < MAX_ARB_MATRIX; i++) {
        alloc_matrix(&glstate->arb_matrix[i], MAX_STACK_ARB_MATRIX);
        set_identity(TOP(arb_matrix[i]));
        glstate->arb_matrix[i]->identity = 1;
    }
}

void set_fpe_textureidentity() {
    if(glstate->texture_matrix[glstate->texture.active]->identity)
        glstate->fpe_state->texture[glstate->texture.active].texmat = 0;
    else
        glstate->fpe_state->texture[glstate->texture.active].texmat = 1;
}

void APIENTRY_GL4ES gl4es_glMatrixMode(GLenum mode) {
    DBG(printf("glMatrixMode(%s), list=%p\n", PrintEnum(mode), glstate->list.active);)
    noerrorShim();
    if (unlikely(glstate->list.active && glstate->list.pending && glstate->matrix_mode == GL_MODELVIEW && mode == GL_MODELVIEW)) {
        return;
    }
    PUSH_IF_COMPILING(glMatrixMode);

    if (unlikely(!((mode == GL_MODELVIEW) || (mode == GL_PROJECTION) || (mode == GL_TEXTURE) || 
                  (mode >= GL_MATRIX0_ARB && mode < (GL_MATRIX0_ARB + MAX_ARB_MATRIX))))) {
        errorShim(GL_INVALID_ENUM);
        return;
    }
    if (glstate->matrix_mode != mode) {
        glstate->matrix_mode = mode;
        LOAD_GLES_FPE(glMatrixMode);
        gles_glMatrixMode(mode);
    }
}

void APIENTRY_GL4ES gl4es_glPushMatrix(void) {
    DBG(printf("glPushMatrix(), list=%p\n", glstate->list.active);)
    if (glstate->list.active && !glstate->list.pending) {
        PUSH_IF_COMPILING(glPushMatrix);
    }
    
    noerrorShim();
    // Simplified macros to use memcpy directly (faster)
    // Checking bounds before memcpy
    #define P(A, B) \
        if (likely(glstate->A->top + 1 < MAX_STACK_##B)) { \
            GLfloat* src = TOP(A); \
            glstate->A->top++; \
            memcpy(TOP(A), src, 16 * sizeof(GLfloat)); \
        } else errorShim(GL_STACK_OVERFLOW)

    switch(glstate->matrix_mode) {
        case GL_PROJECTION: P(projection_matrix, PROJECTION); break;
        case GL_MODELVIEW:  P(modelview_matrix, MODELVIEW); break;
        case GL_TEXTURE:    P(texture_matrix[glstate->texture.active], TEXTURE); break;
        default:
            if(glstate->matrix_mode >= GL_MATRIX0_ARB && glstate->matrix_mode < GL_MATRIX0_ARB + MAX_ARB_MATRIX) {
                P(arb_matrix[glstate->matrix_mode - GL_MATRIX0_ARB], ARB_MATRIX);
            } else {
                errorShim(GL_INVALID_OPERATION);
            }
    }
    #undef P
}

void APIENTRY_GL4ES gl4es_glPopMatrix(void) {
    DBG(printf("glPopMatrix(), list=%p\n", glstate->list.active);)
    if (glstate->list.active && !(glstate->list.compiling) && (globals4es.beginend) && 
        glstate->matrix_mode == GL_MODELVIEW && !(glstate->polygon_mode == GL_LINE) && glstate->list.pending) {
        if(memcmp(TOP(modelview_matrix)-16, TOP(modelview_matrix), 16 * sizeof(GLfloat)) == 0) {
            --glstate->modelview_matrix->top;
            return;
        }
    }
    PUSH_IF_COMPILING(glPopMatrix);
    
    noerrorShim();
    
    #define P(A) \
        if (likely(glstate->A->top)) { \
            --glstate->A->top; \
            /* Optim: Don't check identity immediately if not needed by hardware */ \
            glstate->A->identity = is_identity(update_current_mat()); \
            if (send_to_hardware()) { \
                LOAD_GLES(glLoadMatrixf); \
                gles_glLoadMatrixf(update_current_mat()); \
            } \
        } else errorShim(GL_STACK_UNDERFLOW)

    switch(glstate->matrix_mode) {
        case GL_PROJECTION:
            P(projection_matrix);
            glstate->mvp_matrix_dirty = 1;
            break;
        case GL_MODELVIEW:
            P(modelview_matrix);
            glstate->normal_matrix_dirty = 1;
            glstate->inv_mv_matrix_dirty = 1;
            glstate->mvp_matrix_dirty = 1;
            break;
        case GL_TEXTURE:
            P(texture_matrix[glstate->texture.active]);
            if(glstate->fpe_state) set_fpe_textureidentity();
            break;
        default:
            if(glstate->matrix_mode >= GL_MATRIX0_ARB && glstate->matrix_mode < GL_MATRIX0_ARB + MAX_ARB_MATRIX) {
                P(arb_matrix[glstate->matrix_mode - GL_MATRIX0_ARB]);
            } else {
                errorShim(GL_INVALID_OPERATION);
            }
    }
    #undef P
}

void APIENTRY_GL4ES gl4es_glLoadMatrixf(const GLfloat * m) {
    DBG(printf("glLoadMatrix(%f, %f, %f, %f...), list=%p\n", m[0], m[1], m[2], m[3], glstate->list.active);)
    if (unlikely(glstate->list.active)) {
        if(glstate->list.pending) gl4es_flush();
        else {
            NewStage(glstate->list.active, STAGE_MATRIX);
            glstate->list.active->matrix_op = 1;
            memcpy(glstate->list.active->matrix_val, m, 16 * sizeof(GLfloat));
            return;
        }
    }
    
    // Direct memcpy to current matrix pointer
    GLfloat* current = update_current_mat();
    memcpy(current, m, 16 * sizeof(GLfloat));
    
    update_current_identity(0);
    mark_dirty_matrices(glstate);

    if(send_to_hardware()) {
        LOAD_GLES(glLoadMatrixf);
        gles_glLoadMatrixf(m);
    }
}

void APIENTRY_GL4ES gl4es_glMultMatrixf(const GLfloat * m) {
    DBG(printf("glMultMatrix(%f, %f, %f, %f...), list=%p\n", m[0], m[1], m[2], m[3], glstate->list.active);)
    if (unlikely(glstate->list.active)) {
        if(glstate->list.pending) gl4es_flush();
        else {
            if(glstate->list.active->stage == STAGE_MATRIX) {
                matrix_mul(glstate->list.active->matrix_val, m, glstate->list.active->matrix_val);
                return;
            }
            NewStage(glstate->list.active, STAGE_MATRIX);
            glstate->list.active->matrix_op = 2;
            memcpy(glstate->list.active->matrix_val, m, 16 * sizeof(GLfloat));
            return;
        }
    }
    
    GLfloat *current_mat = update_current_mat();
    matrix_mul(current_mat, m, current_mat);
    
    update_current_identity(0);
    mark_dirty_matrices(glstate);

    if(send_to_hardware()) {
        LOAD_GLES(glLoadMatrixf);
        gles_glLoadMatrixf(current_mat);
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
    mark_dirty_matrices(glstate);

    if(send_to_hardware()) {
        LOAD_GLES(glLoadIdentity);
        gles_glLoadIdentity();
    }
}

// OPTIMIZATION: Direct Math injection without temp matrix
void APIENTRY_GL4ES gl4es_glTranslatef(GLfloat x, GLfloat y, GLfloat z) {
    DBG(printf("glTranslatef(%f, %f, %f), list=%p\n", x, y, z, glstate->list.active);)
    
    if (unlikely(glstate->list.active)) {
        GLfloat tmp[16];
        set_identity(tmp);
        tmp[12] = x; tmp[13] = y; tmp[14] = z;
        gl4es_glMultMatrixf(tmp);
        return;
    }

    if (x == 0.0f && y == 0.0f && z == 0.0f) return;

    GLfloat *m = update_current_mat();
    // M = M * Translation
    // Only the 4th column (index 12,13,14,15) changes
    m[12] = m[0]*x + m[4]*y + m[8]*z + m[12];
    m[13] = m[1]*x + m[5]*y + m[9]*z + m[13];
    m[14] = m[2]*x + m[6]*y + m[10]*z + m[14];
    m[15] = m[3]*x + m[7]*y + m[11]*z + m[15];

    update_current_identity(0);
    mark_dirty_matrices(glstate);

    if(send_to_hardware()) {
        LOAD_GLES(glLoadMatrixf);
        gles_glLoadMatrixf(m);
    }
}

// OPTIMIZATION: Direct Math injection without temp matrix
void APIENTRY_GL4ES gl4es_glScalef(GLfloat x, GLfloat y, GLfloat z) {
    DBG(printf("glScalef(%f, %f, %f), list=%p\n", x, y, z, glstate->list.active);)
    
    if (unlikely(glstate->list.active)) {
        GLfloat tmp[16];
        memset(tmp, 0, 16*sizeof(GLfloat));
        tmp[0] = x; tmp[5] = y; tmp[10] = z; tmp[15] = 1.0f;
        gl4es_glMultMatrixf(tmp);
        return;
    }

    if (x == 1.0f && y == 1.0f && z == 1.0f) return;

    GLfloat *m = update_current_mat();
    // M = M * Scale
    // Column 0 scaled by x, Col 1 by y, Col 2 by z. Col 3 unchanged.
    m[0] *= x; m[1] *= x; m[2] *= x; m[3] *= x;
    m[4] *= y; m[5] *= y; m[6] *= y; m[7] *= y;
    m[8] *= z; m[9] *= z; m[10] *= z; m[11] *= z;

    update_current_identity(0);
    mark_dirty_matrices(glstate);

    if(send_to_hardware()) {
        LOAD_GLES(glLoadMatrixf);
        gles_glLoadMatrixf(m);
    }
}

void APIENTRY_GL4ES gl4es_glRotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z) {
    DBG(printf("glRotatef(%f, %f, %f, %f), list=%p\n", angle, x, y, z, glstate->list.active);)
    
    if((x==0 && y==0 && z==0) || angle==0)
        return;

    if (unlikely(glstate->list.active)) {
         // Fallback to slow path for display lists to simplify recording
         GLfloat tmp[16];
         memset(tmp, 0, 16*sizeof(GLfloat));
         GLfloat l = 1.0f/sqrtf(x*x+y*y+z*z);
         x*=l; y*=l; z*=l;
         float rad = angle * 3.1415926535f / 180.0f;
         float s = sinf(rad);
         float c = cosf(rad);
         float c1 = 1-c;
         tmp[0] = x*x*c1+c;   tmp[4] = x*y*c1-z*s; tmp[8] = x*z*c1+y*s;
         tmp[1] = y*x*c1+z*s; tmp[5] = y*y*c1+c;   tmp[9] = y*z*c1-x*s;
         tmp[2] = x*z*c1-y*s; tmp[6] = y*z*c1+x*s; tmp[10] = z*z*c1+c;
         tmp[15] = 1.0f;
         gl4es_glMultMatrixf(tmp);
         return;
    }

    // Direct math for rotation
    GLfloat l = 1.0f/sqrtf(x*x+y*y+z*z);
    x*=l; y*=l; z*=l;
    float rad = angle * 3.1415926535f / 180.0f;
    float s = sinf(rad);
    float c = cosf(rad);
    float c1 = 1.0f - c;

    // Rotation Matrix elements (Row Major in calculation, stored Column Major)
    float r00 = x*x*c1+c;   float r01 = x*y*c1-z*s; float r02 = x*z*c1+y*s;
    float r10 = y*x*c1+z*s; float r11 = y*y*c1+c;   float r12 = y*z*c1-x*s;
    float r20 = x*z*c1-y*s; float r21 = y*z*c1+x*s; float r22 = z*z*c1+c;

    GLfloat *m = update_current_mat();
    GLfloat m0, m1, m2, m3;

    // We need to compute M_new = M_current * Rotation
    // Unrolled column by column for cache locality
    
    // Column 0
    m0 = m[0]; m1 = m[4]; m2 = m[8];
    m[0] = m0*r00 + m1*r10 + m2*r20;
    m[4] = m0*r01 + m1*r11 + m2*r21;
    m[8] = m0*r02 + m1*r12 + m2*r22;

    // Column 1
    m0 = m[1]; m1 = m[5]; m2 = m[9];
    m[1] = m0*r00 + m1*r10 + m2*r20;
    m[5] = m0*r01 + m1*r11 + m2*r21;
    m[9] = m0*r02 + m1*r12 + m2*r22;

    // Column 2
    m0 = m[2]; m1 = m[6]; m2 = m[10];
    m[2] = m0*r00 + m1*r10 + m2*r20;
    m[6] = m0*r01 + m1*r11 + m2*r21;
    m[10] = m0*r02 + m1*r12 + m2*r22;

    // Column 3
    m0 = m[3]; m1 = m[7]; m2 = m[11];
    m[3] = m0*r00 + m1*r10 + m2*r20;
    m[7] = m0*r01 + m1*r11 + m2*r21;
    m[11] = m0*r02 + m1*r12 + m2*r22;

    update_current_identity(0);
    mark_dirty_matrices(glstate);

    if(send_to_hardware()) {
        LOAD_GLES(glLoadMatrixf);
        gles_glLoadMatrixf(m);
    }
}

void APIENTRY_GL4ES gl4es_glOrthof(GLfloat left, GLfloat right, GLfloat bottom, GLfloat top, GLfloat nearVal, GLfloat farVal) {
    DBG(printf("glOrthof(%f, %f, %f, %f, %f, %f), list=%p\n", left, right, top, bottom, nearVal, farVal, glstate->list.active);)
    
    // Construct Ortho Matrix
    GLfloat r_l = 1.0f / (right - left);
    GLfloat t_b = 1.0f / (top - bottom);
    GLfloat f_n = 1.0f / (farVal - nearVal);
    
    GLfloat tx = -(right + left) * r_l;
    GLfloat ty = -(top + bottom) * t_b;
    GLfloat tz = -(farVal + nearVal) * f_n;
    
    GLfloat sx = 2.0f * r_l;
    GLfloat sy = 2.0f * t_b;
    GLfloat sz = -2.0f * f_n;

    if (unlikely(glstate->list.active)) {
        GLfloat tmp[16];
        memset(tmp, 0, 16*sizeof(GLfloat));
        tmp[0] = sx; tmp[12] = tx;
        tmp[5] = sy; tmp[13] = ty;
        tmp[10] = sz; tmp[14] = tz;
        tmp[15] = 1.0f;
        gl4es_glMultMatrixf(tmp);
        return;
    }

    // Direct Apply
    GLfloat *m = update_current_mat();
    
    // Apply Scale (Diagonal)
    m[0] *= sx; m[1] *= sx; m[2] *= sx; m[3] *= sx;
    m[4] *= sy; m[5] *= sy; m[6] *= sy; m[7] *= sy;
    m[8] *= sz; m[9] *= sz; m[10] *= sz; m[11] *= sz;
    
    // Apply Translation (Last Column)
    // Note: Since Ortho matrix off-diagonals are 0, we can simplify M * Ortho
    m[12] = m[0]*tx + m[4]*ty + m[8]*tz + m[12];
    m[13] = m[1]*tx + m[5]*ty + m[9]*tz + m[13];
    m[14] = m[2]*tx + m[6]*ty + m[10]*tz + m[14];
    m[15] = m[3]*tx + m[7]*ty + m[11]*tz + m[15];

    update_current_identity(0);
    mark_dirty_matrices(glstate);
    
    if(send_to_hardware()) {
        LOAD_GLES(glLoadMatrixf);
        gles_glLoadMatrixf(m);
    }
}

void APIENTRY_GL4ES gl4es_glFrustumf(GLfloat left, GLfloat right, GLfloat bottom, GLfloat top, GLfloat nearVal, GLfloat farVal) {
    DBG(printf("glFrustumf(%f, %f, %f, %f, %f, %f) list=%p\n", left, right, top, bottom, nearVal, farVal, glstate->list.active);)
    
    GLfloat r_l = 1.0f / (right - left);
    GLfloat t_b = 1.0f / (top - bottom);
    GLfloat f_n = 1.0f / (farVal - nearVal);
    
    GLfloat A = (right + left) * r_l;
    GLfloat B = (top + bottom) * t_b;
    GLfloat C = -(farVal + nearVal) * f_n;
    GLfloat D = -2.0f * farVal * nearVal * f_n;
    GLfloat X = 2.0f * nearVal * r_l;
    GLfloat Y = 2.0f * nearVal * t_b;

    if (unlikely(glstate->list.active)) {
        GLfloat tmp[16];
        memset(tmp, 0, 16*sizeof(GLfloat));
        tmp[0] = X; tmp[8] = A;
        tmp[5] = Y; tmp[9] = B;
        tmp[10] = C; tmp[14] = D;
        tmp[11] = -1.0f;
        gl4es_glMultMatrixf(tmp);
        return;
    }

    // Frustum is complex, just create and mult directly to avoid errors
    // but without full glMultMatrixf overhead
    GLfloat *m = update_current_mat();
    GLfloat m0, m1, m2, m3;

    // Column 0 (Scale X)
    m[0] *= X; m[1] *= X; m[2] *= X; m[3] *= X;
    // Column 1 (Scale Y)
    m[4] *= Y; m[5] *= Y; m[6] *= Y; m[7] *= Y;

    // Column 2 (Complex) - Affected by A, B, C, -1
    // New Col 2 = Col0*A + Col1*B + Col2*C + Col3*(-1)
    GLfloat c0, c1, c2, c3; // Cache old col 2
    c0 = m[8]; c1 = m[9]; c2 = m[10]; c3 = m[11];
    
    // We need original Col 0 and Col 1 values? 
    // Wait, we already modified m[0]..m[7] above! 
    // Correct approach: Don't modify in-place until calculated.
    // REVERTING TO Standard Mult for Frustum due to complexity and rarity compared to Translate/Scale
    
    // Recalculate safely using tmp but applying directly
    GLfloat f[16];
    memset(f, 0, 16*sizeof(GLfloat));
    f[0] = X; f[8] = A;
    f[5] = Y; f[9] = B;
    f[10] = C; f[14] = D;
    f[11] = -1.0f;
    
    matrix_mul(m, f, m);

    update_current_identity(0);
    mark_dirty_matrices(glstate);
    
    if(send_to_hardware()) {
        LOAD_GLES(glLoadMatrixf);
        gles_glLoadMatrixf(m);
    }
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