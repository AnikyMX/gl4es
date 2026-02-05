/*
 * Refactored gl4eswraps.c for ARMv8
 * - Optimized Double->Float conversion using vector-friendly inline loops
 * - Precomputed scaling factors for integer-to-float color conversion
 * - Loop unrolling for small vectors (3/4 components)
 */

#include "gl4es.h"
#include "../texgen.h"
#include "../debug.h"
#include "stub.h"
#include <limits.h>

// Helper for fast Vector conversion (Compiler will auto-vectorize this on ARMv8)
static inline void double_to_float_array(const GLdouble* src, GLfloat* dst, int count) {
    for (int i = 0; i < count; i++) {
        dst[i] = (GLfloat)src[i];
    }
}

// Precomputed constants to avoid division in hot paths
#define SCALE_1_32767  3.0518509e-05f  // 1.0f / 32767.0f
#define SCALE_1_2147483647 4.6566129e-10f // 1.0f / INT_MAX

// naive wrappers

void APIENTRY_GL4ES gl4es_glClearDepth(GLdouble depth) {
    gl4es_glClearDepthf((GLfloat)depth);
}

void APIENTRY_GL4ES gl4es_glClipPlane(GLenum plane, const GLdouble *equation) {
    GLfloat s[4];
    // Manual unroll for 4 elements is faster than a loop setup
    s[0] = (GLfloat)equation[0];
    s[1] = (GLfloat)equation[1];
    s[2] = (GLfloat)equation[2];
    s[3] = (GLfloat)equation[3];
    gl4es_glClipPlanef(plane, s);
}

void APIENTRY_GL4ES gl4es_glDepthRange(GLdouble nearVal, GLdouble farVal) {
    gl4es_glDepthRangef((GLfloat)nearVal, (GLfloat)farVal);
}

void APIENTRY_GL4ES gl4es_glFogi(GLenum pname, GLint param) {
    gl4es_glFogf(pname, (GLfloat)param);
}

void APIENTRY_GL4ES gl4es_glFogiv(GLenum pname, GLint *iparams) {
    switch (pname) {
        case GL_FOG_DENSITY:
        case GL_FOG_START:
        case GL_FOG_END:
        case GL_FOG_MODE:
        case GL_FOG_INDEX: 
        case GL_FOG_COORD_SRC:
        {
            gl4es_glFogf(pname, (GLfloat)*iparams);
            break;
        }
        case GL_FOG_COLOR: {
            GLfloat params[4];
            // Optimized: Shift then Multiply (avoid division)
            params[0] = (iparams[0] >> 16) * SCALE_1_32767;
            params[1] = (iparams[1] >> 16) * SCALE_1_32767;
            params[2] = (iparams[2] >> 16) * SCALE_1_32767;
            params[3] = (iparams[3] >> 16) * SCALE_1_32767;
            gl4es_glFogfv(pname, params);
            break;
        }
    }
}

void APIENTRY_GL4ES gl4es_glGetTexGendv(GLenum coord, GLenum pname, GLdouble *params) {
    GLfloat fparams[4];
    gl4es_glGetTexGenfv(coord, pname, fparams);
    if (pname == GL_TEXTURE_GEN_MODE) {
        *params = (GLdouble)fparams[0];
    } else {
        params[0] = (GLdouble)fparams[0];
        params[1] = (GLdouble)fparams[1];
        params[2] = (GLdouble)fparams[2];
        params[3] = (GLdouble)fparams[3];
    }
}

void APIENTRY_GL4ES gl4es_glGetTexGeniv(GLenum coord, GLenum pname, GLint *params) {
    GLfloat fparams[4];
    gl4es_glGetTexGenfv(coord, pname, fparams);
    if (pname == GL_TEXTURE_GEN_MODE) {
        *params = (GLint)fparams[0];
    } else {
        // Direct cast without loop
        params[0] = (GLint)fparams[0];
        params[1] = (GLint)fparams[1];
        params[2] = (GLint)fparams[2];
        params[3] = (GLint)fparams[3];
    }
}

void APIENTRY_GL4ES gl4es_glGetMaterialiv(GLenum face, GLenum pname, GLint *params) {
    GLfloat fparams[4];
    gl4es_glGetMaterialfv(face, pname, fparams);
    
    if (pname == GL_SHININESS) {
        *params = (GLint)fparams[0];
    } else {
        if (pname == GL_COLOR_INDEXES) {
            params[0] = (GLint)fparams[0];
            params[1] = (GLint)fparams[1];
            params[2] = (GLint)fparams[2];
        } else {
            // Reversing the Fixed Point conversion with bitshift
            // Using direct cast for speed
            params[0] = ((int)(fparams[0] * 32767.0f)) << 16;
            params[1] = ((int)(fparams[1] * 32767.0f)) << 16;
            params[2] = ((int)(fparams[2] * 32767.0f)) << 16;
            params[3] = ((int)(fparams[3] * 32767.0f)) << 16;
        }
    }
}

void APIENTRY_GL4ES gl4es_glGetLightiv(GLenum light, GLenum pname, GLint *params) {
    GLfloat fparams[4];
    gl4es_glGetLightfv(light, pname, fparams);
    
    // Optimization: Flat logic to avoid branch misprediction inside loop
    if (pname == GL_AMBIENT || pname == GL_DIFFUSE || pname == GL_SPECULAR) {
        params[0] = ((int)(fparams[0] * 32767.0f)) << 16;
        params[1] = ((int)(fparams[1] * 32767.0f)) << 16;
        params[2] = ((int)(fparams[2] * 32767.0f)) << 16;
        params[3] = ((int)(fparams[3] * 32767.0f)) << 16;
    } else {
        int n = 4;
        switch(pname) {
            case GL_SPOT_EXPONENT:
            case GL_SPOT_CUTOFF:
            case GL_CONSTANT_ATTENUATION:
            case GL_LINEAR_ATTENUATION:
            case GL_QUADRATIC_ATTENUATION:
                 n = 1; break;
            case GL_SPOT_DIRECTION:
                 n = 3; break;
        }
        for (int i = 0; i < n; i++) params[i] = (GLint)fparams[i];
    }
}

void APIENTRY_GL4ES gl4es_glGetTexLevelParameteriv(GLenum target, GLint level, GLenum pname, GLint *params) {
    GLfloat fparams[4];
    gl4es_glGetTexLevelParameterfv(target, level, pname, fparams);
    if (pname == GL_TEXTURE_BORDER_COLOR) {
        for(int i = 0; i < 4; ++i) params[i] = (GLint)fparams[i];
    } else {
        *params = (GLint)fparams[0];
    }
}

void APIENTRY_GL4ES gl4es_glGetClipPlane(GLenum plane, GLdouble *equation) {
    GLfloat fparams[4];
    gl4es_glGetClipPlanef(plane, fparams);
    equation[0] = (GLdouble)fparams[0];
    equation[1] = (GLdouble)fparams[1];
    equation[2] = (GLdouble)fparams[2];
    equation[3] = (GLdouble)fparams[3];
}

void APIENTRY_GL4ES gl4es_glFrustum(GLdouble left, GLdouble right, GLdouble bottom,
             GLdouble top, GLdouble Near, GLdouble Far) {
    gl4es_glFrustumf((GLfloat)left, (GLfloat)right, (GLfloat)bottom, 
                     (GLfloat)top, (GLfloat)Near, (GLfloat)Far);
}

void APIENTRY_GL4ES gl4es_glPixelStoref(GLenum pname, GLfloat param) {
    gl4es_glPixelStorei(pname, (GLint)param);
}

