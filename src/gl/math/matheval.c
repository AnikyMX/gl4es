/*
 * Modernized for GL4ES (ARM64 Optimization)
 * Based on Mesa 3-D graphics library v3.5
 */

#include "eval.h"
#include <math.h>
#include <stdbool.h>

/* Hint untuk Branch Prediction: Init biasanya sudah selesai */
#define LIKELY(x)   __builtin_expect(!!(x), 1)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)

static bool init_done = false;
static GLfloat inv_tab[MAX_EVAL_ORDER];

/*
 * Do one-time initialization for evaluators.
 */
void _math_init_eval() {
   if (LIKELY(init_done)) return;

   GLuint i;
   /* KW: precompute 1/x for useful x. */
   for (i = 1; i < MAX_EVAL_ORDER; i++)
      inv_tab[i] = 1.0f / (GLfloat)i;

   init_done = true;
}

/*
 * Helper inline agar compiler lebih mudah melakukan vectorization (NEON)
 * pada operasi dimensi (k). Gunakan 'restrict' untuk optimasi memori.
 */
static inline void accum_dim(GLfloat * __restrict out, const GLfloat * __restrict cp, 
                             GLfloat s, GLfloat val, GLuint dim) {
    for (GLuint k = 0; k < dim; k++) {
        out[k] = s * cp[k] + val * cp[dim + k];
    }
}

static inline void accum_dim_horner(GLfloat * __restrict out, const GLfloat * __restrict cp, 
                                    GLfloat s, GLfloat bincoeff, GLfloat powert, GLuint dim) {
    for (GLuint k = 0; k < dim; k++) {
        out[k] = s * out[k] + bincoeff * powert * cp[k];
    }
}

/*
 * Horner scheme for Bezier curves
 */
void
_math_horner_bezier_curve(const GLfloat * __restrict cp, GLfloat * __restrict out, GLfloat t,
                          GLuint dim, GLuint order) {
    if (UNLIKELY(!init_done))
        _math_init_eval();

    GLfloat s, powert, bincoeff;
    GLuint i, k;

    if (order >= 2) {
        bincoeff = (GLfloat) (order - 1);
        s = 1.0f - t;
        
        // Initial Step
        accum_dim(out, cp, s, bincoeff * t, dim);

        // Loop Horner
        cp += 2 * dim;
        powert = t * t;
        
        for (i = 2; i < order; i++) {
            bincoeff *= (GLfloat) (order - i);
            bincoeff *= inv_tab[i];

            accum_dim_horner(out, cp, s, bincoeff, powert, dim);

            powert *= t;
            cp += dim;
        }
    } else {  /* order=1 -> constant curve */
        for (k = 0; k < dim; k++)
            out[k] = cp[k];
    }
}

/*
 * Tensor product Bezier surfaces
 */
void
_math_horner_bezier_surf(GLfloat * __restrict cn, GLfloat * __restrict out, GLfloat u, GLfloat v,
                          GLuint dim, GLuint uorder, GLuint vorder) {
    if (UNLIKELY(!init_done))
        _math_init_eval();

    // Pointer ke temporary storage (biasanya disediakan caller via cn)
    GLfloat *cp = cn + uorder * vorder * dim;
    GLuint uinc = vorder * dim;

    if (vorder > uorder) {
        if (uorder >= 2) {
            GLfloat s, poweru, bincoeff;
            GLuint j;

            /* Compute the control polygon for the surface-curve in u-direction */
            for (j = 0; j < vorder; j++) {
                GLfloat *ucp = &cn[j * dim];
                GLfloat *target_cp = &cp[j * dim];

                bincoeff = (GLfloat) (uorder - 1);
                s = 1.0f - u;

                accum_dim(target_cp, ucp, s, bincoeff * u, dim);

                ucp += 2 * uinc;
                poweru = u * u;

                for (GLuint i = 2; i < uorder; i++) {
                    bincoeff *= (GLfloat) (uorder - i);
                    bincoeff *= inv_tab[i];

                    accum_dim_horner(target_cp, ucp, s, bincoeff, poweru, dim);

                    poweru *= u;
                    ucp += uinc;
                }
            }

            /* Evaluate curve point in v */
            _math_horner_bezier_curve(cp, out, v, dim, vorder);
        } else {
             /* uorder=1 -> cn defines a curve in v */
            _math_horner_bezier_curve(cn, out, v, dim, vorder);
        }
    } else {  /* vorder <= uorder */

        if (vorder > 1) {
            GLuint i;
            /* Compute the control polygon for the surface-curve in u-direction */
            for (i = 0; i < uorder; i++) {
                _math_horner_bezier_curve(&cn[i * uinc], &cp[i * dim], v, dim, vorder);
            }

            /* Evaluate curve point in u */
            _math_horner_bezier_curve(cp, out, u, dim, uorder);
        } else { 
            /* vorder=1 -> cn defines a curve in u */
            _math_horner_bezier_curve(cn, out, u, dim, uorder);
        }
    }
}

/*
 * The direct de Casteljau algorithm
 * Optimized macros for readability and cache locality
 */

