/*
 * Refactored matrix.c for GL4ES
 * Optimized for ARMv8
 * - In-place matrix operations (Translate/Scale)
 * - Static inline helpers for speed
 * - Fast Identity loading
 */

#include "matrix.h"
#include <string.h>
#include <math.h>

#include "../glx/hardext.h"
#include "debug.h"
#include "fpe.h"
#include "gl4es.h"
#include "glstate.h"
#include "init.h"
#include "loader.h"

#ifndef likely
#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)
#endif

//#define DEBUG
#ifdef DEBUG
#define DBG(a) a
#else
#define DBG(a)
#endif

// Constant Identity Matrix for fast memcpy
static const GLfloat c_identity[16] = {
    1.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 1.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 1.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 1.0f
};

void alloc_matrix(matrixstack_t **matrixstack, int depth) {
    *matrixstack = (matrixstack_t*)malloc(sizeof(matrixstack_t));
    (*matrixstack)->top = 0;
    (*matrixstack)->identity = 0;
    (*matrixstack)->stack = (GLfloat*)malloc(sizeof(GLfloat) * depth * 16);
}

#define TOP(A) (glstate->A->stack + (glstate->A->top * 16))

// Optimized: inline for hot path
static inline GLfloat* update_current_mat() {
    switch(glstate->matrix_mode) {
        case GL_MODELVIEW:
            return TOP(modelview_matrix);
        case GL_PROJECTION:
            return TOP(projection_matrix);
        case GL_TEXTURE:
            return TOP(texture_matrix[glstate->texture.active]);
        default:
            if (glstate->matrix_mode >= GL_MATRIX0_ARB && glstate->matrix_mode < GL_MATRIX0_ARB + MAX_ARB_MATRIX)
                return TOP(arb_matrix[glstate->matrix_mode - GL_MATRIX0_ARB]);
            return NULL;
    }
}

static inline int update_current_identity(int I) {
    int is_ident = (I) ? 1 : is_identity(update_current_mat());
    
    switch(glstate->matrix_mode) {
        case GL_MODELVIEW:
            return glstate->modelview_matrix->identity = is_ident;
        case GL_PROJECTION:
            return glstate->projection_matrix->identity = is_ident;
        case GL_TEXTURE:
            return glstate->texture_matrix[glstate->texture.active]->identity = is_ident;
        default:
            if (glstate->matrix_mode >= GL_MATRIX0_ARB && glstate->matrix_mode < GL_MATRIX0_ARB + MAX_ARB_MATRIX)
                return glstate->arb_matrix[glstate->matrix_mode - GL_MATRIX0_ARB]->identity = is_ident;
        return 0;
    }
}

static inline int send_to_hardware() {
    if (hardext.esversion > 1)
        return 0;
        
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
    memcpy(TOP(projection_matrix), c_identity, 16 * sizeof(GLfloat));
    glstate->projection_matrix->identity = 1;
    
    alloc_matrix(&glstate->modelview_matrix, MAX_STACK_MODELVIEW);
    memcpy(TOP(modelview_matrix), c_identity, 16 * sizeof(GLfloat));
    glstate->modelview_matrix->identity = 1;
    
    glstate->texture_matrix = (matrixstack_t**)malloc(sizeof(matrixstack_t*) * MAX_TEX);
    glstate->arb_matrix = (matrixstack_t**)malloc(sizeof(matrixstack_t*) * MAX_ARB_MATRIX);
    
    memcpy(glstate->mvp_matrix, c_identity, 16 * sizeof(GLfloat));
    glstate->mvp_matrix_dirty = 0;
    
    memcpy(glstate->inv_mv_matrix, c_identity, 16 * sizeof(GLfloat));
    glstate->inv_mv_matrix_dirty = 0;
    
    // Normal matrix is 3x3 packed in float[9]
    memset(glstate->normal_matrix, 0, 9 * sizeof(GLfloat));
    glstate->normal_matrix[0] = glstate->normal_matrix[4] = glstate->normal_matrix[8] = 1.0f;
    glstate->normal_matrix_dirty = 1;
    
    for (int i = 0; i < MAX_TEX; i++) {
        alloc_matrix(&glstate->texture_matrix[i], MAX_STACK_TEXTURE);
        memcpy(TOP(texture_matrix[i]), c_identity, 16 * sizeof(GLfloat));
        glstate->texture_matrix[i]->identity = 1;
    }
    
    for (int i = 0; i < MAX_ARB_MATRIX; i++) {
        alloc_matrix(&glstate->arb_matrix[i], MAX_STACK_ARB_MATRIX);
        memcpy(TOP(arb_matrix[i]), c_identity, 16 * sizeof(GLfloat));
        glstate->arb_matrix[i]->identity = 1;
    }
}