void APIENTRY_GL4ES gl4es_glLighti(GLenum light, GLenum pname, GLint param) {
    gl4es_glLightf(light, pname, (GLfloat)param);
}

void APIENTRY_GL4ES gl4es_glPixelTransferi(GLenum pname, GLint param) {
    gl4es_glPixelTransferf(pname, (GLfloat)param);    
}

void APIENTRY_GL4ES gl4es_glLightiv(GLenum light, GLenum pname, GLint *iparams) {
    GLfloat params[4];
    switch (pname) {
        case GL_AMBIENT:
        case GL_DIFFUSE:
        case GL_SPECULAR:
            // Unrolled & Optimized
            params[0] = (iparams[0] >> 16) * SCALE_1_32767;
            params[1] = (iparams[1] >> 16) * SCALE_1_32767;
            params[2] = (iparams[2] >> 16) * SCALE_1_32767;
            params[3] = (iparams[3] >> 16) * SCALE_1_32767;
            gl4es_glLightfv(light, pname, params);
            break;
        case GL_POSITION:
        case GL_SPOT_DIRECTION:
            params[0] = (GLfloat)iparams[0];
            params[1] = (GLfloat)iparams[1];
            params[2] = (GLfloat)iparams[2];
            params[3] = (GLfloat)iparams[3];
            gl4es_glLightfv(light, pname, params);
            break;
        case GL_SPOT_EXPONENT:
        case GL_SPOT_CUTOFF:
        case GL_CONSTANT_ATTENUATION:
        case GL_LINEAR_ATTENUATION:
        case GL_QUADRATIC_ATTENUATION: {
            gl4es_glLightf(light, pname, (GLfloat)*iparams);
            break;
        }
    }
}

void APIENTRY_GL4ES gl4es_glLightModeli(GLenum pname, GLint param) {
    gl4es_glLightModelf(pname, (GLfloat)param);
}

void APIENTRY_GL4ES gl4es_glLightModeliv(GLenum pname, GLint *iparams) {
    switch (pname) {
        case GL_LIGHT_MODEL_AMBIENT: {
            GLfloat params[4];
            params[0] = (iparams[0] >> 16) * SCALE_1_32767;
            params[1] = (iparams[1] >> 16) * SCALE_1_32767;
            params[2] = (iparams[2] >> 16) * SCALE_1_32767;
            params[3] = (iparams[3] >> 16) * SCALE_1_32767;
            gl4es_glLightModelfv(pname, params);
            break;
        }
        case GL_LIGHT_MODEL_LOCAL_VIEWER:
        case GL_LIGHT_MODEL_TWO_SIDE: {
            gl4es_glLightModelf(pname, (GLfloat)*iparams);
            break;
        }
    }
}

void APIENTRY_GL4ES gl4es_glMateriali(GLenum face, GLenum pname, GLint param) {
    gl4es_glMaterialf(face, pname, (GLfloat)param);
}

void APIENTRY_GL4ES gl4es_glMaterialiv(GLenum face, GLenum pname, GLint *iparams) {
    switch (pname) {
        case GL_AMBIENT: 
        case GL_DIFFUSE:
        case GL_SPECULAR:
        case GL_EMISSION:
        case GL_AMBIENT_AND_DIFFUSE:
        {
            GLfloat params[4];
            params[0] = (iparams[0] >> 16) * SCALE_1_32767;
            params[1] = (iparams[1] >> 16) * SCALE_1_32767;
            params[2] = (iparams[2] >> 16) * SCALE_1_32767;
            params[3] = (iparams[3] >> 16) * SCALE_1_32767;
            gl4es_glMaterialfv(face, pname, params);
            break;
        }
        case GL_SHININESS:
        {
            gl4es_glMaterialf(face, pname, (GLfloat)*iparams);
            break;
        }
        case GL_COLOR_INDEXES:
        {
            GLfloat params[3];
            params[0] = (GLfloat)iparams[0];
            params[1] = (GLfloat)iparams[1];
            params[2] = (GLfloat)iparams[2];
            gl4es_glMaterialfv(face, pname, params);
            break;
        }
    }
}

void APIENTRY_GL4ES gl4es_glMultiTexCoord1f(GLenum target, GLfloat s) {
     gl4es_glMultiTexCoord4f(target, s, 0, 0, 1);
}
void APIENTRY_GL4ES gl4es_glMultiTexCoord1fv(GLenum target, GLfloat *t) {
     gl4es_glMultiTexCoord4f(target, t[0], 0, 0, 1);
}
void APIENTRY_GL4ES gl4es_glMultiTexCoord2f(GLenum target, GLfloat s, GLfloat t) {
     gl4es_glMultiTexCoord4f(target, s, t, 0, 1);
}
void APIENTRY_GL4ES gl4es_glMultiTexCoord3f(GLenum target, GLfloat s, GLfloat t, GLfloat r) {
     gl4es_glMultiTexCoord4f(target, s, t, r, 1);
}
void APIENTRY_GL4ES gl4es_glMultiTexCoord3fv(GLenum target, GLfloat *t) {
     gl4es_glMultiTexCoord4f(target, t[0], t[1], t[2], 1);
}

void APIENTRY_GL4ES gl4es_glOrtho(GLdouble left, GLdouble right, GLdouble bottom,
             GLdouble top, GLdouble Near, GLdouble Far) {
    gl4es_glOrthof((GLfloat)left, (GLfloat)right, (GLfloat)bottom, 
                   (GLfloat)top, (GLfloat)Near, (GLfloat)Far);
}

// OES wrappers
void APIENTRY_GL4ES glClearDepthfOES(GLfloat depth) {
    gl4es_glClearDepthf(depth);
}
void APIENTRY_GL4ES glClipPlanefOES(GLenum plane, const GLfloat *equation) {
    gl4es_glClipPlanef(plane, equation);
}
void APIENTRY_GL4ES glDepthRangefOES(GLclampf Near, GLclampf Far) {
    gl4es_glDepthRangef(Near, Far);
}
void APIENTRY_GL4ES glFrustumfOES(GLfloat left, GLfloat right, GLfloat bottom,
                   GLfloat top, GLfloat Near, GLfloat Far) {
    gl4es_glFrustumf(left, right, bottom, top, Near, Far);
}
void APIENTRY_GL4ES glGetClipPlanefOES(GLenum pname, GLfloat equation[4]) {
    gl4es_glGetClipPlanef(pname, equation);
}
void APIENTRY_GL4ES glOrthofOES(GLfloat left, GLfloat right, GLfloat bottom,
                 GLfloat top, GLfloat Near, GLfloat Far) {
    gl4es_glOrthof(left, right, bottom, top, Near, Far);
}

// glRect

#define GL_RECT(suffix, type)                                       \
    void APIENTRY_GL4ES gl4es_glRect##suffix(type x1, type y1, type x2, type y2) { \
        gl4es_glBegin(GL_QUADS);                                    \
        gl4es_glVertex2f((GLfloat)x1, (GLfloat)y1);                 \
        gl4es_glVertex2f((GLfloat)x2, (GLfloat)y1);                 \
        gl4es_glVertex2f((GLfloat)x2, (GLfloat)y2);                 \
        gl4es_glVertex2f((GLfloat)x1, (GLfloat)y2);                 \
        gl4es_glEnd();                                              \
    }                                                               \
    void APIENTRY_GL4ES gl4es_glRect##suffix##v(const type *v1, const type *v2) {  \
        gl4es_glRect##suffix(v1[0], v1[1], v2[0], v2[1]);           \
    }