void
_math_de_casteljau_surf(GLfloat * __restrict cn, GLfloat * __restrict out, GLfloat * __restrict du,
                        GLfloat * __restrict dv, GLfloat u, GLfloat v, GLuint dim,
                        GLuint uorder, GLuint vorder) {
    
    GLfloat *dcn = cn + uorder * vorder * dim;
    GLfloat us = 1.0f - u, vs = 1.0f - v;
    GLuint h, i, j, k;
    GLuint minorder = uorder < vorder ? uorder : vorder;
    GLuint uinc = vorder * dim;
    GLuint dcuinc = vorder;

/* Macros simplified for compiler optimization */
#define CN(I,J,K) cn[(I)*uinc + (J)*dim + (K)]
#define DCN(I, J) dcn[(I)*dcuinc + (J)]

    /* * Optimasi: Loop 'k' (dimensi) diletakkan di dalam block.
     * Compiler akan lebih mudah melakukan unrolling/vectorizing.
     */

    if (minorder < 3) {
        if (uorder == vorder) {
            for (k = 0; k < dim; k++) {
                // Pre-calc common terms to reduce registers usage
                GLfloat cn00 = CN(0,0,k);
                GLfloat cn01 = CN(0,1,k);
                GLfloat cn10 = CN(1,0,k);
                GLfloat cn11 = CN(1,1,k);

                du[k] = vs * (cn10 - cn00) + v * (cn11 - cn01);
                dv[k] = us * (cn01 - cn00) + u * (cn11 - cn10);
                out[k] = us * (vs * cn00 + v * cn01) + u * (vs * cn10 + v * cn11);
            }
        } else if (minorder == uorder) {
            for (k = 0; k < dim; k++) {
                DCN(1, 0) = CN(1, 0, k) - CN(0, 0, k);
                DCN(0, 0) = us * CN(0, 0, k) + u * CN(1, 0, k);

                for (j = 0; j < vorder - 1; j++) {
                    DCN(1, j + 1) = CN(1, j + 1, k) - CN(0, j + 1, k);
                    DCN(1, j) = vs * DCN(1, j) + v * DCN(1, j + 1);

                    DCN(0, j + 1) = us * CN(0, j + 1, k) + u * CN(1, j + 1, k);
                    DCN(0, j) = vs * DCN(0, j) + v * DCN(0, j + 1);
                }

                for (h = minorder; h < vorder - 1; h++) {
                    for (j = 0; j < vorder - h; j++) {
                        DCN(1, j) = vs * DCN(1, j) + v * DCN(1, j + 1);
                        DCN(0, j) = vs * DCN(0, j) + v * DCN(0, j + 1);
                    }
                }
                dv[k] = DCN(0, 1) - DCN(0, 0);
                du[k] = vs * DCN(1, 0) + v * DCN(1, 1);
                out[k] = vs * DCN(0, 0) + v * DCN(0, 1);
            }
        } else {  /* minorder == vorder */
            for (k = 0; k < dim; k++) {
                DCN(0, 1) = CN(0, 1, k) - CN(0, 0, k);
                DCN(0, 0) = vs * CN(0, 0, k) + v * CN(0, 1, k);
                for (i = 0; i < uorder - 1; i++) {
                    DCN(i + 1, 1) = CN(i + 1, 1, k) - CN(i + 1, 0, k);
                    DCN(i, 1) = us * DCN(i, 1) + u * DCN(i + 1, 1);

                    DCN(i + 1, 0) = vs * CN(i + 1, 0, k) + v * CN(i + 1, 1, k);
                    DCN(i, 0) = us * DCN(i, 0) + u * DCN(i + 1, 0);
                }

                for (h = minorder; h < uorder - 1; h++) {
                    for (i = 0; i < uorder - h; i++) {
                        DCN(i, 1) = us * DCN(i, 1) + u * DCN(i + 1, 1);
                        DCN(i, 0) = us * DCN(i, 0) + u * DCN(i + 1, 0);
                    }
                }
                du[k] = DCN(1, 0) - DCN(0, 0);
                dv[k] = us * DCN(0, 1) + u * DCN(1, 1);
                out[k] = us * DCN(0, 0) + u * DCN(1, 0);
            }
        }
    } else if (uorder == vorder) {
        // High order, equal dimensions
        for (k = 0; k < dim; k++) {
            for (i = 0; i < uorder - 1; i++) {
                DCN(i, 0) = us * CN(i, 0, k) + u * CN(i + 1, 0, k);
                for (j = 0; j < vorder - 1; j++) {
                    DCN(i, j + 1) = us * CN(i, j + 1, k) + u * CN(i + 1, j + 1, k);
                    DCN(i, j) = vs * DCN(i, j) + v * DCN(i, j + 1);
                }
            }
            for (h = 2; h < minorder - 1; h++) {
                for (i = 0; i < uorder - h; i++) {
                    DCN(i, 0) = us * DCN(i, 0) + u * DCN(i + 1, 0);
                    for (j = 0; j < vorder - h; j++) {
                        DCN(i, j + 1) = us * DCN(i, j + 1) + u * DCN(i + 1, j + 1);
                        DCN(i, j) = vs * DCN(i, j) + v * DCN(i, j + 1);
                    }
                }
            }
            du[k] = vs * (DCN(1, 0) - DCN(0, 0)) + v * (DCN(1, 1) - DCN(0, 1));
            dv[k] = us * (DCN(0, 1) - DCN(0, 0)) + u * (DCN(1, 1) - DCN(1, 0));
            out[k] = us * (vs * DCN(0, 0) + v * DCN(0, 1)) + u * (vs * DCN(1, 0) + v * DCN(1, 1));
        }
    } else if (minorder == uorder) {
        // High order, uorder < vorder
        for (k = 0; k < dim; k++) {
            for (i = 0; i < uorder - 1; i++) {
                DCN(i, 0) = us * CN(i, 0, k) + u * CN(i + 1, 0, k);
                for (j = 0; j < vorder - 1; j++) {
                    DCN(i, j + 1) = us * CN(i, j + 1, k) + u * CN(i + 1, j + 1, k);
                    DCN(i, j) = vs * DCN(i, j) + v * DCN(i, j + 1);
                }
            }
            for (h = 2; h < minorder - 1; h++) {
                for (i = 0; i < uorder - h; i++) {
                    DCN(i, 0) = us * DCN(i, 0) + u * DCN(i + 1, 0);
                    for (j = 0; j < vorder - h; j++) {
                        DCN(i, j + 1) = us * DCN(i, j + 1) + u * DCN(i + 1, j + 1);
                        DCN(i, j) = vs * DCN(i, j) + v * DCN(i, j + 1);
                    }
                }
            }
            DCN(2, 0) = DCN(1, 0) - DCN(0, 0);
            DCN(0, 0) = us * DCN(0, 0) + u * DCN(1, 0);
            for (j = 0; j < vorder - 1; j++) {
                DCN(2, j + 1) = DCN(1, j + 1) - DCN(0, j + 1);
                DCN(2, j) = vs * DCN(2, j) + v * DCN(2, j + 1);

                DCN(0, j + 1) = us * DCN(0, j + 1) + u * DCN(1, j + 1);
                DCN(0, j) = vs * DCN(0, j) + v * DCN(0, j + 1);
            }
            for (h = minorder; h < vorder - 1; h++) {
                for (j = 0; j < vorder - h; j++) {
                    DCN(2, j) = vs * DCN(2, j) + v * DCN(2, j + 1);
                    DCN(0, j) = vs * DCN(0, j) + v * DCN(0, j + 1);
                }
            }
            dv[k] = DCN(0, 1) - DCN(0, 0);
            du[k] = vs * DCN(2, 0) + v * DCN(2, 1);
            out[k] = vs * DCN(0, 0) + v * DCN(0, 1);
        }
    } else { 
        /* minorder == vorder, High order */
        for (k = 0; k < dim; k++) {
            for (i = 0; i < uorder - 1; i++) {
                DCN(i, 0) = us * CN(i, 0, k) + u * CN(i + 1, 0, k);
                for (j = 0; j < vorder - 1; j++) {
                    DCN(i, j + 1) = us * CN(i, j + 1, k) + u * CN(i + 1, j + 1, k);
                    DCN(i, j) = vs * DCN(i, j) + v * DCN(i, j + 1);
                }
            }
            for (h = 2; h < minorder - 1; h++) {
                for (i = 0; i < uorder - h; i++) {
                    DCN(i, 0) = us * DCN(i, 0) + u * DCN(i + 1, 0);
                    for (j = 0; j < vorder - h; j++) {
                        DCN(i, j + 1) = us * DCN(i, j + 1) + u * DCN(i + 1, j + 1);
                        DCN(i, j) = vs * DCN(i, j) + v * DCN(i, j + 1);
                    }
                }
            }
            DCN(0, 2) = DCN(0, 1) - DCN(0, 0);
            DCN(0, 0) = vs * DCN(0, 0) + v * DCN(0, 1);
            for (i = 0; i < uorder - 1; i++) {
                DCN(i + 1, 2) = DCN(i + 1, 1) - DCN(i + 1, 0);
                DCN(i, 2) = us * DCN(i, 2) + u * DCN(i + 1, 2);

                DCN(i + 1, 0) = vs * DCN(i + 1, 0) + v * DCN(i + 1, 1);
                DCN(i, 0) = us * DCN(i, 0) + u * DCN(i + 1, 0);
            }
            for (h = minorder; h < uorder - 1; h++) {
                for (i = 0; i < uorder - h; i++) {
                    DCN(i, 2) = us * DCN(i, 2) + u * DCN(i + 1, 2);
                    DCN(i, 0) = us * DCN(i, 0) + u * DCN(i + 1, 0);
                }
            }
            du[k] = DCN(1, 0) - DCN(0, 0);
            dv[k] = us * DCN(0, 2) + u * DCN(1, 2);
            out[k] = us * DCN(0, 0) + u * DCN(1, 0);
        }
    }
#undef DCN
#undef CN
}