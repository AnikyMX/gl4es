/* src/gl/matrix.c
 * Modernized replacement for original matrix.c
 * - safer allocations (calloc/check)
 * - inline helpers to reduce repetition
 * - clearer control flow while preserving behavior
 */

#include "matrix.h"

#include "../glx/hardext.h"
#include "debug.h"
#include "fpe.h"
#include "gl4es.h"
#include "glstate.h"
#include "init.h"
#include "loader.h"

//#define DEBUG
#ifdef DEBUG
#define DBG(a) a
#else
#define DBG(a)
#endif

/* Helper: safely allocate a matrixstack and its storage */
void alloc_matrix(matrixstack_t **matrixstack, int depth) {
    if (!matrixstack) return;
    matrixstack_t *ms = (matrixstack_t*)calloc(1, sizeof(matrixstack_t));
    if (!ms) {
        LOGE("alloc_matrix: calloc failed for matrixstack_t\n");
        *matrixstack = NULL;
        return;
    }
    ms->top = 0;
    ms->identity = 0;
    /* allocate contiguous storage for 'depth' matrices of 16 floats */
    ms->stack = (GLfloat*)calloc((size_t)depth * 16, sizeof(GLfloat));
    if (!ms->stack) {
        LOGE("alloc_matrix: calloc failed for matrix stack storage\n");
        free(ms);
        *matrixstack = NULL;
        return;
    }
    *matrixstack = ms;
}

/* inline accessor for top matrix pointer */
static inline GLfloat* matrixstack_top_ptr(const matrixstack_t *ms) {
    if (!ms || !ms->stack) return NULL;
    return ms->stack + (ms->top * 16);
}

/* update_current_mat: returns pointer to the current matrix depending on matrix_mode */
static GLfloat* update_current_mat(void) {
    switch (glstate->matrix_mode) {
        case GL_MODELVIEW:
            return matrixstack_top_ptr(glstate->modelview_matrix);
        case GL_PROJECTION:
            return matrixstack_top_ptr(glstate->projection_matrix);
        case GL_TEXTURE:
            return matrixstack_top_ptr(glstate->texture_matrix[glstate->texture.active]);
        default:
            if (glstate->matrix_mode >= GL_MATRIX0_ARB &&
                glstate->matrix_mode < GL_MATRIX0_ARB + MAX_ARB_MATRIX) {
                int idx = glstate->matrix_mode - GL_MATRIX0_ARB;
                return matrixstack_top_ptr(glstate->arb_matrix[idx]);
            }
            return NULL;
    }
}

/* update_current_identity: update and return identity status for current matrix */
static int update_current_identity(int force_identity_flag) {
    switch (glstate->matrix_mode) {
        case GL_MODELVIEW:
            return glstate->modelview_matrix->identity =
                (force_identity_flag) ? 1 : is_identity(matrixstack_top_ptr(glstate->modelview_matrix));
        case GL_PROJECTION:
            return glstate->projection_matrix->identity =
                (force_identity_flag) ? 1 : is_identity(matrixstack_top_ptr(glstate->projection_matrix));
        case GL_TEXTURE: {
            matrixstack_t *ms = glstate->texture_matrix[glstate->texture.active];
            return ms->identity = (force_identity_flag) ? 1 : is_identity(matrixstack_top_ptr(ms));
        }
        default:
            if (glstate->matrix_mode >= GL_MATRIX0_ARB &&
                glstate->matrix_mode < GL_MATRIX0_ARB + MAX_ARB_MATRIX) {
                int idx = glstate->matrix_mode - GL_MATRIX0_ARB;
                matrixstack_t *ms = glstate->arb_matrix[idx];
                return ms->identity = (force_identity_flag) ? 1 : is_identity(matrixstack_top_ptr(ms));
            }
            return 0;
    }
}

/* decide whether to send matrix to GPU hardware:
 * keep original logic: esversion > 1 => skip; projection & modelview => yes; texture depends on globals4es.texmat
 */
static int send_to_hardware(void) {
    if (hardext.esversion > 1) return 0;
    switch (glstate->matrix_mode) {
        case GL_PROJECTION:
        case GL_MODELVIEW:
            return 1;
        case GL_TEXTURE:
            return (globals4es.texmat) ? 1 : 0;
        default:
            return 0;
    }
}

/* Initialize matrix subsystem.
 * Note: keep behavior consistent with original (allocate arrays, set identity, set dirty flags).
 */