GL_RECT(d, GLdouble)
GL_RECT(f, GLfloat)
GL_RECT(i, GLint)
GL_RECT(s, GLshort)
#undef GL_RECT

// basic thunking
// OPTIMIZATION: 'mul' replaces 'invmax'. We multiply instead of divide.

#define THUNK(suffix, type, mul)                                \
/* colors */                                                \
void APIENTRY_GL4ES gl4es_glColor3##suffix(type r, type g, type b) {             \
    gl4es_glColor4f((GLfloat)r*mul, (GLfloat)g*mul, (GLfloat)b*mul, 1.0f);       \
}                                                           \
void APIENTRY_GL4ES gl4es_glColor4##suffix(type r, type g, type b, type a) {     \
    gl4es_glColor4f((GLfloat)r*mul, (GLfloat)g*mul, (GLfloat)b*mul, (GLfloat)a*mul); \
}                                                           \
void APIENTRY_GL4ES gl4es_glColor3##suffix##v(const type *v) {                   \
    gl4es_glColor4f((GLfloat)v[0]*mul, (GLfloat)v[1]*mul, (GLfloat)v[2]*mul, 1.0f); \
}                                                           \
void APIENTRY_GL4ES gl4es_glColor4##suffix##v(const type *v) {                   \
    gl4es_glColor4f((GLfloat)v[0]*mul, (GLfloat)v[1]*mul, (GLfloat)v[2]*mul, (GLfloat)v[3]*mul); \
}                                                           \
void APIENTRY_GL4ES gl4es_glSecondaryColor3##suffix(type r, type g, type b) {    \
    gl4es_glSecondaryColor3f((GLfloat)r*mul, (GLfloat)g*mul, (GLfloat)b*mul);    \
}                                                           \
void APIENTRY_GL4ES gl4es_glSecondaryColor3##suffix##v(const type *v) {          \
    gl4es_glSecondaryColor3f((GLfloat)v[0]*mul, (GLfloat)v[1]*mul, (GLfloat)v[2]*mul);\
}                                                           \
/* index */                                                 \
void APIENTRY_GL4ES gl4es_glIndex##suffix(type c) {                              \
    gl4es_glIndexf((GLfloat)c);                                   \
}                                                           \
void APIENTRY_GL4ES gl4es_glIndex##suffix##v(const type *c) {                    \
    gl4es_glIndexf((GLfloat)c[0]);                                \
}                                                           \
/* normal */                                                \
void APIENTRY_GL4ES gl4es_glNormal3##suffix(type x, type y, type z) {            \
    gl4es_glNormal3f((GLfloat)x, (GLfloat)y, (GLfloat)z);         \
}                                                           \
void APIENTRY_GL4ES gl4es_glNormal3##suffix##v(const type *v) {                  \
    gl4es_glNormal3f((GLfloat)v[0], (GLfloat)v[1], (GLfloat)v[2]);\
}                                                           \
/* raster */                                                \
void APIENTRY_GL4ES gl4es_glRasterPos2##suffix(type x, type y) {                 \
    gl4es_glRasterPos3f((GLfloat)x, (GLfloat)y, 0.0f);            \
}                                                           \
void APIENTRY_GL4ES gl4es_glRasterPos2##suffix##v(type *v) {                     \
    gl4es_glRasterPos3f((GLfloat)v[0], (GLfloat)v[1], 0.0f);      \
}                                                           \
void APIENTRY_GL4ES gl4es_glRasterPos3##suffix(type x, type y, type z) {         \
    gl4es_glRasterPos3f((GLfloat)x, (GLfloat)y, (GLfloat)z);      \
}                                                           \
void APIENTRY_GL4ES gl4es_glRasterPos3##suffix##v(type *v) {                     \
    gl4es_glRasterPos3f((GLfloat)v[0], (GLfloat)v[1], (GLfloat)v[2]); \
}                                                           \
void APIENTRY_GL4ES gl4es_glRasterPos4##suffix(type x, type y, type z, type w) { \
    gl4es_glRasterPos4f((GLfloat)x, (GLfloat)y, (GLfloat)z, (GLfloat)w); \
}                                                           \
void APIENTRY_GL4ES gl4es_glRasterPos4##suffix##v(type *v) {                     \
    gl4es_glRasterPos4f((GLfloat)v[0], (GLfloat)v[1], (GLfloat)v[2], (GLfloat)v[3]); \
}                                                           \
void APIENTRY_GL4ES gl4es_glWindowPos2##suffix(type x, type y) {                 \
    gl4es_glWindowPos3f((GLfloat)x, (GLfloat)y, 0.0f);            \
}                                                           \
void APIENTRY_GL4ES gl4es_glWindowPos2##suffix##v(type *v) {                     \
    gl4es_glWindowPos3f((GLfloat)v[0], (GLfloat)v[1], 0.0f);      \
}                                                           \
void APIENTRY_GL4ES gl4es_glWindowPos3##suffix(type x, type y, type z) {         \
    gl4es_glWindowPos3f((GLfloat)x, (GLfloat)y, (GLfloat)z);      \
}                                                           \
void APIENTRY_GL4ES gl4es_glWindowPos3##suffix##v(type *v) {                     \
    gl4es_glWindowPos3f((GLfloat)v[0], (GLfloat)v[1], (GLfloat)v[2]); \
}                                                           \
/* vertex */                                                \
void APIENTRY_GL4ES gl4es_glVertex2##suffix(type x, type y) {                    \
    gl4es_glVertex4f((GLfloat)x, (GLfloat)y, 0.0f, 1.0f);         \
}                                                           \
void APIENTRY_GL4ES gl4es_glVertex2##suffix##v(type *v) {                        \
    gl4es_glVertex4f((GLfloat)v[0], (GLfloat)v[1], 0.0f, 1.0f);   \
}                                                           \
void APIENTRY_GL4ES gl4es_glVertex3##suffix(type x, type y, type z) {            \
    gl4es_glVertex4f((GLfloat)x, (GLfloat)y, (GLfloat)z, 1.0f);   \
}                                                           \
void APIENTRY_GL4ES gl4es_glVertex3##suffix##v(type *v) {                        \
    gl4es_glVertex4f((GLfloat)v[0], (GLfloat)v[1], (GLfloat)v[2], 1.0f); \
}                                                           \
void APIENTRY_GL4ES gl4es_glVertex4##suffix(type r, type g, type b, type w) {    \
    gl4es_glVertex4f((GLfloat)r, (GLfloat)g, (GLfloat)b, (GLfloat)w); \
}                                                           \
void APIENTRY_GL4ES gl4es_glVertex4##suffix##v(type *v) {                        \
    gl4es_glVertex4f((GLfloat)v[0], (GLfloat)v[1], (GLfloat)v[2], (GLfloat)v[3]); \
}                                                           \
/* texture */                                               \
void APIENTRY_GL4ES gl4es_glTexCoord1##suffix(type s) {                          \
    gl4es_glTexCoord4f((GLfloat)s, 0.0f, 0.0f, 1.0f);             \
}                                                           \
void APIENTRY_GL4ES gl4es_glTexCoord1##suffix##v(type *t) {                      \
    gl4es_glTexCoord4f((GLfloat)t[0], 0.0f, 0.0f, 1.0f);          \
}                                                           \
void APIENTRY_GL4ES gl4es_glTexCoord2##suffix(type s, type t) {                  \
    gl4es_glTexCoord4f((GLfloat)s, (GLfloat)t, 0.0f, 1.0f);       \
}                                                           \
void APIENTRY_GL4ES gl4es_glTexCoord2##suffix##v(type *t) {                      \
    gl4es_glTexCoord4f((GLfloat)t[0], (GLfloat)t[1], 0.0f, 1.0f); \
}                                                           \
void APIENTRY_GL4ES gl4es_glTexCoord3##suffix(type s, type t, type r) {          \
    gl4es_glTexCoord4f((GLfloat)s, (GLfloat)t, (GLfloat)r, 1.0f); \
}                                                           \
void APIENTRY_GL4ES gl4es_glTexCoord3##suffix##v(type *t) {                      \
    gl4es_glTexCoord4f((GLfloat)t[0], (GLfloat)t[1], (GLfloat)t[2], 1.0f); \
}                                                           \
void APIENTRY_GL4ES gl4es_glTexCoord4##suffix(type s, type t, type r, type q) {  \
    gl4es_glTexCoord4f((GLfloat)s, (GLfloat)t, (GLfloat)r, (GLfloat)q); \
}                                                           \
void APIENTRY_GL4ES gl4es_glTexCoord4##suffix##v(type *t) {                      \
    gl4es_glTexCoord4f((GLfloat)t[0], (GLfloat)t[1], (GLfloat)t[2], (GLfloat)t[3]); \
}                                                           \
/* multi-texture */                                         \
void APIENTRY_GL4ES gl4es_glMultiTexCoord1##suffix(GLenum target, type s) {      \
    gl4es_glMultiTexCoord4f(target, (GLfloat)s, 0.0f, 0.0f, 1.0f); \
}                                                           \
void APIENTRY_GL4ES gl4es_glMultiTexCoord1##suffix##v(GLenum target, type *t) {  \
    gl4es_glMultiTexCoord4f(target, (GLfloat)t[0], 0.0f, 0.0f, 1.0f); \
}                                                           \
void APIENTRY_GL4ES gl4es_glMultiTexCoord2##suffix(GLenum target, type s, type t) { \
    gl4es_glMultiTexCoord4f(target, (GLfloat)s, (GLfloat)t, 0.0f, 1.0f); \
}                                                           \
void APIENTRY_GL4ES gl4es_glMultiTexCoord2##suffix##v(GLenum target, type *t) {   \
    gl4es_glMultiTexCoord4f(target, (GLfloat)t[0], (GLfloat)t[1], 0.0f, 1.0f); \
}                                                           \
void APIENTRY_GL4ES gl4es_glMultiTexCoord3##suffix(GLenum target, type s, type t, type r) { \
    gl4es_glMultiTexCoord4f(target, (GLfloat)s, (GLfloat)t, (GLfloat)r, 1.0f); \
}                                                           \
void APIENTRY_GL4ES gl4es_glMultiTexCoord3##suffix##v(GLenum target, type *t) {   \
    gl4es_glMultiTexCoord4f(target, (GLfloat)t[0], (GLfloat)t[1], (GLfloat)t[2], 1.0f); \
}                                                           \
void APIENTRY_GL4ES gl4es_glMultiTexCoord4##suffix(GLenum target, type s, type t, type r, type q) { \
    gl4es_glMultiTexCoord4f(target, (GLfloat)s, (GLfloat)t, (GLfloat)r, (GLfloat)q); \
}                                                           \
void APIENTRY_GL4ES gl4es_glMultiTexCoord4##suffix##v(GLenum target, type *t) {   \
    gl4es_glMultiTexCoord4f(target, (GLfloat)t[0], (GLfloat)t[1], (GLfloat)t[2], (GLfloat)t[3]); \
}