void set_fpe_textureidentity() {
    // Inverted logic in FPE flags? Kept original behavior.
    if (glstate->texture_matrix[glstate->texture.active]->identity)
        glstate->fpe_state->texture[glstate->texture.active].texmat = 0;
    else
        glstate->fpe_state->texture[glstate->texture.active].texmat = 1;
}

void APIENTRY_GL4ES gl4es_glMatrixMode(GLenum mode) {
    DBG(printf("glMatrixMode(%s), list=%p\n", PrintEnum(mode), glstate->list.active);)
    noerrorShim();
    
    // Redundancy check
    if (glstate->matrix_mode == mode) return;
    
    if (glstate->list.active && glstate->list.pending && glstate->matrix_mode == GL_MODELVIEW && mode == GL_MODELVIEW) {
        return; 
    }
    
    PUSH_IF_COMPILING(glMatrixMode);

    if (!((mode == GL_MODELVIEW) || (mode == GL_PROJECTION) || (mode == GL_TEXTURE) || 
          (mode >= GL_MATRIX0_ARB && mode < (GL_MATRIX0_ARB + MAX_ARB_MATRIX)))) {
        errorShim(GL_INVALID_ENUM);
        return;
    }
    
    glstate->matrix_mode = mode;
    
    // Hardware sync only if needed (ES1.1)
    if (hardext.esversion == 1) {
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
    
    #define P(A, B) \
        if (glstate->A->top + 1 < MAX_STACK_##B) { \
            GLfloat *top = TOP(A); \
            memcpy(top + 16, top, 16 * sizeof(GLfloat)); \
            glstate->A->top++; \
        } else errorShim(GL_STACK_OVERFLOW)

    switch(glstate->matrix_mode) {
        case GL_PROJECTION: P(projection_matrix, PROJECTION); break;
        case GL_MODELVIEW:  P(modelview_matrix, MODELVIEW); break;
        case GL_TEXTURE:    P(texture_matrix[glstate->texture.active], TEXTURE); break;
        default:
            if (glstate->matrix_mode >= GL_MATRIX0_ARB && glstate->matrix_mode < GL_MATRIX0_ARB + MAX_ARB_MATRIX) {
                P(arb_matrix[glstate->matrix_mode - GL_MATRIX0_ARB], ARB_MATRIX);
            } else {
                errorShim(GL_INVALID_OPERATION);
            }
    }
    #undef P
}

void APIENTRY_GL4ES gl4es_glPopMatrix(void) {
    DBG(printf("glPopMatrix(), list=%p\n", glstate->list.active);)
    
    if (glstate->list.active && !glstate->list.compiling && globals4es.beginend && 
        glstate->matrix_mode == GL_MODELVIEW && !(glstate->polygon_mode == GL_LINE) && 
        glstate->list.pending) {
        // Optimization: if pop restores exact same matrix, just dec counter
        if (memcmp(TOP(modelview_matrix) - 16, TOP(modelview_matrix), 16 * sizeof(GLfloat)) == 0) {
            --glstate->modelview_matrix->top;
            return;
        }
    }
    
    PUSH_IF_COMPILING(glPopMatrix);
    noerrorShim();

    #define P(A) \
        if (glstate->A->top) { \
            --glstate->A->top; \
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
            glstate->mvp_matrix_dirty = 1;
            glstate->inv_mv_matrix_dirty = 1;
            glstate->normal_matrix_dirty = 1;
            break;
        case GL_TEXTURE:
            P(texture_matrix[glstate->texture.active]);
            if (glstate->fpe_state) set_fpe_textureidentity();
            break;
        default:
            if (glstate->matrix_mode >= GL_MATRIX0_ARB && glstate->matrix_mode < GL_MATRIX0_ARB + MAX_ARB_MATRIX) {
                P(arb_matrix[glstate->matrix_mode - GL_MATRIX0_ARB]);
            } else {
                errorShim(GL_INVALID_OPERATION);
            }
    }
    #undef P
}

void APIENTRY_GL4ES gl4es_glLoadMatrixf(const GLfloat * m) {
    DBG(printf("glLoadMatrix(%f, ...), list=%p\n", m[0], glstate->list.active);)
    
    if (glstate->list.active) {
        if (glstate->list.pending) gl4es_flush();
        else {
            NewStage(glstate->list.active, STAGE_MATRIX);
            glstate->list.active->matrix_op = 1;
            memcpy(glstate->list.active->matrix_val, m, 16 * sizeof(GLfloat));
            return;
        }
    }
    
    memcpy(update_current_mat(), m, 16 * sizeof(GLfloat));
    const int id = update_current_identity(0);
    
    if (glstate->matrix_mode == GL_MODELVIEW) {
        glstate->normal_matrix_dirty = glstate->inv_mv_matrix_dirty = 1;
        glstate->mvp_matrix_dirty = 1;
    } else if (glstate->matrix_mode == GL_PROJECTION) {
        glstate->mvp_matrix_dirty = 1;
    } else if ((glstate->matrix_mode == GL_TEXTURE) && glstate->fpe_state) {
        set_fpe_textureidentity();
    }
    
    if (send_to_hardware()) {
        LOAD_GLES(glLoadMatrixf);
        if (id) {
            LOAD_GLES(glLoadIdentity);
            gles_glLoadIdentity();
        } else {
            gles_glLoadMatrixf(m);
        }
    }
}

void APIENTRY_GL4ES gl4es_glMultMatrixf(const GLfloat * m) {
    DBG(printf("glMultMatrix(%f, ...), list=%p\n", m[0], glstate->list.active);)
    
    if (glstate->list.active) {
        if (glstate->list.pending) gl4es_flush();
        else {
            if (glstate->list.active->stage == STAGE_MATRIX) {
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
    const int id = update_current_identity(0);
    
    if (glstate->matrix_mode == GL_MODELVIEW) {
        glstate->normal_matrix_dirty = glstate->inv_mv_matrix_dirty = 1;
        glstate->mvp_matrix_dirty = 1;
    } else if (glstate->matrix_mode == GL_PROJECTION) {
        glstate->mvp_matrix_dirty = 1;
    } else if ((glstate->matrix_mode == GL_TEXTURE) && glstate->fpe_state) {
        set_fpe_textureidentity();
    }
    
    if (send_to_hardware()) {
        LOAD_GLES(glLoadMatrixf);
        if (id) {
            LOAD_GLES(glLoadIdentity);
            gles_glLoadIdentity();
        } else {
            gles_glLoadMatrixf(current_mat);
        }
    }
}

void APIENTRY_GL4ES gl4es_glLoadIdentity(void) {
    DBG(printf("glLoadIdentity(), list=%p\n", glstate->list.active);)
    
    if (glstate->list.active) {
        if (glstate->list.pending) gl4es_flush();
        else {
            NewStage(glstate->list.active, STAGE_MATRIX);
            glstate->list.active->matrix_op = 1;
            set_identity(glstate->list.active->matrix_val);
            return;
        }
    }
    
    set_identity(update_current_mat());
    update_current_identity(1);
    
    if (glstate->matrix_mode == GL_MODELVIEW) {
        glstate->normal_matrix_dirty = glstate->inv_mv_matrix_dirty = 1;
        glstate->mvp_matrix_dirty = 1;
    } else if (glstate->matrix_mode == GL_PROJECTION) {
        glstate->mvp_matrix_dirty = 1;
    } else if ((glstate->matrix_mode == GL_TEXTURE) && glstate->fpe_state) {
        set_fpe_textureidentity();
    }
    
    if (send_to_hardware()) {
        LOAD_GLES(glLoadIdentity);
        gles_glLoadIdentity();
    }
}

void APIENTRY_GL4ES gl4es_glTranslatef(GLfloat x, GLfloat y, GLfloat z) {
    DBG(printf("glTranslatef(%f, %f, %f), list=%p\n", x, y, z, glstate->list.active);)
    
    // Optimization: In-place translation
    // If not compiling list, directly modify current matrix column 3
    if (!glstate->list.active) {
        GLfloat *m = update_current_mat();
        // M = M * T
        // Only column 3 changes:
        // m[12] = m[0]*x + m[4]*y + m[8]*z + m[12]
        // m[13] = m[1]*x + m[5]*y + m[9]*z + m[13]
        // m[14] = m[2]*x + m[6]*y + m[10]*z + m[14]
        // m[15] = m[3]*x + m[7]*y + m[11]*z + m[15]
        
        m[12] += m[0] * x + m[4] * y + m[8] * z;
        m[13] += m[1] * x + m[5] * y + m[9] * z;
        m[14] += m[2] * x + m[6] * y + m[10] * z;
        m[15] += m[3] * x + m[7] * y + m[11] * z;
        
        update_current_identity(0);
        
        if (glstate->matrix_mode == GL_MODELVIEW) {
            // Translate doesn't affect rotation/normal matrix part 3x3, only position
            glstate->inv_mv_matrix_dirty = 1;
            glstate->mvp_matrix_dirty = 1;
        } else if (glstate->matrix_mode == GL_PROJECTION) {
            glstate->mvp_matrix_dirty = 1;
        } else if ((glstate->matrix_mode == GL_TEXTURE) && glstate->fpe_state) {
            set_fpe_textureidentity();
        }
        
        if (send_to_hardware()) {
            LOAD_GLES(glTranslatef);
            gles_glTranslatef(x, y, z);
        }
        return;
    }

    // Fallback for display lists
    GLfloat tmp[16];
    set_identity(tmp);
    tmp[12] = x; tmp[13] = y; tmp[14] = z;
    gl4es_glMultMatrixf(tmp);
}

void APIENTRY_GL4ES gl4es_glScalef(GLfloat x, GLfloat y, GLfloat z) {
    DBG(printf("glScalef(%f, %f, %f), list=%p\n", x, y, z, glstate->list.active);)
    
    // Optimization: In-place scale
    if (!glstate->list.active) {
        GLfloat *m = update_current_mat();
        // Multiply Col 0 by x
        m[0] *= x; m[1] *= x; m[2] *= x; m[3] *= x;
        // Multiply Col 1 by y
        m[4] *= y; m[5] *= y; m[6] *= y; m[7] *= y;
        // Multiply Col 2 by z
        m[8] *= z; m[9] *= z; m[10] *= z; m[11] *= z;
        
        update_current_identity(0);
        
        if (glstate->matrix_mode == GL_MODELVIEW) {
            glstate->normal_matrix_dirty = glstate->inv_mv_matrix_dirty = 1;
            glstate->mvp_matrix_dirty = 1;
        } else if (glstate->matrix_mode == GL_PROJECTION) {
            glstate->mvp_matrix_dirty = 1;
        } else if ((glstate->matrix_mode == GL_TEXTURE) && glstate->fpe_state) {
            set_fpe_textureidentity();
        }
        
        if (send_to_hardware()) {
            LOAD_GLES(glScalef);
            gles_glScalef(x, y, z);
        }
        return;
    }

    GLfloat tmp[16];
    memset(tmp, 0, 16 * sizeof(GLfloat));
    tmp[0] = x; tmp[5] = y; tmp[10] = z; tmp[15] = 1.0f;
    gl4es_glMultMatrixf(tmp);
}

void APIENTRY_GL4ES gl4es_glRotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z) {
    DBG(printf("glRotatef(%f, %f, %f, %f), list=%p\n", angle, x, y, z, glstate->list.active);)
    
    if (angle == 0.0f) return;
    
    // create a rotation matrix than multiply it...
    GLfloat tmp[16];
    memset(tmp, 0, 16 * sizeof(GLfloat));
    
    if (x == 0.0f && y == 0.0f && z == 0.0f) return;
    
    // Normalize vector
    GLfloat mag = sqrtf(x*x + y*y + z*z);
    if (mag > 0.0f) {
        GLfloat l = 1.0f / mag;
        x *= l; y *= l; z *= l;
    }

    float rad = angle * 3.1415926535f / 180.f;
    float s = sinf(rad);
    float c = cosf(rad);
    float c1 = 1.0f - c;

    tmp[0] = x*x*c1 + c;     tmp[4] = x*y*c1 - z*s;   tmp[8] = x*z*c1 + y*s;
    tmp[1] = y*x*c1 + z*s;   tmp[5] = y*y*c1 + c;     tmp[9] = y*z*c1 - x*s;
    tmp[2] = x*z*c1 - y*s;   tmp[6] = y*z*c1 + x*s;   tmp[10] = z*z*c1 + c;
    tmp[15] = 1.0f;

    gl4es_glMultMatrixf(tmp);
}

void APIENTRY_GL4ES gl4es_glOrthof(GLfloat left, GLfloat right, GLfloat bottom, GLfloat top, GLfloat nearVal, GLfloat farVal) {
    DBG(printf("glOrthof(%f, %f, %f, %f, %f, %f), list=%p\n", left, right, top, bottom, nearVal, farVal, glstate->list.active);)
    
    GLfloat tmp[16];
    memset(tmp, 0, 16 * sizeof(GLfloat));

    tmp[0] = 2.0f / (right - left);
    tmp[12] = -(right + left) / (right - left);
    tmp[5] = 2.0f / (top - bottom);
    tmp[13] = -(top + bottom) / (top - bottom);
    tmp[10] = -2.0f / (farVal - nearVal);
    tmp[14] = -(farVal + nearVal) / (farVal - nearVal);
    tmp[15] = 1.0f;

    gl4es_glMultMatrixf(tmp);
}

void APIENTRY_GL4ES gl4es_glFrustumf(GLfloat left, GLfloat right, GLfloat bottom, GLfloat top, GLfloat nearVal, GLfloat farVal) {
    DBG(printf("glFrustumf(%f, %f, %f, %f, %f, %f) list=%p\n", left, right, top, bottom, nearVal, farVal, glstate->list.active);)
    
    GLfloat tmp[16];
    memset(tmp, 0, 16 * sizeof(GLfloat));

    GLfloat deltaX = right - left;
    GLfloat deltaY = top - bottom;
    GLfloat deltaZ = farVal - nearVal;

    tmp[0] = 2.0f * nearVal / deltaX;
    tmp[8] = (right + left) / deltaX;
    tmp[5] = 2.0f * nearVal / deltaY;
    tmp[9] = (top + bottom) / deltaY;
    tmp[10] = -(farVal + nearVal) / deltaZ;
    tmp[14] = -2.0f * farVal * nearVal / deltaZ;
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