void init_matrix(glstate_t* glstate) {
    DBG(printf("init_matrix(%p)\n", glstate);)

    /* Projection */
    alloc_matrix(&glstate->projection_matrix, MAX_STACK_PROJECTION);
    if (glstate->projection_matrix && matrixstack_top_ptr(glstate->projection_matrix)) {
        set_identity(matrixstack_top_ptr(glstate->projection_matrix));
        glstate->projection_matrix->identity = 1;
    }

    /* Modelview */
    alloc_matrix(&glstate->modelview_matrix, MAX_STACK_MODELVIEW);
    if (glstate->modelview_matrix && matrixstack_top_ptr(glstate->modelview_matrix)) {
        set_identity(matrixstack_top_ptr(glstate->modelview_matrix));
        glstate->modelview_matrix->identity = 1;
    }

    /* Texture and ARB matrices arrays */
    glstate->texture_matrix = (matrixstack_t**)calloc(MAX_TEX, sizeof(matrixstack_t*));
    glstate->arb_matrix = (matrixstack_t**)calloc(MAX_ARB_MATRIX, sizeof(matrixstack_t*));

    /* MVP / inverse matrix init */
    set_identity(glstate->mvp_matrix);
    glstate->mvp_matrix_dirty = 0;
    set_identity(glstate->inv_mv_matrix);
    glstate->inv_mv_matrix_dirty = 0;

    /* normal matrix (3x3) */
    memset(glstate->normal_matrix, 0, 9 * sizeof(GLfloat));
    glstate->normal_matrix[0] = glstate->normal_matrix[4] = glstate->normal_matrix[8] = 1.0f;
    glstate->normal_matrix_dirty = 1;

    for (int i = 0; i < MAX_TEX; ++i) {
        alloc_matrix(&glstate->texture_matrix[i], MAX_STACK_TEXTURE);
        if (glstate->texture_matrix[i] && matrixstack_top_ptr(glstate->texture_matrix[i])) {
            set_identity(matrixstack_top_ptr(glstate->texture_matrix[i]));
            glstate->texture_matrix[i]->identity = 1;
        }
    }
    for (int i = 0; i < MAX_ARB_MATRIX; ++i) {
        alloc_matrix(&glstate->arb_matrix[i], MAX_STACK_ARB_MATRIX);
        if (glstate->arb_matrix[i] && matrixstack_top_ptr(glstate->arb_matrix[i])) {
            set_identity(matrixstack_top_ptr(glstate->arb_matrix[i]));
            glstate->arb_matrix[i]->identity = 1;
        }
    }
}

/* Update FPE texture matrix flag depending on current texture matrix identity */
void set_fpe_textureidentity(void) {
    if (!glstate->fpe_state) return;
    /* original logic: inverted in fpe flags */
    if (glstate->texture_matrix[glstate->texture.active]->identity)
        glstate->fpe_state->texture[glstate->texture.active].texmat = 0;
    else
        glstate->fpe_state->texture[glstate->texture.active].texmat = 1;
}

/* ----------------- OpenGL API implementations ----------------- */