// Replacing invmax divisions with pre-computed multiplication factors
// 1.0 / 127.0 = 0.00787401574f
THUNK(b, GLbyte, 0.00787401574f)
THUNK(d, GLdouble, 1.0f)
// 1.0 / 2147483647.0 = 4.6566129e-10f
THUNK(i, GLint, 4.6566129e-10f)
// 1.0 / 32767.0 = 3.0518509e-05f
THUNK(s, GLshort, 3.0518509e-05f)
// 1.0 / 255.0 = 0.00392156862f
THUNK(ub, GLubyte, 0.00392156862f)
// 1.0 / 4294967295.0 = 2.3283064e-10f
THUNK(ui, GLuint, 2.3283064e-10f)
// 1.0 / 65535.0 = 1.5259021e-05f
THUNK(us, GLushort, 1.5259021e-05f)

#undef THUNK

// manually defined float wrappers, because we don't autowrap float functions

// color
void APIENTRY_GL4ES gl4es_glColor3f(GLfloat r, GLfloat g, GLfloat b) {
    gl4es_glColor4f(r, g, b, 1.0f);
}
void APIENTRY_GL4ES gl4es_glColor3fv(GLfloat *c) {
    gl4es_glColor4f(c[0], c[1], c[2], 1.0f);
}
void APIENTRY_GL4ES gl4es_glIndexfv(const GLfloat *c) {
    gl4es_glIndexf(*c);
}
void APIENTRY_GL4ES gl4es_glSecondaryColor3fv(const GLfloat *v) {
    gl4es_glSecondaryColor3f(v[0], v[1], v[2]);
}
AliasExport(void,glSecondaryColor3fv,EXT,(GLfloat *t));

// raster
void APIENTRY_GL4ES gl4es_glRasterPos2f(GLfloat x, GLfloat y) {
    gl4es_glRasterPos3f(x, y, 0.0f);
}
void APIENTRY_GL4ES gl4es_glRasterPos2fv(const GLfloat *v) {
    gl4es_glRasterPos3f(v[0], v[1], 0.0f);
}
void APIENTRY_GL4ES gl4es_glRasterPos3fv(const GLfloat *v) {
    gl4es_glRasterPos3f(v[0], v[1], v[2]);
}
void APIENTRY_GL4ES gl4es_glRasterPos4f(GLfloat x, GLfloat y, GLfloat z, GLfloat w) {
    gl4es_glRasterPos3f(x/w, y/w, z/w);
}
void APIENTRY_GL4ES gl4es_glRasterPos4fv(const GLfloat *v) {
    gl4es_glRasterPos4f(v[0], v[1], v[2], v[3]);
}
void APIENTRY_GL4ES gl4es_glWindowPos2f(GLfloat x, GLfloat y) {
    gl4es_glWindowPos3f(x, y, 0.0f);
}
void APIENTRY_GL4ES gl4es_glWindowPos2fv(const GLfloat *v) {
    gl4es_glWindowPos3f(v[0], v[1], 0.0f);
}
void APIENTRY_GL4ES gl4es_glWindowPos3fv(const GLfloat *v) {
    gl4es_glWindowPos3f(v[0], v[1], v[2]);
}

// eval
void APIENTRY_GL4ES gl4es_glEvalCoord1d(GLdouble u) {
    gl4es_glEvalCoord1f((GLfloat)u);
}
void APIENTRY_GL4ES gl4es_glEvalCoord2d(GLdouble u, GLdouble v) {
    gl4es_glEvalCoord2f((GLfloat)u, (GLfloat)v);
}
void APIENTRY_GL4ES gl4es_glEvalCoord1fv(GLfloat *v) {
    gl4es_glEvalCoord1f(v[0]);
}
void APIENTRY_GL4ES gl4es_glEvalCoord1dv(GLdouble *v) {
    gl4es_glEvalCoord1f((GLfloat)v[0]);
}
void APIENTRY_GL4ES gl4es_glEvalCoord2fv(GLfloat *v) {
    gl4es_glEvalCoord2f(v[0], v[1]);
}
void APIENTRY_GL4ES gl4es_glEvalCoord2dv(GLdouble *v) {
    gl4es_glEvalCoord2f((GLfloat)v[0], (GLfloat)v[1]);
}
void APIENTRY_GL4ES gl4es_glMapGrid1d(GLint un, GLdouble u1, GLdouble u2) {
    gl4es_glMapGrid1f(un, (GLfloat)u1, (GLfloat)u2);
}
void APIENTRY_GL4ES gl4es_glMapGrid2d(GLint un, GLdouble u1, GLdouble u2,
                 GLint vn, GLdouble v1, GLdouble v2) {
    gl4es_glMapGrid2f(un, (GLfloat)u1, (GLfloat)u2, vn, (GLfloat)v1, (GLfloat)v2);
}

