/*
 * Refactored matheval.c for GL4ES optimized for ARMv8
 * Based on original Mesa 3-D graphics library code
 * * OPTIMIZATIONS:
 * 1. Removed runtime initialization checks (Branch Prediction Optimization).
 * 2. Added 'restrict' keyword for pointer anti-aliasing (Auto-Vectorization).
 * 3. Precomputed inverse table lookup.
 * 4. Improved loop locality for cache efficiency.
 */

#include "eval.h"
#include <math.h>

/* * Precomputed Inverse Table (1/x) up to Order 40.
 * Eliminates the need for _math_init_eval() and runtime branching.
 * Standard OpenGL usually caps this lower, but 40 provides safety buffer.
 */
static const GLfloat inv_tab[] = {
    0.0f, 1.0f, 0.5f, 0.33333334f, 0.25f, 0.2f, 
    0.16666667f, 0.14285715f, 0.125f, 0.11111111f, 0.1f,
    0.09090909f, 0.08333333f, 0.07692308f, 0.07142857f, 0.06666667f,
    0.0625f, 0.05882353f, 0.05555556f, 0.05263158f, 0.05f,
    0.04761905f, 0.04545455f, 0.04347826f, 0.04166666f, 0.04f,
    0.03846154f, 0.03703704f, 0.03571429f, 0.03448276f, 0.03333333f,
    0.03225806f, 0.03125f, 0.03030303f, 0.02941176f, 0.02857143f,
    0.02777778f, 0.02702703f, 0.02631579f, 0.02564103f, 0.025f
};

/*
 * Horner scheme for Bezier curves
 * Optimized with 'restrict' and inline loop variables.
 */
void
_math_horner_bezier_curve(const GLfloat * restrict cp, GLfloat * restrict out, GLfloat t,
                          GLuint dim, GLuint order) {
    if (order < 2) {
        /* order=1 -> constant curve */
        for (GLuint k = 0; k < dim; k++) {
            out[k] = cp[k];
        }
        return;
    }

    GLfloat s = 1.0f - t;
    GLfloat bincoeff = (GLfloat)(order - 1);
    
    // First iteration unrolled manually for setup
    for (GLuint k = 0; k < dim; k++) {
        out[k] = s * cp[k] + bincoeff * t * cp[dim + k];
    }

    cp += 2 * dim;
    GLfloat powert = t * t;

    for (GLuint i = 2; i < order; i++) {
        bincoeff *= (GLfloat)(order - i);
        
        // Use lookup table directly, fallback if order exceeds table size (unlikely)
        if (i < sizeof(inv_tab)/sizeof(GLfloat)) {
            bincoeff *= inv_tab[i];
        } else {
            bincoeff /= (GLfloat)i;
        }

        for (GLuint k = 0; k < dim; k++) {
            out[k] = s * out[k] + bincoeff * powert * cp[k];
        }
        
        powert *= t;
        cp += dim;
    }
}

/*
 * Tensor product Bezier surfaces
 * Refactored for better stack usage and pointer arithmetic.
 */