void APIENTRY_GL4ES gl4es_glMatrixMode(GLenum mode) {
    DBG(printf("glMatrixMode(%s), list=%p\n", PrintEnum(mode), glstate->list.active);)
    noerrorShim();

    if (glstate->list.active && glstate->list.pending &&
        glstate->matrix_mode == GL_MODELVIEW && mode == GL_MODELVIEW) {
        return; /* nothing to do */
    }
    PUSH_IF_COMPILING(glMatrixMode);

    if (!((mode == GL_MODELVIEW) || (mode == GL_PROJECTION) || (mode == GL_TEXTURE) ||
          (mode >= GL_MATRIX0_ARB && mode < (GL_MATRIX0_ARB + MAX_ARB_MATRIX)))) {
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
    const GLint matrix_mode = glstate->matrix_mode;

    /* push semantics: duplicate top -> increment top */
    switch (matrix_mode) {
        case GL_PROJECTION: {
            matrixstack_t *ms = glstate->projection_matrix;
            if (ms && ms->top + 1 < MAX_STACK_PROJECTION) {
                GLfloat *src = matrixstack_top_ptr(ms);
                GLfloat *dst = ms->stack + ((ms->top + 1) * 16);
                memcpy(dst, src, 16 * sizeof(GLfloat));
                ms->top++;
            } else {
                errorShim(GL_STACK_OVERFLOW);
            }
            break;
        }
        case GL_MODELVIEW: {
            matrixstack_t *ms = glstate->modelview_matrix;
            if (ms && ms->top + 1 < MAX_STACK_MODELVIEW) {
                GLfloat *src = matrixstack_top_ptr(ms);
                GLfloat *dst = ms->stack + ((ms->top + 1) * 16);
                memcpy(dst, src, 16 * sizeof(GLfloat));
                ms->top++;
            } else {
                errorShim(GL_STACK_OVERFLOW);
            }
            break;
        }
        case GL_TEXTURE: {
            int a = glstate->texture.active;
            matrixstack_t *ms = glstate->texture_matrix[a];
            if (ms && ms->top + 1 < MAX_STACK_TEXTURE) {
                GLfloat *src = matrixstack_top_ptr(ms);
                GLfloat *dst = ms->stack + ((ms->top + 1) * 16);
                memcpy(dst, src, 16 * sizeof(GLfloat));
                ms->top++;
            } else {
                errorShim(GL_STACK_OVERFLOW);
            }
            break;
        }
        default:
            if (matrix_mode >= GL_MATRIX0_ARB && matrix_mode < GL_MATRIX0_ARB + MAX_ARB_MATRIX) {
                int idx = matrix_mode - GL_MATRIX0_ARB;
                matrixstack_t *ms = glstate->arb_matrix[idx];
                if (ms && ms->top + 1 < MAX_STACK_ARB_MATRIX) {
                    GLfloat *src = matrixstack_top_ptr(ms);
                    GLfloat *dst = ms->stack + ((ms->top + 1) * 16);
                    memcpy(dst, src, 16 * sizeof(GLfloat));
                    ms->top++;
                } else {
                    errorShim(GL_STACK_OVERFLOW);
                }
            } else {
                errorShim(GL_INVALID_OPERATION);
            }
    }
}

void APIENTRY_GL4ES gl4es_glPopMatrix(void) {
    DBG(printf("glPopMatrix(), list=%p\n", glstate->list.active);)

    /* Fast path check (original behavior) for display list optim */
    if (glstate->list.active &&
        !(glstate->list.compiling) &&
        (globals4es.beginend) &&
        glstate->matrix_mode == GL_MODELVIEW &&
        !(glstate->polygon_mode == GL_LINE) &&
        glstate->list.pending) {
        /* if popped matrix equals current top, just decrement and return */
        matrixstack_t *ms = glstate->modelview_matrix;
        if (ms && ms->top > 0) {
            GLfloat *cur = matrixstack_top_ptr(ms);
            GLfloat *prev = cur - 16;
            if (memcmp(prev, cur, 16 * sizeof(GLfloat)) == 0) {
                ms->top--;
                return;
            }
        }
    }

    PUSH_IF_COMPILING(glPopMatrix);
    noerrorShim();
    const GLint matrix_mode = glstate->matrix_mode;

    switch (matrix_mode) {
        case GL_PROJECTION: {
            matrixstack_t *ms = glstate->projection_matrix;
            if (ms && ms->top > 0) {
                ms->top--;
                glstate->projection_matrix->identity = is_identity(matrixstack_top_ptr(ms));
                if (send_to_hardware()) {
                    LOAD_GLES(glLoadMatrixf);
                    gles_glLoadMatrixf(matrixstack_top_ptr(ms));
                }
                glstate->mvp_matrix_dirty = 1;
            } else {
                errorShim(GL_STACK_UNDERFLOW);
            }
            break;
        }
        case GL_MODELVIEW: {
            matrixstack_t *ms = glstate->modelview_matrix;
            if (ms && ms->top > 0) {
                ms->top--;
                glstate->modelview_matrix->identity = is_identity(matrixstack_top_ptr(ms));
                if (send_to_hardware()) {
                    LOAD_GLES(glLoadMatrixf);
                    gles_glLoadMatrixf(matrixstack_top_ptr(ms));
                }
                glstate->mvp_matrix_dirty = 1;
                glstate->inv_mv_matrix_dirty = 1;
                glstate->normal_matrix_dirty = 1;
            } else {
                errorShim(GL_STACK_UNDERFLOW);
            }
            break;
        }
        case GL_TEXTURE: {
            int a = glstate->texture.active;
            matrixstack_t *ms = glstate->texture_matrix[a];
            if (ms && ms->top > 0) {
                ms->top--;
                ms->identity = is_identity(matrixstack_top_ptr(ms));
                if (send_to_hardware()) {
                    LOAD_GLES(glLoadMatrixf);
                    gles_glLoadMatrixf(matrixstack_top_ptr(ms));
                }
                if (glstate->fpe_state) set_fpe_textureidentity();
            } else {
                errorShim(GL_STACK_UNDERFLOW);
            }
            break;
        }
        default:
            if (matrix_mode >= GL_MATRIX0_ARB && matrix_mode < GL_MATRIX0_ARB + MAX_ARB_MATRIX) {
                int idx = matrix_mode - GL_MATRIX0_ARB;
                matrixstack_t *ms = glstate->arb_matrix[idx];
                if (ms && ms->top > 0) {
                    ms->top--;
                } else {
                    errorShim(GL_STACK_UNDERFLOW);
                }
            } else {
                errorShim(GL_INVALID_OPERATION);
            }
    }
}

void APIENTRY_GL4ES gl4es_glLoadMatrixf(const GLfloat * m) {
    DBG(printf("glLoadMatrix(%f, %f, %f, %f, %f, %f, %f...), list=%p\n",
               m ? m[0] : 0.0f, m ? m[1] : 0.0f, m ? m[2] : 0.0f, m ? m[3] : 0.0f,
               m ? m[4] : 0.0f, m ? m[5] : 0.0f, m ? m[6] : 0.0f, glstate->list.active);)

    if (!m) return; /* guard */

    if (glstate->list.active) {
        if (glstate->list.pending) gl4es_flush();
        else {
            NewStage(glstate->list.active, STAGE_MATRIX);
            glstate->list.active->matrix_op = 1;
            memcpy(glstate->list.active->matrix_val, m, 16 * sizeof(GLfloat));
            return;
        }
    }

    GLfloat *dst = update_current_mat();
    if (!dst) return;
    memcpy(dst, m, 16 * sizeof(GLfloat));

    (void)update_current_identity(0);

    if (glstate->matrix_mode == GL_MODELVIEW)
        glstate->normal_matrix_dirty = glstate->inv_mv_matrix_dirty = 1;

    if (glstate->matrix_mode == GL_MODELVIEW || glstate->matrix_mode == GL_PROJECTION)
        glstate->mvp_matrix_dirty = 1;
    else if ((glstate->matrix_mode == GL_TEXTURE) && glstate->fpe_state)
        set_fpe_textureidentity();

    if (send_to_hardware()) {
        LOAD_GLES(glLoadMatrixf);
        LOAD_GLES(glLoadIdentity);
        if (is_identity(dst)) gles_glLoadIdentity();
        else gles_glLoadMatrixf(dst);
    }
}

void APIENTRY_GL4ES gl4es_glMultMatrixf(const GLfloat * m) {
    DBG(printf("glMultMatrix(%f, %f, %f, %f, %f, %f, %f...), list=%p\n",
               m ? m[0] : 0.0f, m ? m[1] : 0.0f, m ? m[2] : 0.0f, m ? m[3] : 0.0f,
               m ? m[4] : 0.0f, m ? m[5] : 0.0f, m ? m[6] : 0.0f, glstate->list.active);)

    if (!m) return;

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
    if (!current_mat) return;
    matrix_mul(current_mat, m, current_mat);

    (void)update_current_identity(0);

    if (glstate->matrix_mode == GL_MODELVIEW)
        glstate->normal_matrix_dirty = glstate->inv_mv_matrix_dirty = 1;

    if (glstate->matrix_mode == GL_MODELVIEW || glstate->matrix_mode == GL_PROJECTION)
        glstate->mvp_matrix_dirty = 1;
    else if ((glstate->matrix_mode == GL_TEXTURE) && glstate->fpe_state)
        set_fpe_textureidentity();

    DBG(printf(" => (%f, %f, %f, %f, %f, %f, %f...)\n",
               current_mat[0], current_mat[1], current_mat[2], current_mat[3],
               current_mat[4], current_mat[5], current_mat[6]);)

    if (send_to_hardware()) {
        LOAD_GLES(glLoadMatrixf);
        LOAD_GLES(glLoadIdentity);
        if (is_identity(current_mat)) gles_glLoadIdentity();
        else gles_glLoadMatrixf(current_mat);
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

    GLfloat *m = update_current_mat();
    if (!m) return;
    set_identity(m);
    update_current_identity(1);

    if (glstate->matrix_mode == GL_MODELVIEW)
        glstate->normal_matrix_dirty = glstate->inv_mv_matrix_dirty = 1;

    if (glstate->matrix_mode == GL_MODELVIEW || glstate->matrix_mode == GL_PROJECTION)
        glstate->mvp_matrix_dirty = 1;
    else if ((glstate->matrix_mode == GL_TEXTURE) && glstate->fpe_state)
        set_fpe_textureidentity();

    if (send_to_hardware()) {
        LOAD_GLES(glLoadIdentity);
        gles_glLoadIdentity();
    }
}

void APIENTRY_GL4ES gl4es_glTranslatef(GLfloat x, GLfloat y, GLfloat z) {
    DBG(printf("glTranslatef(%f, %f, %f), list=%p\n", x, y, z, glstate->list.active);)
    GLfloat tmp[16];
    set_identity(tmp);
    tmp[12 + 0] = x;
    tmp[12 + 1] = y;
    tmp[12 + 2] = z;
    gl4es_glMultMatrixf(tmp);
}

void APIENTRY_GL4ES gl4es_glScalef(GLfloat x, GLfloat y, GLfloat z) {
    DBG(printf("glScalef(%f, %f, %f), list=%p\n", x, y, z, glstate->list.active);)
    GLfloat tmp[16];
    memset(tmp, 0, 16 * sizeof(GLfloat));
    tmp[0 + 0] = x;
    tmp[1 + 4] = y;
    tmp[2 + 8] = z;
    tmp[3 + 12] = 1.0f;
    gl4es_glMultMatrixf(tmp);
}

void APIENTRY_GL4ES gl4es_glRotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z) {
    DBG(printf("glRotatef(%f, %f, %f, %f), list=%p\n", angle, x, y, z, glstate->list.active);)
    if ((x == 0 && y == 0 && z == 0) || angle == 0.0f) return;

    GLfloat tmp[16];
    memset(tmp, 0, 16 * sizeof(GLfloat));

    /* normalize axis */
    GLfloat len = sqrtf(x * x + y * y + z * z);
    if (len == 0.0f) return;
    x /= len; y /= len; z /= len;

    angle *= 3.1415926535f / 180.0f;
    const GLfloat s = sinf(angle);
    const GLfloat c = cosf(angle);
    const GLfloat c1 = 1.0f - c;

    tmp[0 + 0] = x * x * c1 + c;     tmp[0 + 4] = x * y * c1 - z * s; tmp[0 + 8] = x * z * c1 + y * s;
    tmp[1 + 0] = y * x * c1 + z * s; tmp[1 + 4] = y * y * c1 + c;     tmp[1 + 8] = y * z * c1 - x * s;
    tmp[2 + 0] = x * z * c1 - y * s; tmp[2 + 4] = y * z * c1 + x * s; tmp[2 + 8] = z * z * c1 + c;

    tmp[3 + 12] = 1.0f;

    gl4es_glMultMatrixf(tmp);
}

void APIENTRY_GL4ES gl4es_glOrthof(GLfloat left, GLfloat right, GLfloat bottom, GLfloat top, GLfloat nearVal, GLfloat farVal) {
    DBG(printf("glOrthof(%f, %f, %f, %f, %f, %f), list=%p\n", left, right, top, bottom, nearVal, farVal, glstate->list.active);)
    GLfloat tmp[16];
    memset(tmp, 0, 16 * sizeof(GLfloat));

    tmp[0 + 0] = 2.0f / (right - left);     tmp[0 + 12] = -(right + left) / (right - left);
    tmp[1 + 4] = 2.0f / (top - bottom);     tmp[1 + 12] = -(top + bottom) / (top - bottom);
    tmp[2 + 8] = -2.0f / (farVal - nearVal); tmp[2 + 12] = -(farVal + nearVal) / (farVal - nearVal);
                                             tmp[3 + 12] = 1.0f;

    gl4es_glMultMatrixf(tmp);
}

void APIENTRY_GL4ES gl4es_glFrustumf(GLfloat left, GLfloat right, GLfloat bottom, GLfloat top, GLfloat nearVal, GLfloat farVal) {
    DBG(printf("glFrustumf(%f, %f, %f, %f, %f, %f) list=%p\n", left, right, top, bottom, nearVal, farVal, glstate->list.active);)
    GLfloat tmp[16];
    memset(tmp, 0, 16 * sizeof(GLfloat));

    tmp[0 + 0] = 2.0f * nearVal / (right - left); tmp[0 + 8] = (right + left) / (right - left);
    tmp[1 + 4] = 2.0f * nearVal / (top - bottom);  tmp[1 + 8] = (top + bottom) / (top - bottom);
    tmp[2 + 8] = -(farVal + nearVal) / (farVal - nearVal); tmp[2 + 12] = -2.0f * farVal * nearVal / (farVal - nearVal);
    tmp[3 + 8] = -1.0f;

    gl4es_glMultMatrixf(tmp);
}

/* keep exports compatible with existing project */
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