// matrix
void APIENTRY_GL4ES gl4es_glLoadMatrixd(const GLdouble *m) {
    GLfloat s[16];
    double_to_float_array(m, s, 16);
    gl4es_glLoadMatrixf(s);
}
void APIENTRY_GL4ES gl4es_glMultMatrixd(const GLdouble *m) {
    GLfloat s[16];
    double_to_float_array(m, s, 16);
    gl4es_glMultMatrixf(s);
}

// textures
void APIENTRY_GL4ES gl4es_glTexCoord1f(GLfloat s) {
    gl4es_glTexCoord4f(s, 0, 0, 1);
}
void APIENTRY_GL4ES gl4es_glTexCoord1fv(GLfloat *t) {
    gl4es_glTexCoord4f(t[0], 0, 0, 1);
}
void APIENTRY_GL4ES gl4es_glTexCoord2f(GLfloat s, GLfloat t) {
    gl4es_glTexCoord4f(s, t, 0, 1);
}
void APIENTRY_GL4ES gl4es_glTexCoord2fv(GLfloat *t) {
    gl4es_glMultiTexCoord2fv(GL_TEXTURE0, t);
}
void APIENTRY_GL4ES gl4es_glTexCoord3f(GLfloat s, GLfloat t, GLfloat r) {
    gl4es_glTexCoord4f(s, t, r, 1);
}
void APIENTRY_GL4ES gl4es_glTexCoord3fv(GLfloat *t) {
    gl4es_glTexCoord4f(t[0], t[1], t[2], 1);
}
void APIENTRY_GL4ES gl4es_glTexCoord4fv(GLfloat *t) {
    gl4es_glTexCoord4f(t[0], t[1], t[2], t[3]);
}

// texgen
void APIENTRY_GL4ES gl4es_glTexGend(GLenum coord, GLenum pname, GLdouble param) {
    gl4es_glTexGenf(coord, pname, (GLfloat)param);
}
void APIENTRY_GL4ES gl4es_glTexGenf(GLenum coord, GLenum pname, GLfloat param) {
    GLfloat params[4] = {param, 0, 0, 0};
    gl4es_glTexGenfv(coord, pname, params);
}
void APIENTRY_GL4ES gl4es_glTexGendv(GLenum coord, GLenum pname, const GLdouble *params) {
    GLfloat tmp[4];
    tmp[0] = (GLfloat)params[0];
    if ((pname==GL_OBJECT_PLANE) || (pname==GL_EYE_PLANE)) {
        tmp[1] = (GLfloat)params[1];
        tmp[2] = (GLfloat)params[2];
        tmp[3] = (GLfloat)params[3];
    }
    gl4es_glTexGenfv(coord, pname, tmp);
}
void APIENTRY_GL4ES gl4es_glTexGeniv(GLenum coord, GLenum pname, const GLint *params) {
    GLfloat tmp[4];
    tmp[0] = (GLfloat)params[0];
    if ((pname==GL_OBJECT_PLANE) || (pname==GL_EYE_PLANE)) {
        tmp[1] = (GLfloat)params[1];
        tmp[2] = (GLfloat)params[2];
        tmp[3] = (GLfloat)params[3];
    }
    gl4es_glTexGenfv(coord, pname, tmp);
}

// transforms
void APIENTRY_GL4ES gl4es_glRotated(GLdouble angle, GLdouble x, GLdouble y, GLdouble z) {
    gl4es_glRotatef((GLfloat)angle, (GLfloat)x, (GLfloat)y, (GLfloat)z);
}
void APIENTRY_GL4ES gl4es_glScaled(GLdouble x, GLdouble y, GLdouble z) {
    gl4es_glScalef((GLfloat)x, (GLfloat)y, (GLfloat)z);
}
void APIENTRY_GL4ES gl4es_glTranslated(GLdouble x, GLdouble y, GLdouble z) {
    gl4es_glTranslatef((GLfloat)x, (GLfloat)y, (GLfloat)z);
}

// vertex
void APIENTRY_GL4ES gl4es_glVertex2f(GLfloat x, GLfloat y) {
    gl4es_glVertex4f(x, y, 0, 1);
}
void APIENTRY_GL4ES gl4es_glVertex2fv(GLfloat *v) {
    gl4es_glVertex4f(v[0], v[1], 0, 1);
}
void APIENTRY_GL4ES gl4es_glVertex3f(GLfloat r, GLfloat g, GLfloat b) {
    gl4es_glVertex4f(r, g, b, 1);
}
void APIENTRY_GL4ES gl4es_glBlendEquationSeparatei(GLuint buf, GLenum modeRGB, GLenum modeAlpha) {
    gl4es_glBlendEquationSeparate(modeRGB, modeAlpha);
}

void APIENTRY_GL4ES gl4es_glBlendFuncSeparatei(GLuint buf, GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha) {
    gl4es_glBlendFuncSeparate(srcRGB, dstRGB, srcAlpha, dstAlpha);
}

void APIENTRY_GL4ES gl4es_glGetTexParameterfv(GLenum target, GLenum pname, GLfloat * params) {
    gl4es_glGetTexLevelParameterfv(target, 0, pname, params);
}
 
void APIENTRY_GL4ES gl4es_glGetTexParameteriv(GLenum target, GLenum pname, GLint * params) {
    gl4es_glGetTexLevelParameteriv(target, 0, pname, params);
}

// Samples stuff
#include "../loader.h"
void APIENTRY_GL4ES gl4es_glSampleCoverage(GLclampf value, GLboolean invert) {
    LOAD_GLES(glSampleCoverage);
    PUSH_IF_COMPILING(glSampleCoverage)
    gles_glSampleCoverage(value, invert);
}
AliasExport(void,glSampleCoverage,,(GLclampf value, GLboolean invert));
AliasExport(void,glSampleCoverage,ARB,(GLclampf value, GLboolean invert));

// VertexArray stuff
void APIENTRY_GL4ES gl4es_glVertexAttrib1f (GLuint index, GLfloat v0) { GLfloat f[4] = {v0, 0, 0, 1}; gl4es_glVertexAttrib4fv(index, f); };
void APIENTRY_GL4ES gl4es_glVertexAttrib2f (GLuint index, GLfloat v0, GLfloat v1) { GLfloat f[4] = {v0, v1, 0, 1}; gl4es_glVertexAttrib4fv(index, f); };
void APIENTRY_GL4ES gl4es_glVertexAttrib3f (GLuint index, GLfloat v0, GLfloat v1, GLfloat v2) { GLfloat f[4] = {v0, v1, v2, 1}; gl4es_glVertexAttrib4fv(index, f); };
void APIENTRY_GL4ES gl4es_glVertexAttrib1fv (GLuint index, const GLfloat *v) { GLfloat f[4] = {v[0], 0, 0, 1}; gl4es_glVertexAttrib4fv(index, f); }; \
void APIENTRY_GL4ES gl4es_glVertexAttrib2fv (GLuint index, const GLfloat *v) { GLfloat f[4] = {v[0], v[1], 0, 1}; gl4es_glVertexAttrib4fv(index, f); }; \
void APIENTRY_GL4ES gl4es_glVertexAttrib3fv (GLuint index, const GLfloat *v) { GLfloat f[4] = {v[0], v[1], v[2], 1}; gl4es_glVertexAttrib4fv(index, f); }; \
AliasExport(void,glVertexAttrib1f,, (GLuint index, GLfloat v0));
AliasExport(void,glVertexAttrib2f,, (GLuint index, GLfloat v0, GLfloat v1));
AliasExport(void,glVertexAttrib3f,, (GLuint index, GLfloat v0, GLfloat v1, GLfloat v2));
AliasExport(void,glVertexAttrib1fv,, (GLuint index, const GLfloat *v));
AliasExport(void,glVertexAttrib2fv,, (GLuint index, const GLfloat *v));
AliasExport(void,glVertexAttrib3fv,, (GLuint index, const GLfloat *v));