void
_math_horner_bezier_surf(GLfloat * restrict cn, GLfloat * restrict out, GLfloat u, GLfloat v,
                         GLuint dim, GLuint uorder, GLuint vorder) {
    
    // Safety check for pointer validity removed for raw speed, assuming caller is sane.
    
    GLfloat *cp = cn + uorder * vorder * dim;
    GLuint uinc = vorder * dim;

    if (vorder > uorder) {
        if (uorder >= 2) {
            GLfloat s = 1.0f - u;
            GLfloat bincoeff;
            GLfloat poweru;

            /* Compute the control polygon for the surface-curve in u-direction */
            for (GLuint j = 0; j < vorder; j++) {
                GLfloat *ucp = &cn[j * dim];
                GLfloat *target_cp = &cp[j * dim];

                bincoeff = (GLfloat)(uorder - 1);

                for (GLuint k = 0; k < dim; k++) {
                    target_cp[k] = s * ucp[k] + bincoeff * u * ucp[uinc + k];
                }

                ucp += 2 * uinc;
                poweru = u * u;

                for (GLuint i = 2; i < uorder; i++) {
                    bincoeff *= (GLfloat)(uorder - i);
                    if (i < sizeof(inv_tab)/sizeof(GLfloat)) bincoeff *= inv_tab[i];
                    else bincoeff /= (GLfloat)i;

                    for (GLuint k = 0; k < dim; k++) {
                        target_cp[k] = s * target_cp[k] + bincoeff * poweru * ucp[k];
                    }
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
    } else {  
        /* vorder <= uorder */
        if (vorder > 1) {
            /* Compute the control polygon for the surface-curve in u-direction */
            // Optimized loop: directly point to destination buffer
            for (GLuint i = 0; i < uorder; i++) {
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
 * de Casteljau algorithm for surfaces
 * Macros cleaned up and logic streamlined.
 */
void
_math_de_casteljau_surf(GLfloat * restrict cn, GLfloat * restrict out, GLfloat * restrict du,
                        GLfloat * restrict dv, GLfloat u, GLfloat v, GLuint dim,
                        GLuint uorder, GLuint vorder) {
    
    GLfloat *dcn = cn + uorder * vorder * dim;
    GLfloat us = 1.0f - u, vs = 1.0f - v;
    GLuint minorder = (uorder < vorder) ? uorder : vorder;
    GLuint uinc = vorder * dim;
    GLuint dcuinc = vorder;

    // Helper macros for strided access (Local scope only)
    #define CN(I,J,K) cn[(I)*uinc + (J)*dim + (K)]
    #define DCN(I, J) dcn[(I)*dcuinc + (J)]

    if (minorder < 3) {
        if (uorder == vorder) {
            // Case: uorder == vorder == 2 (Bilinear)
             for (GLuint k = 0; k < dim; k++) {
                GLfloat c00 = CN(0, 0, k);
                GLfloat c01 = CN(0, 1, k);
                GLfloat c10 = CN(1, 0, k);
                GLfloat c11 = CN(1, 1, k);

                du[k] = vs * (c10 - c00) + v * (c11 - c01);
                dv[k] = us * (c01 - c00) + u * (c11 - c10);
                out[k] = us * (vs * c00 + v * c01) + u * (vs * c10 + v * c11);
            }
        } else if (minorder == uorder) {
             for (GLuint k = 0; k < dim; k++) {
                DCN(1, 0) = CN(1, 0, k) - CN(0, 0, k);
                DCN(0, 0) = us * CN(0, 0, k) + u * CN(1, 0, k);

                for (GLuint j = 0; j < vorder - 1; j++) {
                    DCN(1, j + 1) = CN(1, j + 1, k) - CN(0, j + 1, k);
                    DCN(1, j) = vs * DCN(1, j) + v * DCN(1, j + 1);

                    DCN(0, j + 1) = us * CN(0, j + 1, k) + u * CN(1, j + 1, k);
                    DCN(0, j) = vs * DCN(0, j) + v * DCN(0, j + 1);
                }

                for (GLuint h = minorder; h < vorder - 1; h++) {
                    for (GLuint j = 0; j < vorder - h; j++) {
                        DCN(1, j) = vs * DCN(1, j) + v * DCN(1, j + 1);
                        DCN(0, j) = vs * DCN(0, j) + v * DCN(0, j + 1);
                    }
                }

                dv[k] = DCN(0, 1) - DCN(0, 0);
                du[k] = vs * DCN(1, 0) + v * DCN(1, 1);
                out[k] = vs * DCN(0, 0) + v * DCN(0, 1);
            }
        } else { 
            // minorder == vorder
             for (GLuint k = 0; k < dim; k++) {
                DCN(0, 1) = CN(0, 1, k) - CN(0, 0, k);
                DCN(0, 0) = vs * CN(0, 0, k) + v * CN(0, 1, k);
                
                for (GLuint i = 0; i < uorder - 1; i++) {
                    DCN(i + 1, 1) = CN(i + 1, 1, k) - CN(i + 1, 0, k);
                    DCN(i, 1) = us * DCN(i, 1) + u * DCN(i + 1, 1);

                    DCN(i + 1, 0) = vs * CN(i + 1, 0, k) + v * CN(i + 1, 1, k);
                    DCN(i, 0) = us * DCN(i, 0) + u * DCN(i + 1, 0);
                }

                for (GLuint h = minorder; h < uorder - 1; h++) {
                    for (GLuint i = 0; i < uorder - h; i++) {
                        DCN(i, 1) = us * DCN(i, 1) + u * DCN(i + 1, 1);
                        DCN(i, 0) = us * DCN(i, 0) + u * DCN(i + 1, 0);
                    }
                }

                du[k] = DCN(1, 0) - DCN(0, 0);
                dv[k] = us * DCN(0, 1) + u * DCN(1, 1);
                out[k] = us * DCN(0, 0) + u * DCN(1, 0);
            }
        }
    } else {
        // Higher order surfaces
        for (GLuint k = 0; k < dim; k++) {
            if (uorder == vorder) {
                for (GLuint i = 0; i < uorder - 1; i++) {
                    DCN(i, 0) = us * CN(i, 0, k) + u * CN(i + 1, 0, k);
                    for (GLuint j = 0; j < vorder - 1; j++) {
                        DCN(i, j + 1) = us * CN(i, j + 1, k) + u * CN(i + 1, j + 1, k);
                        DCN(i, j) = vs * DCN(i, j) + v * DCN(i, j + 1);
                    }
                }
                for (GLuint h = 2; h < minorder - 1; h++) {
                    for (GLuint i = 0; i < uorder - h; i++) {
                        DCN(i, 0) = us * DCN(i, 0) + u * DCN(i + 1, 0);
                        for (GLuint j = 0; j < vorder - h; j++) {
                            DCN(i, j + 1) = us * DCN(i, j + 1) + u * DCN(i + 1, j + 1);
                            DCN(i, j) = vs * DCN(i, j) + v * DCN(i, j + 1);
                        }
                    }
                }
                du[k] = vs * (DCN(1, 0) - DCN(0, 0)) + v * (DCN(1, 1) - DCN(0, 1));
                dv[k] = us * (DCN(0, 1) - DCN(0, 0)) + u * (DCN(1, 1) - DCN(1, 0));
                out[k] = us * (vs * DCN(0, 0) + v * DCN(0, 1)) + u * (vs * DCN(1, 0) + v * DCN(1, 1));
            
            } else if (minorder == uorder) {
                // Similar logic for mixed orders, variables scoped locally
                for (GLuint i = 0; i < uorder - 1; i++) {
                    DCN(i, 0) = us * CN(i, 0, k) + u * CN(i + 1, 0, k);
                    for (GLuint j = 0; j < vorder - 1; j++) {
                        DCN(i, j + 1) = us * CN(i, j + 1, k) + u * CN(i + 1, j + 1, k);
                        DCN(i, j) = vs * DCN(i, j) + v * DCN(i, j + 1);
                    }
                }
                for (GLuint h = 2; h < minorder - 1; h++) {
                    for (GLuint i = 0; i < uorder - h; i++) {
                        DCN(i, 0) = us * DCN(i, 0) + u * DCN(i + 1, 0);
                        for (GLuint j = 0; j < vorder - h; j++) {
                            DCN(i, j + 1) = us * DCN(i, j + 1) + u * DCN(i + 1, j + 1);
                            DCN(i, j) = vs * DCN(i, j) + v * DCN(i, j + 1);
                        }
                    }
                }
                
                DCN(2, 0) = DCN(1, 0) - DCN(0, 0);
                DCN(0, 0) = us * DCN(0, 0) + u * DCN(1, 0);
                for (GLuint j = 0; j < vorder - 1; j++) {
                    DCN(2, j + 1) = DCN(1, j + 1) - DCN(0, j + 1);
                    DCN(2, j) = vs * DCN(2, j) + v * DCN(2, j + 1);
                    DCN(0, j + 1) = us * DCN(0, j + 1) + u * DCN(1, j + 1);
                    DCN(0, j) = vs * DCN(0, j) + v * DCN(0, j + 1);
                }
                for (GLuint h = minorder; h < vorder - 1; h++) {
                    for (GLuint j = 0; j < vorder - h; j++) {
                        DCN(2, j) = vs * DCN(2, j) + v * DCN(2, j + 1);
                        DCN(0, j) = vs * DCN(0, j) + v * DCN(0, j + 1);
                    }
                }
                dv[k] = DCN(0, 1) - DCN(0, 0);
                du[k] = vs * DCN(2, 0) + v * DCN(2, 1);
                out[k] = vs * DCN(0, 0) + v * DCN(0, 1);

            } else { // minorder == vorder
                for (GLuint i = 0; i < uorder - 1; i++) {
                    DCN(i, 0) = us * CN(i, 0, k) + u * CN(i + 1, 0, k);
                    for (GLuint j = 0; j < vorder - 1; j++) {
                        DCN(i, j + 1) = us * CN(i, j + 1, k) + u * CN(i + 1, j + 1, k);
                        DCN(i, j) = vs * DCN(i, j) + v * DCN(i, j + 1);
                    }
                }
                for (GLuint h = 2; h < minorder - 1; h++) {
                    for (GLuint i = 0; i < uorder - h; i++) {
                        DCN(i, 0) = us * DCN(i, 0) + u * DCN(i + 1, 0);
                        for (GLuint j = 0; j < vorder - h; j++) {
                            DCN(i, j + 1) = us * DCN(i, j + 1) + u * DCN(i + 1, j + 1);
                            DCN(i, j) = vs * DCN(i, j) + v * DCN(i, j + 1);
                        }
                    }
                }

                DCN(0, 2) = DCN(0, 1) - DCN(0, 0);
                DCN(0, 0) = vs * DCN(0, 0) + v * DCN(0, 1);
                for (GLuint i = 0; i < uorder - 1; i++) {
                    DCN(i + 1, 2) = DCN(i + 1, 1) - DCN(i + 1, 0);
                    DCN(i, 2) = us * DCN(i, 2) + u * DCN(i + 1, 2);
                    DCN(i + 1, 0) = vs * DCN(i + 1, 0) + v * DCN(i + 1, 1);
                    DCN(i, 0) = us * DCN(i, 0) + u * DCN(i + 1, 0);
                }
                for (GLuint h = minorder; h < uorder - 1; h++) {
                    for (GLuint i = 0; i < uorder - h; i++) {
                        DCN(i, 2) = us * DCN(i, 2) + u * DCN(i + 1, 2);
                        DCN(i, 0) = us * DCN(i, 0) + u * DCN(i + 1, 0);
                    }
                }
                du[k] = DCN(1, 0) - DCN(0, 0);
                dv[k] = us * DCN(0, 2) + u * DCN(1, 2);
                out[k] = us * DCN(0, 0) + u * DCN(1, 0);
            }
        }
    }
    #undef DCN
    #undef CN
}

/* * _math_init_eval is deprecated/removed.
 * But we keep an empty stub in case header files reference it externally.
 */
void _math_init_eval() {
    // No-op: Initialization is now static.
}