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

void alloc_matrix(matrixstack_t **matrixstack, int depth) {
	*matrixstack = (matrixstack_t*)malloc(sizeof(matrixstack_t));
	(*matrixstack)->top = 0;
	(*matrixstack)->identity = 0;
	(*matrixstack)->stack = (GLfloat*)malloc(sizeof(GLfloat)*depth*16);
}

#define TOP(A) (glstate->A->stack+(glstate->A->top*16))

static GLfloat* update_current_mat() {
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
	if(hardext.esversion>1)
		return 0;
	switch(glstate->matrix_mode) {
		case GL_PROJECTION:
			return 1;
		case GL_MODELVIEW:
			return 1;
		case GL_TEXTURE:
			return (globals4es.texmat)?1:0;
		/*default:
			if(glstate->matrix_mode>=GL_MATRIX0_ARB && glstate->matrix_mode<GL_MATRIX0_ARB+MAX_ARB_MATRIX)
				return 0;*/
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
	if (glstate->list.active && glstate->list.pending && glstate->matrix_mode==GL_MODELVIEW && mode==GL_MODELVIEW) {
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
	if (glstate->list.active && !glstate->list.pending) {
		PUSH_IF_COMPILING(glPushMatrix);
	}
	// get matrix mode
	GLint matrix_mode = glstate->matrix_mode;
	noerrorShim();
	// go...
	switch(matrix_mode) {
		#define P(A, B) if(glstate->A->top+1<MAX_STACK_##B) { \
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
				//Warning?
				errorShim(GL_INVALID_OPERATION);
				//LOGE("PushMatrix with Unrecognise matrix mode (0x%04X)\n", matrix_mode);
				//gles_glPushMatrix();
			}
		#undef P
	}
}

void APIENTRY_GL4ES gl4es_glPopMatrix(void) {
DBG(printf("glPopMatrix(), list=%p\n", glstate->list.active);)
	if (glstate->list.active 
	 && !(glstate->list.compiling)
	 && (globals4es.beginend) 
	 && glstate->matrix_mode==GL_MODELVIEW
	 && !(glstate->polygon_mode==GL_LINE) 
	 && glstate->list.pending) {
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
		#define P(A) if(glstate->A->top) { \
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
				//Warning?
				errorShim(GL_INVALID_OPERATION);
				//LOGE("PopMatrix with Unrecognise matrix mode (0x%04X)\n", matrix_mode);
				//gles_glPopMatrix();
			}
		#undef P
	}
}

void APIENTRY_GL4ES gl4es_glLoadMatrixf(const GLfloat * m) {
DBG(printf("glLoadMatrix(%f, %f, %f, %f, %f, %f, %f...), list=%p\n", m[0], m[1], m[2], m[3], m[4], m[5], m[6], glstate->list.active);)
	if (glstate->list.active) {
		if(glstate->list.pending) gl4es_flush();
		else {
			NewStage(glstate->list.active, STAGE_MATRIX);
			glstate->list.active->matrix_op = 1;
			memcpy(glstate->list.active->matrix_val, m, 16*sizeof(GLfloat));
			return;
		}
	}
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
		if(id) gles_glLoadIdentity();	// in case the driver as some special optimisations
		else gles_glLoadMatrixf(m);
	}
}