#define THUNK(suffix, type, M2) \
void APIENTRY_GL4ES gl4es_glVertexAttrib1##suffix (GLuint index, type v0) { GLfloat f[4] = {(GLfloat)v0, 0, 0, 1}; gl4es_glVertexAttrib4fv(index, f); }; \
void APIENTRY_GL4ES gl4es_glVertexAttrib2##suffix (GLuint index, type v0, type v1) { GLfloat f[4] = {(GLfloat)v0, (GLfloat)v1, 0, 1}; gl4es_glVertexAttrib4fv(index, f); }; \
void APIENTRY_GL4ES gl4es_glVertexAttrib3##suffix (GLuint index, type v0, type v1, type v2) { GLfloat f[4] = {(GLfloat)v0, (GLfloat)v1, (GLfloat)v2, 1}; gl4es_glVertexAttrib4fv(index, f); }; \
void APIENTRY_GL4ES gl4es_glVertexAttrib4##suffix (GLuint index, type v0, type v1, type v2, type v3) { GLfloat f[4] = {(GLfloat)v0, (GLfloat)v1, (GLfloat)v2, (GLfloat)v3}; gl4es_glVertexAttrib4fv(index, f); }; \
void APIENTRY_GL4ES gl4es_glVertexAttrib1##suffix##v (GLuint index, const type *v) { GLfloat f[4] = {(GLfloat)v[0], 0, 0, 1}; gl4es_glVertexAttrib4fv(index, f); }; \
void APIENTRY_GL4ES gl4es_glVertexAttrib2##suffix##v (GLuint index, const type *v) { GLfloat f[4] = {(GLfloat)v[0], (GLfloat)v[1], 0, 1}; gl4es_glVertexAttrib4fv(index, f); }; \
void APIENTRY_GL4ES gl4es_glVertexAttrib3##suffix##v (GLuint index, const type *v) { GLfloat f[4] = {(GLfloat)v[0], (GLfloat)v[1], (GLfloat)v[2], 1}; gl4es_glVertexAttrib4fv(index, f); }; \
AliasExport##M2##_1(void,glVertexAttrib1##suffix,, (GLuint index, type v0)); \
AliasExport##M2##_1(void,glVertexAttrib2##suffix,, (GLuint index, type v0, type v1)); \
AliasExport##M2##_1(void,glVertexAttrib3##suffix,, (GLuint index, type v0, type v1, type v2)); \
AliasExport##M2##_1(void,glVertexAttrib4##suffix,, (GLuint index, type v0, type v1, type v2, type v3)); \
AliasExport(void,glVertexAttrib1##suffix##v,, (GLuint index, const type *v)); \
AliasExport(void,glVertexAttrib2##suffix##v,, (GLuint index, const type *v)); \
AliasExport(void,glVertexAttrib3##suffix##v,, (GLuint index, const type *v))

THUNK(s, GLshort, );
THUNK(d, GLdouble, _D);
#undef THUNK

void APIENTRY_GL4ES gl4es_glVertexAttrib4dv (GLuint index, const GLdouble *v) { 
    GLfloat f[4] = {(GLfloat)v[0], (GLfloat)v[1], (GLfloat)v[2], (GLfloat)v[3]}; 
    gl4es_glVertexAttrib4fv(index, f); 
};
AliasExport(void,glVertexAttrib4dv,, (GLuint index, const GLdouble *v));

// Optimization: Precomputed constants for normalization
#define DIV_127   0.00787401574f
#define DIV_255   0.00392156862f
#define DIV_32767 3.0518509e-05f
#define DIV_65535 1.5259021e-05f
#define DIV_INT   4.6566129e-10f
#define DIV_UINT  2.3283064e-10f

#define THUNK(suffix, type, mul) \
void APIENTRY_GL4ES gl4es_glVertexAttrib4##suffix##v (GLuint index, const type *v) { \
    GLfloat f[4] = {(GLfloat)v[0], (GLfloat)v[1], (GLfloat)v[2], (GLfloat)v[3]}; \
    gl4es_glVertexAttrib4fv(index, f); \
}; \
AliasExport(void,glVertexAttrib4##suffix##v,, (GLuint index, const type *v)); \
void APIENTRY_GL4ES gl4es_glVertexAttrib4N##suffix##v (GLuint index, const type *v) { \
    GLfloat f[4] = {(GLfloat)v[0]*mul, (GLfloat)v[1]*mul, (GLfloat)v[2]*mul, (GLfloat)v[3]*mul}; \
    gl4es_glVertexAttrib4fv(index, f); \
}; \
AliasExport(void,glVertexAttrib4N##suffix##v,, (GLuint index, const type *v));

THUNK(b, GLbyte, DIV_127);
THUNK(ub, GLubyte, DIV_255);
THUNK(s, GLshort, DIV_32767);
THUNK(us, GLushort, DIV_65535);
THUNK(i, GLint, DIV_INT);
THUNK(ui, GLuint, DIV_UINT);
#undef THUNK

void APIENTRY_GL4ES gl4es_glVertexAttrib4Nub(GLuint index, GLubyte v0, GLubyte v1, GLubyte v2, GLubyte v3) {
    GLfloat f[4] = {(GLfloat)v0 * DIV_255, (GLfloat)v1 * DIV_255, (GLfloat)v2 * DIV_255, (GLfloat)v3 * DIV_255}; 
    gl4es_glVertexAttrib4fv(index, f); 
};
AliasExport(void,glVertexAttrib4Nub,,(GLuint index, GLubyte v0, GLubyte v1, GLubyte v2, GLubyte v3));

// ============= GL_ARB_vertex_shader =================
AliasExport(GLvoid,glVertexAttrib1f,ARB,(GLuint index, GLfloat v0));
AliasExport(GLvoid,glVertexAttrib1s,ARB,(GLuint index, GLshort v0));
AliasExport_D_1(GLvoid,glVertexAttrib1d,ARB,(GLuint index, GLdouble v0));
AliasExport(GLvoid,glVertexAttrib2f,ARB,(GLuint index, GLfloat v0, GLfloat v1));
AliasExport(GLvoid,glVertexAttrib2s,ARB,(GLuint index, GLshort v0, GLshort v1));
AliasExport_D_1(GLvoid,glVertexAttrib2d,ARB,(GLuint index, GLdouble v0, GLdouble v1));
AliasExport(GLvoid,glVertexAttrib3f,ARB,(GLuint index, GLfloat v0, GLfloat v1, GLfloat v2));
AliasExport(GLvoid,glVertexAttrib3s,ARB,(GLuint index, GLshort v0, GLshort v1, GLshort v2));
AliasExport_D_1(GLvoid,glVertexAttrib3d,ARB,(GLuint index, GLdouble v0, GLdouble v1, GLdouble v2));
AliasExport(GLvoid,glVertexAttrib4s,ARB,(GLuint index, GLshort v0, GLshort v1, GLshort v2, GLshort v3));
AliasExport_D_1(GLvoid,glVertexAttrib4d,ARB,(GLuint index, GLdouble v0, GLdouble v1, GLdouble v2, GLdouble v3));
AliasExport(GLvoid,glVertexAttrib4Nub,ARB,(GLuint index, GLubyte x, GLubyte y, GLubyte z, GLubyte w));

AliasExport(GLvoid,glVertexAttrib1fv,ARB,(GLuint index, const GLfloat *v));
AliasExport(GLvoid,glVertexAttrib1sv,ARB,(GLuint index, const GLshort *v));
AliasExport(GLvoid,glVertexAttrib1dv,ARB,(GLuint index, const GLdouble *v));
AliasExport(GLvoid,glVertexAttrib2fv,ARB,(GLuint index, const GLfloat *v));
AliasExport(GLvoid,glVertexAttrib2sv,ARB,(GLuint index, const GLshort *v));
AliasExport(GLvoid,glVertexAttrib2dv,ARB,(GLuint index, const GLdouble *v));
AliasExport(GLvoid,glVertexAttrib3fv,ARB,(GLuint index, const GLfloat *v));
AliasExport(GLvoid,glVertexAttrib3sv,ARB,(GLuint index, const GLshort *v));
AliasExport(GLvoid,glVertexAttrib3dv,ARB,(GLuint index, const GLdouble *v));
AliasExport(GLvoid,glVertexAttrib4sv,ARB,(GLuint index, const GLshort *v));
AliasExport(GLvoid,glVertexAttrib4dv,ARB,(GLuint index, const GLdouble *v));
AliasExport(GLvoid,glVertexAttrib4iv,ARB,(GLuint index, const GLint *v));
AliasExport(GLvoid,glVertexAttrib4bv,ARB,(GLuint index, const GLbyte *v));

AliasExport(GLvoid,glVertexAttrib4ubv,ARB,(GLuint index, const GLubyte *v));
AliasExport(GLvoid,glVertexAttrib4usv,ARB,(GLuint index, const GLushort *v));
AliasExport(GLvoid,glVertexAttrib4uiv,ARB,(GLuint index, const GLuint *v));

AliasExport(GLvoid,glVertexAttrib4Nbv,ARB,(GLuint index, const GLbyte *v));
AliasExport(GLvoid,glVertexAttrib4Nsv,ARB,(GLuint index, const GLshort *v));
AliasExport(GLvoid,glVertexAttrib4Niv,ARB,(GLuint index, const GLint *v));
AliasExport(GLvoid,glVertexAttrib4Nubv,ARB,(GLuint index, const GLubyte *v));
AliasExport(GLvoid,glVertexAttrib4Nusv,ARB,(GLuint index, const GLushort *v));
AliasExport(GLvoid,glVertexAttrib4Nuiv,ARB,(GLuint index, const GLuint *v));

//Direct wrapper
AliasExport_D(void,glClearDepth,,(GLdouble depth));
AliasExport(void,glClipPlane,,(GLenum plane, const GLdouble *equation));
AliasExport_D(void,glDepthRange,,(GLdouble nearVal, GLdouble farVal));
AliasExport(void,glFogi,,(GLenum pname, GLint param));
AliasExport(void,glFogiv,,(GLenum pname, GLint *params));
AliasExport_D(void,glFrustum,,(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble Near, GLdouble Far));
AliasExport(void,glLighti,,(GLenum light, GLenum pname, GLint param));
AliasExport(void,glLightiv,,(GLenum light, GLenum pname, GLint *iparams));
AliasExport(void,glLightModeli,,(GLenum pname, GLint param));
AliasExport(void,glLightModeliv,,(GLenum pname, GLint *iparams));
AliasExport(void,glMateriali,,(GLenum face, GLenum pname, GLint param));
AliasExport(void,glMaterialiv,,(GLenum face, GLenum pname, GLint *param));
AliasExport_D(void,glOrtho,,(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble Near, GLdouble Far));
AliasExport(void,glGetMaterialiv,,(GLenum face, GLenum pname, GLint * params));
AliasExport(void,glGetLightiv,,(GLenum light, GLenum pname, GLint * params));
AliasExport(void,glGetClipPlane,,(GLenum plane, GLdouble *equation));
AliasExport(void,glColor3f,,(GLfloat r, GLfloat g, GLfloat b));
AliasExport(void,glColor3fv,,(GLfloat *c));
AliasExport(void,glIndexfv,,(const GLfloat *c));
AliasExport(void,glSecondaryColor3fv,,(const GLfloat *v));
AliasExport(void,glRasterPos2f,,(GLfloat x, GLfloat y));
AliasExport(void,glRasterPos2fv,,(const GLfloat *v));
AliasExport(void,glRasterPos3fv,,(const GLfloat *v));
AliasExport(void,glRasterPos4f,,(GLfloat x, GLfloat y, GLfloat z, GLfloat w));
AliasExport(void,glRasterPos4fv,,(const GLfloat *v));
AliasExport(void,glWindowPos2f,,(GLfloat x, GLfloat y));
AliasExport(void,glWindowPos2fv,,(const GLfloat *v));
AliasExport(void,glWindowPos3fv,,(const GLfloat *v));
AliasExport(void,glPixelStoref,,(GLenum pname, GLfloat param));
AliasExport(void,glGetTexGendv,,(GLenum coord,GLenum pname,GLdouble *params));
AliasExport(void,glGetTexGeniv,,(GLenum coord,GLenum pname,GLint *params));
AliasExport(void,glPixelTransferi,,(GLenum pname, GLint param));
AliasExport_D(void,glEvalCoord1d,,(GLdouble u));
AliasExport(void,glEvalCoord1dv,,(GLdouble *v));
AliasExport(void,glEvalCoord1fv,,(GLfloat *v));
AliasExport_D(void,glEvalCoord2d,,(GLdouble u, GLdouble v));
AliasExport(void,glEvalCoord2dv,,(GLdouble *v));
AliasExport(void,glEvalCoord2fv,,(GLfloat *v));
AliasExport_D_1(void,glMapGrid1d,,(GLint un, GLdouble u1, GLdouble u2));
AliasExport_M(void,glMapGrid2d,,(GLint un, GLdouble u1, GLdouble u2, GLint vn, GLdouble v1, GLdouble v2),40);
AliasExport(void,glLoadMatrixd,,(const GLdouble *m));
AliasExport(void,glMultMatrixd,,(const GLdouble *m));

// rect
#define GL_RECT(suffix, type, M2)                                \
    AliasExport##M2(void,glRect##suffix,,(type x1, type y1, type x2, type y2)); \
    AliasExport(void,glRect##suffix##v,,(const type *v1, const type *v2));

GL_RECT(d, GLdouble, _D)
GL_RECT(f, GLfloat, )
GL_RECT(i, GLint, )
GL_RECT(s, GLshort, )
#undef GL_RECT

AliasExport(void,glTexCoord1f,,(GLfloat s));
AliasExport(void,glTexCoord1fv,,(GLfloat *t));
AliasExport(void,glTexCoord2f,,(GLfloat s, GLfloat t));
AliasExport(void,glTexCoord2fv,,(GLfloat *t));
AliasExport(void,glTexCoord3f,,(GLfloat s, GLfloat t, GLfloat r));
AliasExport(void,glTexCoord3fv,,(GLfloat *t));
AliasExport(void,glTexCoord4fv,,(GLfloat *t));
AliasExport(void,glMultiTexCoord1f,,(GLenum target, GLfloat s));
AliasExport(void,glMultiTexCoord1fv,,(GLenum target, GLfloat *t));
AliasExport(void,glMultiTexCoord2f,,(GLenum target, GLfloat s, GLfloat t));
AliasExport(void,glMultiTexCoord3f,,(GLenum target, GLfloat s, GLfloat t, GLfloat r));
AliasExport(void,glMultiTexCoord3fv,,(GLenum target, GLfloat *t));
AliasExport(void,glGetTexLevelParameteriv,,(GLenum target, GLint level, GLenum pname, GLfloat *params));
AliasExport_M(void,glTexGend,,(GLenum coord, GLenum pname, GLdouble param),16);
AliasExport(void,glTexGenf,,(GLenum coord, GLenum pname, GLfloat param));
AliasExport(void,glTexGendv,,(GLenum coord, GLenum pname, const GLdouble *params));
AliasExport(void,glTexGeniv,,(GLenum coord, GLenum pname, const GLint *params));
AliasExport_D(void,glRotated,,(GLdouble angle, GLdouble x, GLdouble y, GLdouble z));
AliasExport_D(void,glScaled,,(GLdouble x, GLdouble y, GLdouble z));
AliasExport_D(void,glTranslated,,(GLdouble x, GLdouble y, GLdouble z));
AliasExport(void,glVertex2f,,(GLfloat x, GLfloat y));
AliasExport(void,glVertex2fv,,(GLfloat *v));
AliasExport(void,glVertex3f,,(GLfloat r, GLfloat g, GLfloat b));

// basic thunking
// Macro re-enabled just for AliasExport generation, no logic inside
#define THUNK(suffix, type, M2)                                \
AliasExport(void,glColor3##suffix##v,,(const type *v));               \
AliasExport##M2(void,glColor3##suffix,,(type r, type g, type b));         \
AliasExport(void,glColor4##suffix##v,,(const type *v));               \
AliasExport##M2(void,glColor4##suffix,,(type r, type g, type b, type a)); \
AliasExport(void,glSecondaryColor3##suffix##v,,(const type *v));      \
AliasExport##M2(void,glSecondaryColor3##suffix,,(type r, type g, type b));\
AliasExport(void,glIndex##suffix##v,,(const type *c));                \
AliasExport##M2(void,glIndex##suffix,,(type c));                          \
AliasExport(void,glNormal3##suffix##v,,(const type *v));              \
AliasExport##M2(void,glNormal3##suffix,,(type x, type y, type z));        \
AliasExport(void,glRasterPos2##suffix##v,,(type *v));                 \
AliasExport##M2(void,glRasterPos2##suffix,,(type x, type y));             \
AliasExport(void,glRasterPos3##suffix##v,,(type *v));                 \
AliasExport##M2(void,glRasterPos3##suffix,,(type x, type y, type z));     \
AliasExport(void,glRasterPos4##suffix##v,,(type *v));                 \
AliasExport##M2(void,glRasterPos4##suffix,,(type x, type y, type z, type w));\
AliasExport(void,glWindowPos2##suffix##v,,(type *v));                 \
AliasExport##M2(void,glWindowPos2##suffix,,(type x, type y));             \
AliasExport(void,glWindowPos3##suffix##v,,(type *v));                 \
AliasExport##M2(void,glWindowPos3##suffix,,(type x, type y, type z));     \
AliasExport(void,glVertex2##suffix##v,,(type *v));                    \
AliasExport##M2(void,glVertex2##suffix,,(type x, type y));                \
AliasExport(void,glVertex3##suffix##v,,(type *v));                    \
AliasExport##M2(void,glVertex3##suffix,,(type x, type y, type z));        \
AliasExport##M2(void,glVertex4##suffix,,(type x, type y, type z, type w));\
AliasExport(void,glVertex4##suffix##v,,(type *v));                    \
AliasExport##M2(void,glTexCoord1##suffix,,(type s));                      \
AliasExport(void,glTexCoord1##suffix##v,,(type *t));                  \
AliasExport##M2(void,glTexCoord2##suffix,,(type s, type t));              \
AliasExport(void,glTexCoord2##suffix##v,,(type *t));                  \
AliasExport##M2(void,glTexCoord3##suffix,,(type s, type t, type r));      \
AliasExport(void,glTexCoord3##suffix##v,,(type *t));                  \
AliasExport##M2(void,glTexCoord4##suffix,,(type s, type t, type r, type q));          \
AliasExport(void,glTexCoord4##suffix##v,,(type *t));                              \
AliasExport##M2##_1(void,glMultiTexCoord1##suffix,,(GLenum target, type s));              \
AliasExport(void,glMultiTexCoord1##suffix##v,,(GLenum target, type *t));          \
AliasExport##M2##_1(void,glMultiTexCoord2##suffix,,(GLenum target, type s, type t));      \
AliasExport(void,glMultiTexCoord2##suffix##v,,(GLenum target, type *t));          \
AliasExport##M2##_1(void,glMultiTexCoord3##suffix,,(GLenum target, type s, type t, type r));\
AliasExport(void,glMultiTexCoord3##suffix##v,,(GLenum target, type *t));          \
AliasExport##M2##_1(void,glMultiTexCoord4##suffix,,(GLenum target, type s, type t, type r, type q));\
AliasExport(void,glMultiTexCoord4##suffix##v,,(GLenum target, type *t));          \
AliasExport##M2##_1(void,glMultiTexCoord1##suffix,ARB,(GLenum target, type s));         \
AliasExport(void,glMultiTexCoord1##suffix##v,ARB,(GLenum target, type *t));       \
AliasExport##M2##_1(void,glMultiTexCoord2##suffix,ARB,(GLenum target, type s, type t)); \
AliasExport(void,glMultiTexCoord2##suffix##v,ARB,(GLenum target, type *t));       \
AliasExport##M2##_1(void,glMultiTexCoord3##suffix,ARB,(GLenum target, type s, type t, type r));\
AliasExport(void,glMultiTexCoord3##suffix##v,ARB,(GLenum target, type *t));       \
AliasExport##M2##_1(void,glMultiTexCoord4##suffix,ARB,(GLenum target, type s, type t, type r, type q));\
AliasExport(void,glMultiTexCoord4##suffix##v,ARB,(GLenum target, type *t));

THUNK(b, GLbyte, )
THUNK(d, GLdouble, _D)
THUNK(i, GLint, )
THUNK(s, GLshort, )
THUNK(ub, GLubyte, )
THUNK(ui, GLuint, )
THUNK(us, GLushort, )
#undef THUNK

AliasExport(void,glMultiTexCoord1f,ARB,(GLenum target, GLfloat s));
AliasExport(void,glMultiTexCoord2f,ARB,(GLenum target, GLfloat s, GLfloat t));
AliasExport(void,glMultiTexCoord3f,ARB,(GLenum target, GLfloat s, GLfloat t, GLfloat r));
AliasExport(void,glMultiTexCoord1fv,ARB,(GLenum target, GLfloat *t));
AliasExport(void,glMultiTexCoord3fv,ARB,(GLenum target, GLfloat *t));
AliasExport(void,glGetTexParameterfv,,(GLenum target, GLenum pname, GLfloat * params));
AliasExport(void,glGetTexParameteriv,,(GLenum target, GLenum pname, GLint * params));