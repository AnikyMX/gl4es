/*
 * Modernized matrix.c for GL4ES
 * Focus: Reducing overhead in Translate/Scale/Rotate & SIMD-friendly access
 */

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

// Hints untuk Branch Prediction (Optimasi CPU Pipeline)
#define LIKELY(x)   __builtin_expect(!!(x), 1)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)

void alloc_matrix(matrixstack_t **matrixstack, int depth) {
	*matrixstack = (matrixstack_t*)malloc(sizeof(matrixstack_t));
	(*matrixstack)->top = 0;
	(*matrixstack)->identity = 0;
	(*matrixstack)->stack = (GLfloat*)malloc(sizeof(GLfloat)*depth*16);
}

#define TOP(A) (glstate->A->stack+(glstate->A->top*16))

// Inline untuk kecepatan akses
static inline GLfloat* update_current_mat() {
	switch(glstate->matrix_mode) {
		case GL_MODELVIEW:
			return TOP(modelview_matrix);
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
	switch(glstate->matrix_mode) {
		case GL_MODELVIEW:
			return glstate->modelview_matrix->identity = (I)?1:is_identity(TOP(modelview_matrix));
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
	switch(glstate->matrix_mode) {
		case GL_PROJECTION:
			return 1;
		case GL_MODELVIEW:
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
	// no identity function for 3x3 matrix
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
	if(glstate->texture_matrix[glstate->texture.active]->identity)	// inverted in fpe flags
		glstate->fpe_state->texture[glstate->texture.active].texmat = 0;
	else
		glstate->fpe_state->texture[glstate->texture.active].texmat = 1;
}

void APIENTRY_GL4ES gl4es_glMatrixMode(GLenum mode) {
DBG(printf("glMatrixMode(%s), list=%p\n", PrintEnum(mode), glstate->list.active);)
	noerrorShim();
	if (UNLIKELY(glstate->list.active && glstate->list.pending && glstate->matrix_mode==GL_MODELVIEW && mode==GL_MODELVIEW)) {
		return;	// nothing to do...
	}
	PUSH_IF_COMPILING(glMatrixMode);

	if(!((mode==GL_MODELVIEW) || (mode==GL_PROJECTION) || (mode==GL_TEXTURE) || (mode>=GL_MATRIX0_ARB && mode<(GL_MATRIX0_ARB+MAX_ARB_MATRIX)))) {
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
	if (UNLIKELY(glstate->list.active && !glstate->list.pending)) {
		PUSH_IF_COMPILING(glPushMatrix);
	}
	// get matrix mode
	GLint matrix_mode = glstate->matrix_mode;
	noerrorShim();
	// go...
	switch(matrix_mode) {
		#define P(A, B) if(LIKELY(glstate->A->top+1<MAX_STACK_##B)) { \
			memcpy(TOP(A)+16, TOP(A), 16*sizeof(GLfloat)); \
			glstate->A->top++; \
		} else errorShim(GL_STACK_OVERFLOW)
		case GL_PROJECTION:
			P(projection_matrix, PROJECTION);
			break;
		case GL_MODELVIEW:
			P(modelview_matrix, MODELVIEW);
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
	if (UNLIKELY(glstate->list.active 
	 && !(glstate->list.compiling)
	 && (globals4es.beginend) 
	 && glstate->matrix_mode==GL_MODELVIEW
	 && !(glstate->polygon_mode==GL_LINE) 
	 && glstate->list.pending)) {
		// check if pop'd matrix is the same as actual...
		if(memcmp(TOP(modelview_matrix)-16, TOP(modelview_matrix), 16*sizeof(GLfloat))==0) {
			--glstate->modelview_matrix->top;
			return;
		}
	}
	PUSH_IF_COMPILING(glPopMatrix);
	// get matrix mode
	GLint matrix_mode = glstate->matrix_mode;
	// go...
	noerrorShim();
	switch(matrix_mode) {
		#define P(A) if(LIKELY(glstate->A->top)) { \
			--glstate->A->top; \
			glstate->A->identity = is_identity(update_current_mat()); \
			if (send_to_hardware()) {LOAD_GLES(glLoadMatrixf); gles_glLoadMatrixf(update_current_mat()); } \
		} else errorShim(GL_STACK_UNDERFLOW)
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
	if (UNLIKELY(glstate->list.active)) {
		if(glstate->list.pending) gl4es_flush();
		else {
			NewStage(glstate->list.active, STAGE_MATRIX);
			glstate->list.active->matrix_op = 1;
			memcpy(glstate->list.active->matrix_val, m, 16*sizeof(GLfloat));
			return;
		}
	}
	// Direct memcpy, compiler optimize will use NEON
	memcpy(update_current_mat(), m, 16*sizeof(GLfloat));
	
	const int id = update_current_identity(0);
	if(glstate->matrix_mode==GL_MODELVIEW)
		glstate->normal_matrix_dirty = glstate->inv_mv_matrix_dirty = 1;
	if(glstate->matrix_mode==GL_MODELVIEW || glstate->matrix_mode==GL_PROJECTION)
		glstate->mvp_matrix_dirty = 1;
	else if((glstate->matrix_mode==GL_TEXTURE) && glstate->fpe_state)
		set_fpe_textureidentity();
    if(send_to_hardware()) {
		LOAD_GLES(glLoadMatrixf);
		LOAD_GLES(glLoadIdentity);
		if(id) gles_glLoadIdentity();
		else gles_glLoadMatrixf(m);
	}
}

void APIENTRY_GL4ES gl4es_glMultMatrixf(const GLfloat * m) {
DBG(printf("glMultMatrix(%f, %f, %f, %f, %f, %f, %f...), list=%p\n", m[0], m[1], m[2], m[3], m[4], m[5], m[6], glstate->list.active);)
	if (UNLIKELY(glstate->list.active)) {
		if(glstate->list.pending) gl4es_flush();
		else {
			if(glstate->list.active->stage == STAGE_MATRIX) {
				// multiply the matrix mith the current one....
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
	if(glstate->matrix_mode==GL_MODELVIEW)
		glstate->normal_matrix_dirty = glstate->inv_mv_matrix_dirty = 1;
	if(glstate->matrix_mode==GL_MODELVIEW || glstate->matrix_mode==GL_PROJECTION)
		glstate->mvp_matrix_dirty = 1;
	else if((glstate->matrix_mode==GL_TEXTURE) && glstate->fpe_state)
		set_fpe_textureidentity();
	
	if(send_to_hardware()) {
		LOAD_GLES(glLoadMatrixf);
		LOAD_GLES(glLoadIdentity);
		if(id) gles_glLoadIdentity();
		else gles_glLoadMatrixf(current_mat);
	}
}

void APIENTRY_GL4ES gl4es_glLoadIdentity(void) {
DBG(printf("glLoadIdentity(), list=%p\n", glstate->list.active);)
	if (UNLIKELY(glstate->list.active)) {
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
	if(glstate->matrix_mode==GL_MODELVIEW)
		glstate->normal_matrix_dirty = glstate->inv_mv_matrix_dirty = 1;
	if(glstate->matrix_mode==GL_MODELVIEW || glstate->matrix_mode==GL_PROJECTION)
		glstate->mvp_matrix_dirty = 1;
	else if((glstate->matrix_mode==GL_TEXTURE) && glstate->fpe_state)
		set_fpe_textureidentity();
	if(send_to_hardware()) {
		LOAD_GLES(glLoadIdentity);
		gles_glLoadIdentity();
	}
}

// OPTIMIZED: Translate Direct Modify
void APIENTRY_GL4ES gl4es_glTranslatef(GLfloat x, GLfloat y, GLfloat z) {
DBG(printf("glTranslatef(%f, %f, %f), list=%p\n", x, y, z, glstate->list.active);)
    
    // Fallback untuk Display List (jarang di frame-loop utama game modern)
    if (UNLIKELY(glstate->list.active)) {
        GLfloat tmp[16];
        set_identity(tmp);
        tmp[12] = x; tmp[13] = y; tmp[14] = z;
        gl4es_glMultMatrixf(tmp);
        return;
    }

    // FAST PATH: Modify column 3 directly
    // M = M * T.
    // Kolom 3 baru = M * [x, y, z, 1]^T
    // = x*Col0 + y*Col1 + z*Col2 + Col3
    GLfloat *m = update_current_mat();
    
    m[12] = m[0]*x + m[4]*y + m[8]*z + m[12];
    m[13] = m[1]*x + m[5]*y + m[9]*z + m[13];
    m[14] = m[2]*x + m[6]*y + m[10]*z + m[14];
    m[15] = m[3]*x + m[7]*y + m[11]*z + m[15];

    // Update dirty flags
    update_current_identity(0);
    if(glstate->matrix_mode==GL_MODELVIEW)
        glstate->normal_matrix_dirty = glstate->inv_mv_matrix_dirty = 1;
    if(glstate->matrix_mode==GL_MODELVIEW || glstate->matrix_mode==GL_PROJECTION)
        glstate->mvp_matrix_dirty = 1;
    
    // GLES Hardware Sync
    if(send_to_hardware()) {
        LOAD_GLES(glLoadMatrixf);
        gles_glLoadMatrixf(m);
    }
}

// OPTIMIZED: Scale Direct Modify
void APIENTRY_GL4ES gl4es_glScalef(GLfloat x, GLfloat y, GLfloat z) {
DBG(printf("glScalef(%f, %f, %f), list=%p\n", x, y, z, glstate->list.active);)
    
    if (UNLIKELY(glstate->list.active)) {
        GLfloat tmp[16];
        memset(tmp, 0, 16*sizeof(GLfloat));
        tmp[0] = x; tmp[5] = y; tmp[10] = z; tmp[15] = 1.0f;
        gl4es_glMultMatrixf(tmp);
        return;
    }

    // FAST PATH: M = M * S
    // Hanya mengalikan kolom 0, 1, 2 dengan x, y, z
    GLfloat *m = update_current_mat();
    
    m[0] *= x; m[1] *= x; m[2] *= x; m[3] *= x;
    m[4] *= y; m[5] *= y; m[6] *= y; m[7] *= y;
    m[8] *= z; m[9] *= z; m[10] *= z; m[11] *= z;

    update_current_identity(0);
    if(glstate->matrix_mode==GL_MODELVIEW)
        glstate->normal_matrix_dirty = glstate->inv_mv_matrix_dirty = 1;
    if(glstate->matrix_mode==GL_MODELVIEW || glstate->matrix_mode==GL_PROJECTION)
        glstate->mvp_matrix_dirty = 1;

    if(send_to_hardware()) {
        LOAD_GLES(glLoadMatrixf);
        gles_glLoadMatrixf(m);
    }
}

void APIENTRY_GL4ES gl4es_glRotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z) {
DBG(printf("glRotatef(%f, %f, %f, %f), list=%p\n", angle, x, y, z, glstate->list.active);)
	
    if((x==0 && y==0 && z==0) || angle==0)
		return;	// nothing to do

    // Normalisasi vector rotasi
	GLfloat mag = x*x + y*y + z*z;
    if (mag > 0.0f && (mag < 0.99f || mag > 1.01f)) {
        mag = 1.0f / sqrtf(mag);
        x *= mag; y *= mag; z *= mag;
    }

	angle *= 3.1415926535f/180.f;
	
    // Gunakan sincosf jika memungkinkan (Linux/Android extension)
    GLfloat s, c;
    #ifdef _GNU_SOURCE
        sincosf(angle, &s, &c);
    #else
        s = sinf(angle);
        c = cosf(angle);
    #endif
	
    const GLfloat c1 = 1.0f - c;

    // Construct Matrix
	GLfloat tmp[16];
    // Pastikan 0 inisialisasi untuk elemen yang tidak disentuh
	// memset(tmp, 0, 16*sizeof(GLfloat)); // Manual set lebih cepat drpd memset+overwrite

	tmp[0] = x*x*c1+c;   tmp[4] = x*y*c1-z*s; tmp[8] = x*z*c1+y*s;  tmp[12] = 0.0f;
	tmp[1] = y*x*c1+z*s; tmp[5] = y*y*c1+c;   tmp[9] = y*z*c1-x*s;  tmp[13] = 0.0f;
	tmp[2] = x*z*c1-y*s; tmp[6] = y*z*c1+x*s; tmp[10] = z*z*c1+c;   tmp[14] = 0.0f;
	tmp[3] = 0.0f;       tmp[7] = 0.0f;       tmp[11] = 0.0f;       tmp[15] = 1.0f;

	gl4es_glMultMatrixf(tmp);
}

void APIENTRY_GL4ES gl4es_glOrthof(GLfloat left, GLfloat right, GLfloat bottom, GLfloat top, GLfloat nearVal, GLfloat farVal) {
DBG(printf("glOrthof(%f, %f, %f, %f, %f, %f), list=%p\n", left, right, top, bottom, nearVal, farVal, glstate->list.active);)
	GLfloat tmp[16];
	memset(tmp, 0, 16*sizeof(GLfloat));

    // Optimasi pembagian
    GLfloat r_l = 1.0f / (right - left);
    GLfloat t_b = 1.0f / (top - bottom);
    GLfloat f_n = 1.0f / (farVal - nearVal);

	tmp[0] = 2.0f * r_l;     
    tmp[12] = -(right+left) * r_l;
	tmp[5] = 2.0f * t_b;     
    tmp[13] = -(top+bottom) * t_b;
	tmp[10] = -2.0f * f_n; 
    tmp[14] = -(farVal+nearVal) * f_n;
	tmp[15] = 1.0f;

	gl4es_glMultMatrixf(tmp);
}

void APIENTRY_GL4ES gl4es_glFrustumf(GLfloat left,	GLfloat right, GLfloat bottom, GLfloat top,	GLfloat nearVal, GLfloat farVal) {
DBG(printf("glFrustumf(%f, %f, %f, %f, %f, %f) list=%p\n", left, right, top, bottom, nearVal, farVal, glstate->list.active);)
	GLfloat tmp[16];
	memset(tmp, 0, 16*sizeof(GLfloat));

    GLfloat r_l = 1.0f / (right - left);
    GLfloat t_b = 1.0f / (top - bottom);
    GLfloat f_n = 1.0f / (farVal - nearVal);

	tmp[0] = 2.0f*nearVal * r_l;	
    tmp[8] = (right+left) * r_l;
	tmp[5] = 2.0f*nearVal * t_b;   
    tmp[9] = (top+bottom) * t_b;
	tmp[10] = -(farVal+nearVal) * f_n; 
    tmp[14] = -2.0f*farVal*nearVal * f_n;
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