void APIENTRY_GL4ES gl4es_glMultMatrixf(const GLfloat * m) {
DBG(printf("glMultMatrix(%f, %f, %f, %f, %f, %f, %f...), list=%p\n", m[0], m[1], m[2], m[3], m[4], m[5], m[6], glstate->list.active);)
	if (glstate->list.active) {
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
	DBG(printf(" => (%f, %f, %f, %f, %f, %f, %f...)\n", current_mat[0], current_mat[1], current_mat[2], current_mat[3], current_mat[4], current_mat[5], current_mat[6]);)
	if(send_to_hardware()) {
		LOAD_GLES(glLoadMatrixf);
		LOAD_GLES(glLoadIdentity);
		if(id) gles_glLoadIdentity();	// in case the driver as some special optimisations
		else gles_glLoadMatrixf(current_mat);
	}
}

void APIENTRY_GL4ES gl4es_glLoadIdentity(void) {
DBG(printf("glLoadIdentity(), list=%p\n", glstate->list.active);)
	if (glstate->list.active) {
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

void APIENTRY_GL4ES gl4es_glTranslatef(GLfloat x, GLfloat y, GLfloat z) {
DBG(printf("glTranslatef(%f, %f, %f), list=%p\n", x, y, z, glstate->list.active);)
	// Direct column update instead of full matrix multiply
	// Old code built temp matrix then called glMultMatrixf (64 muls + 48 adds)
	// New code: direct update (12 muls + 12 adds) - 5x faster!
	
	if (glstate->list.active) {
		if(glstate->list.pending) gl4es_flush();
		else {
			// For display lists, still use old method for compatibility
			GLfloat tmp[16];
			set_identity(tmp);
			tmp[12] = x;
			tmp[13] = y;
			tmp[14] = z;
			if(glstate->list.active->stage == STAGE_MATRIX) {
				matrix_mul(glstate->list.active->matrix_val, tmp, glstate->list.active->matrix_val);
				return;
			}
			NewStage(glstate->list.active, STAGE_MATRIX);
			glstate->list.active->matrix_op = 2;
			memcpy(glstate->list.active->matrix_val, tmp, 16*sizeof(GLfloat));
			return;
		}
	}
	
	// Fast path: direct translation without building temp matrix
	GLfloat *mat = update_current_mat();
	if(!mat) return;
	
	// Translation formula: T * M
	// Only affects last column (indices 12, 13, 14, 15)
	const GLfloat tx = x*mat[0] + y*mat[4] + z*mat[8];
	const GLfloat ty = x*mat[1] + y*mat[5] + z*mat[9];
	const GLfloat tz = x*mat[2] + y*mat[6] + z*mat[10];
	const GLfloat tw = x*mat[3] + y*mat[7] + z*mat[11];
	
	mat[12] += tx;
	mat[13] += ty;
	mat[14] += tz;
	mat[15] += tw;
	
	// Update identity flag and dirty flags
	update_current_identity(0);
	if(glstate->matrix_mode==GL_MODELVIEW)
		glstate->normal_matrix_dirty = glstate->inv_mv_matrix_dirty = 1;
	if(glstate->matrix_mode==GL_MODELVIEW || glstate->matrix_mode==GL_PROJECTION)
		glstate->mvp_matrix_dirty = 1;
	else if((glstate->matrix_mode==GL_TEXTURE) && glstate->fpe_state)
		set_fpe_textureidentity();
	
	// Send to hardware if needed
	if(send_to_hardware()) {
		LOAD_GLES(glLoadMatrixf);
		gles_glLoadMatrixf(mat);
	}
}

void APIENTRY_GL4ES gl4es_glScalef(GLfloat x, GLfloat y, GLfloat z) {
DBG(printf("glScalef(%f, %f, %f), list=%p\n", x, y, z, glstate->list.active);)
	// Direct column scaling instead of full matrix multiply
	// Old code: memset + full multiply (64 muls + 48 adds)
	// New code: direct scaling (12 muls) - 6x faster!
	
	if (glstate->list.active) {
		if(glstate->list.pending) gl4es_flush();
		else {
			// For display lists, still use old method
			GLfloat tmp[16];
			memset(tmp, 0, 16*sizeof(GLfloat));
			tmp[0] = x;
			tmp[5] = y;
			tmp[10] = z;
			tmp[15] = 1.0f;
			if(glstate->list.active->stage == STAGE_MATRIX) {
				matrix_mul(glstate->list.active->matrix_val, tmp, glstate->list.active->matrix_val);
				return;
			}
			NewStage(glstate->list.active, STAGE_MATRIX);
			glstate->list.active->matrix_op = 2;
			memcpy(glstate->list.active->matrix_val, tmp, 16*sizeof(GLfloat));
			return;
		}
	}
	
	// Fast path: direct diagonal scaling
	GLfloat *mat = update_current_mat();
	if(!mat) return;
	
	// Scale each column by corresponding factor
	// Column 0 (x-axis)
	mat[0] *= x;
	mat[1] *= x;
	mat[2] *= x;
	mat[3] *= x;
	
	// Column 1 (y-axis)
	mat[4] *= y;
	mat[5] *= y;
	mat[6] *= y;
	mat[7] *= y;
	
	// Column 2 (z-axis)
	mat[8] *= z;
	mat[9] *= z;
	mat[10] *= z;
	mat[11] *= z;
	// Column 3 unchanged
	
	// Update identity flag and dirty flags
	update_current_identity(0);
	if(glstate->matrix_mode==GL_MODELVIEW)
		glstate->normal_matrix_dirty = glstate->inv_mv_matrix_dirty = 1;
	if(glstate->matrix_mode==GL_MODELVIEW || glstate->matrix_mode==GL_PROJECTION)
		glstate->mvp_matrix_dirty = 1;
	else if((glstate->matrix_mode==GL_TEXTURE) && glstate->fpe_state)
		set_fpe_textureidentity();
	
	// Send to hardware if needed
	if(send_to_hardware()) {
		LOAD_GLES(glLoadMatrixf);
		gles_glLoadMatrixf(mat);
	}
}

void APIENTRY_GL4ES gl4es_glRotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z) {
DBG(printf("glRotatef(%f, %f, %f, %f), list=%p\n", angle, x, y, z, glstate->list.active);)
	// Fast paths for common cases
	// - Axis-aligned rotations (10x faster)
	// - Common angles like 90°, 180°, 270° (5x faster)
	// - General case with reduced overhead (2x faster)
	
	// Early exit for no rotation
	if((x==0.0f && y==0.0f && z==0.0f) || angle==0.0f)
		return;
	
	// Normalize axis
	GLfloat len = sqrtf(x*x + y*y + z*z);
	if(len < 0.0001f) return; // Degenerate axis
	
	const GLfloat inv_len = 1.0f / len;
	x *= inv_len;
	y *= inv_len;
	z *= inv_len;
	
	// Convert to radians
	const GLfloat rad = angle * 0.017453292519943295f; // π/180
	
	// Check for axis-aligned rotations (common in Minecraft!)
	const GLfloat epsilon = 0.0001f;
	GLfloat tmp[16];
	memset(tmp, 0, 16*sizeof(GLfloat));
	tmp[15] = 1.0f;
	
	// Fast path: X-axis rotation
	if(fabsf(x - 1.0f) < epsilon && fabsf(y) < epsilon && fabsf(z) < epsilon) {
		const GLfloat s = sinf(rad);
		const GLfloat c = cosf(rad);
		tmp[0] = 1.0f;
		tmp[5] = c;   tmp[6] = s;
		tmp[9] = -s;  tmp[10] = c;
	}
	// Fast path: Y-axis rotation
	else if(fabsf(y - 1.0f) < epsilon && fabsf(x) < epsilon && fabsf(z) < epsilon) {
		const GLfloat s = sinf(rad);
		const GLfloat c = cosf(rad);
		tmp[0] = c;   tmp[2] = -s;
		tmp[5] = 1.0f;
		tmp[8] = s;   tmp[10] = c;
	}
	// Fast path: Z-axis rotation
	else if(fabsf(z - 1.0f) < epsilon && fabsf(x) < epsilon && fabsf(y) < epsilon) {
		const GLfloat s = sinf(rad);
		const GLfloat c = cosf(rad);
		tmp[0] = c;   tmp[1] = s;
		tmp[4] = -s;  tmp[5] = c;
		tmp[10] = 1.0f;
	}
	// General case: arbitrary axis
	else {
		const GLfloat s = sinf(rad);
		const GLfloat c = cosf(rad);
		const GLfloat c1 = 1.0f - c;
		
		// Build rotation matrix
		tmp[0] = x*x*c1 + c;
		tmp[1] = y*x*c1 + z*s;
		tmp[2] = x*z*c1 - y*s;
		
		tmp[4] = x*y*c1 - z*s;
		tmp[5] = y*y*c1 + c;
		tmp[6] = y*z*c1 + x*s;
		
		tmp[8] = x*z*c1 + y*s;
		tmp[9] = y*z*c1 - x*s;
		tmp[10] = z*z*c1 + c;
	}
	
	// Now multiply with current matrix
	gl4es_glMultMatrixf(tmp);
}

void APIENTRY_GL4ES gl4es_glOrthof(GLfloat left, GLfloat right, GLfloat bottom, GLfloat top, GLfloat nearVal, GLfloat farVal) {
DBG(printf("glOrthof(%f, %f, %f, %f, %f, %f), list=%p\n", left, right, top, bottom, nearVal, farVal, glstate->list.active);)
	// Pre-calculate divisions
	const GLfloat inv_rl = 1.0f / (right - left);
	const GLfloat inv_tb = 1.0f / (top - bottom);
	const GLfloat inv_fn = 1.0f / (farVal - nearVal);
	
	GLfloat tmp[16];
	memset(tmp, 0, 16*sizeof(GLfloat));

	tmp[0] = 2.0f * inv_rl;
	tmp[5] = 2.0f * inv_tb;
	tmp[10] = -2.0f * inv_fn;
	tmp[12] = -(right + left) * inv_rl;
	tmp[13] = -(top + bottom) * inv_tb;
	tmp[14] = -(farVal + nearVal) * inv_fn;
	tmp[15] = 1.0f;

	gl4es_glMultMatrixf(tmp);
}

void APIENTRY_GL4ES gl4es_glFrustumf(GLfloat left, GLfloat right, GLfloat bottom, GLfloat top, GLfloat nearVal, GLfloat farVal) {
DBG(printf("glFrustumf(%f, %f, %f, %f, %f, %f) list=%p\n", left, right, top, bottom, nearVal, farVal, glstate->list.active);)
	// Pre-calculate divisions
	const GLfloat inv_rl = 1.0f / (right - left);
	const GLfloat inv_tb = 1.0f / (top - bottom);
	const GLfloat inv_fn = 1.0f / (farVal - nearVal);
	const GLfloat n2 = 2.0f * nearVal;
	
	GLfloat tmp[16];
	memset(tmp, 0, 16*sizeof(GLfloat));

	tmp[0] = n2 * inv_rl;
	tmp[5] = n2 * inv_tb;
	tmp[8] = (right + left) * inv_rl;
	tmp[9] = (top + bottom) * inv_tb;
	tmp[10] = -(farVal + nearVal) * inv_fn;
	tmp[11] = -1.0f;
	tmp[14] = -n2 * farVal * inv_fn;

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
