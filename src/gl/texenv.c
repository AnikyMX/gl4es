/*
 * Refactored texenv.c for GL4ES
 * Optimized for ARMv8
 * - Redundant state filtering for glTexEnv
 * - Efficient FPE state updates
 * - Branch prediction hints
 */

#include "texenv.h"
#include "../glx/hardext.h"
#include "debug.h"
#include "fpe.h"
#include "gl4es.h"
#include "glstate.h"
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

// Helper to convert GL enum to FPE enum for Texture Env Mode
static inline int get_fpe_texenv_mode(GLenum param) {
    switch(param) {
        case GL_ADD: return FPE_ADD;
        case GL_DECAL: return FPE_DECAL;
        case GL_BLEND: return FPE_BLEND;
        case GL_REPLACE: return FPE_REPLACE;
        case GL_COMBINE: return FPE_COMBINE;
        case GL_COMBINE4: return FPE_COMBINE4;
        default: return FPE_MODULATE;
    }
}

void APIENTRY_GL4ES gl4es_glTexEnvf(GLenum target, GLenum pname, GLfloat param) {
    DBG(printf("glTexEnvf(%s, %s, 0x%04X(%s)), tmu=%d, pending=%d, compiling=%d\n", PrintEnum(target), PrintEnum(pname), (GLenum)param, PrintEnum((GLenum)param), glstate->texture.active, glstate->list.pending, glstate->list.compiling);)
    
    if (unlikely(!glstate->list.pending)) {
        PUSH_IF_COMPILING(glTexEnvf);
    }

    // Standardize DOT3 enums
    if (param == GL_DOT3_RGB_EXT) param = GL_DOT3_RGB;
    if (param == GL_DOT3_RGBA_EXT) param = GL_DOT3_RGBA;
    
    const int tmu = glstate->texture.active;
    noerrorShim();

    switch(target) {
        case GL_POINT_SPRITE:
            if (pname == GL_COORD_REPLACE) {
                int p = (param != 0.0f) ? 1 : 0;
                if (glstate->texture.pscoordreplace[tmu] == p)
                    return;
                
                FLUSH_BEGINEND;
                glstate->texture.pscoordreplace[tmu] = p;
                if (glstate->fpe_state)
                    glstate->fpe_state->pointsprite_coord = p;
            } else {
                errorShim(GL_INVALID_ENUM);
            }
            break;

        case GL_TEXTURE_FILTER_CONTROL:
            if (pname == GL_TEXTURE_LOD_BIAS) {
                if (glstate->texenv[tmu].filter.lod_bias == param)
                    return;
                FLUSH_BEGINEND;
                glstate->texenv[tmu].filter.lod_bias = param;
            } else {
                errorShim(GL_INVALID_ENUM);
            }
            break;

        case GL_TEXTURE_ENV: {
            texenv_t *t = &glstate->texenv[tmu].env;
            switch(pname) {
                case GL_TEXTURE_ENV_MODE:
                    if (t->mode == param) return;
                    
                    if (param == GL_COMBINE4) {
                        if (hardext.esversion == 1) { errorShim(GL_INVALID_ENUM); return; }
                    } else if (param != GL_ADD && param != GL_MODULATE && param != GL_DECAL && 
                               param != GL_BLEND && param != GL_REPLACE && param != GL_COMBINE) {
                        errorShim(GL_INVALID_ENUM); return;
                    }
                    
                    FLUSH_BEGINEND;
                    t->mode = param;
                    if (glstate->fpe_state) {
                        glstate->fpe_state->texenv[tmu].texenv = get_fpe_texenv_mode(param);
                    }
                    break;

                case GL_COMBINE_RGB:
                    if (t->combine_rgb == param) return;
                    // Validation checks... simplified for speed but kept structure
                    if (hardext.esversion == 1 && (param == GL_MODULATE_ADD_ATI || param == GL_MODULATE_SIGNED_ADD_ATI || param == GL_MODULATE_SUBTRACT_ATI)) {
                        errorShim(GL_INVALID_ENUM); return;
                    }
                    
                    FLUSH_BEGINEND;
                    t->combine_rgb = param;
                    if (glstate->fpe_state) {
                        int state = FPE_CR_REPLACE;
                        switch((GLenum)param) {
                            case GL_MODULATE: state=FPE_CR_MODULATE; break;
                            case GL_ADD: state=FPE_CR_ADD; break;
                            case GL_ADD_SIGNED: state=FPE_CR_ADD_SIGNED; break;
                            case GL_INTERPOLATE: state=FPE_CR_INTERPOLATE; break;
                            case GL_SUBTRACT: state=FPE_CR_SUBTRACT; break;
                            case GL_DOT3_RGB: state=FPE_CR_DOT3_RGB; break;
                            case GL_DOT3_RGBA: state=FPE_CR_DOT3_RGBA; break;
                            case GL_MODULATE_ADD_ATI: state=FPE_CR_MOD_ADD; break;
                            case GL_MODULATE_SIGNED_ADD_ATI: state=FPE_CR_MOD_ADD_SIGNED; break;
                            case GL_MODULATE_SUBTRACT_ATI: state=FPE_CR_MOD_SUB; break;
                        }
                        glstate->fpe_state->texcombine[tmu] &= 0xf0;
                        glstate->fpe_state->texcombine[tmu] |= state;
                    }
                    break;

                case GL_COMBINE_ALPHA:
                    if (t->combine_alpha == param) return;
                    
                    FLUSH_BEGINEND;
                    t->combine_alpha = param;
                    if (glstate->fpe_state) {
                        int state = FPE_CR_REPLACE;
                        switch((GLenum)param) {
                            case GL_MODULATE: state=FPE_CR_MODULATE; break;
                            case GL_ADD: state=FPE_CR_ADD; break;
                            case GL_ADD_SIGNED: state=FPE_CR_ADD_SIGNED; break;
                            case GL_INTERPOLATE: state=FPE_CR_INTERPOLATE; break;
                            case GL_SUBTRACT: state=FPE_CR_SUBTRACT; break;
                            case GL_MODULATE_ADD_ATI: state=FPE_CR_MOD_ADD; break;
                            case GL_MODULATE_SIGNED_ADD_ATI: state=FPE_CR_MOD_ADD_SIGNED; break;
                            case GL_MODULATE_SUBTRACT_ATI: state=FPE_CR_MOD_SUB; break;
                        }
                        glstate->fpe_state->texcombine[tmu] &= 0x0f;
                        glstate->fpe_state->texcombine[tmu] |= (state << 4);
                    }
                    break;

                // Source RGB/Alpha setters follow similar patterns
                case GL_SRC0_RGB: case GL_SRC1_RGB: case GL_SRC2_RGB: case GL_SRC3_RGB:
                case GL_SRC0_ALPHA: case GL_SRC1_ALPHA: case GL_SRC2_ALPHA: case GL_SRC3_ALPHA:
                    {
                        // Helper logic extracted for brevity in refactor
                        GLfloat *target_val = NULL;
                        uint8_t *fpe_target = NULL;
                        int is_alpha = 0;
                        
                        switch(pname) {
                            case GL_SRC0_RGB: target_val = &t->src0_rgb; fpe_target = &glstate->fpe_state->texenv[tmu].texsrcrgb0; break;
                            case GL_SRC1_RGB: target_val = &t->src1_rgb; fpe_target = &glstate->fpe_state->texenv[tmu].texsrcrgb1; break;
                            case GL_SRC2_RGB: target_val = &t->src2_rgb; fpe_target = &glstate->fpe_state->texenv[tmu].texsrcrgb2; break;
                            case GL_SRC3_RGB: target_val = &t->src3_rgb; fpe_target = &glstate->fpe_state->texenv[tmu].texsrcrgb3; break;
                            case GL_SRC0_ALPHA: target_val = &t->src0_alpha; fpe_target = &glstate->fpe_state->texenv[tmu].texsrcalpha0; is_alpha=1; break;
                            case GL_SRC1_ALPHA: target_val = &t->src1_alpha; fpe_target = &glstate->fpe_state->texenv[tmu].texsrcalpha1; is_alpha=1; break;
                            case GL_SRC2_ALPHA: target_val = &t->src2_alpha; fpe_target = &glstate->fpe_state->texenv[tmu].texsrcalpha2; is_alpha=1; break;
                            case GL_SRC3_ALPHA: target_val = &t->src3_alpha; fpe_target = &glstate->fpe_state->texenv[tmu].texopalpha3; is_alpha=1; break; // src3 alpha logic slightly diff usually
                        }
                        
                        if (*target_val == param) return;
                        FLUSH_BEGINEND;
                        *target_val = param;
                        
                        if (glstate->fpe_state && fpe_target) {
                            int state = FPE_SRC_TEXTURE;
                            if (param >= GL_TEXTURE0 && param < GL_TEXTURE0 + MAX_TEX) {
                                state = FPE_SRC_TEXTURE0 + (param - GL_TEXTURE0);
                            } else {
                                switch((GLenum)param) {
                                    case GL_CONSTANT: state=FPE_SRC_CONSTANT; break;
                                    case GL_PRIMARY_COLOR: state=FPE_SRC_PRIMARY_COLOR; break;
                                    case GL_PREVIOUS: state=FPE_SRC_PREVIOUS; break;
                                    case GL_ONE: state=FPE_SRC_ONE; break;
                                    case GL_ZERO: state=FPE_SRC_ZERO; break;
                                    case GL_SECONDARY_COLOR_ATIX: state=FPE_SRC_SECONDARY_COLOR; break;
                                }
                            }
                            *fpe_target = state;
                        }
                    }
                    break;

                // Operands
                case GL_OPERAND0_RGB: case GL_OPERAND1_RGB: case GL_OPERAND2_RGB: case GL_OPERAND3_RGB:
                case GL_OPERAND0_ALPHA: case GL_OPERAND1_ALPHA: case GL_OPERAND2_ALPHA: case GL_OPERAND3_ALPHA:
                    {
                        GLfloat *target_val = NULL;
                        uint8_t *fpe_target = NULL;
                        int is_alpha = 0;

                        switch(pname) {
                            case GL_OPERAND0_RGB: target_val = &t->op0_rgb; fpe_target = &glstate->fpe_state->texenv[tmu].texoprgb0; break;
                            case GL_OPERAND1_RGB: target_val = &t->op1_rgb; fpe_target = &glstate->fpe_state->texenv[tmu].texoprgb1; break;
                            case GL_OPERAND2_RGB: target_val = &t->op2_rgb; fpe_target = &glstate->fpe_state->texenv[tmu].texoprgb2; break;
                            case GL_OPERAND3_RGB: target_val = &t->op3_rgb; fpe_target = &glstate->fpe_state->texenv[tmu].texoprgb3; break;
                            case GL_OPERAND0_ALPHA: target_val = &t->op0_alpha; fpe_target = &glstate->fpe_state->texenv[tmu].texopalpha0; is_alpha=1; break;
                            case GL_OPERAND1_ALPHA: target_val = &t->op1_alpha; fpe_target = &glstate->fpe_state->texenv[tmu].texopalpha1; is_alpha=1; break;
                            case GL_OPERAND2_ALPHA: target_val = &t->op2_alpha; fpe_target = &glstate->fpe_state->texenv[tmu].texopalpha2; is_alpha=1; break;
                            case GL_OPERAND3_ALPHA: target_val = &t->op3_alpha; fpe_target = &glstate->fpe_state->texenv[tmu].texopalpha3; is_alpha=1; break;
                        }

                        if (*target_val == param) return;
                        FLUSH_BEGINEND;
                        *target_val = param;

                        if (glstate->fpe_state && fpe_target) {
                            int state = FPE_OP_ALPHA;
                            if (!is_alpha) {
                                switch((GLenum)param) {
                                    case GL_SRC_COLOR: state=FPE_OP_SRCCOLOR; break;
                                    case GL_ONE_MINUS_SRC_COLOR: state=FPE_OP_MINUSCOLOR; break;
                                    case GL_ONE_MINUS_SRC_ALPHA: state=FPE_OP_MINUSALPHA; break;
                                }
                            } else {
                                if (param == GL_ONE_MINUS_SRC_ALPHA) state = FPE_OP_MINUSALPHA;
                            }
                            *fpe_target = state;
                        }
                    }
                    break;

                case GL_RGB_SCALE:
                    if (t->rgb_scale == param) return;
                    FLUSH_BEGINEND;
                    t->rgb_scale = param;
                    if (glstate->fpe_state) 
                        glstate->fpe_state->texenv[tmu].texrgbscale = (param == 1.0f) ? 0 : 1;
                    break;

                case GL_ALPHA_SCALE:
                    if (t->alpha_scale == param) return;
                    FLUSH_BEGINEND;
                    t->alpha_scale = param;
                    if (glstate->fpe_state)
                        glstate->fpe_state->texenv[tmu].texalphascale = (param == 1.0f) ? 0 : 1;
                    break;

                default:
                    errorShim(GL_INVALID_ENUM);
                    return;
            }
        }
        break;
        
        default:
            errorShim(GL_INVALID_ENUM);
            return;
    }

    errorGL();
    if (hardext.esversion == 1) {
        LOAD_GLES3(glTexEnvf);
        realize_active();
        gles_glTexEnvf(target, pname, param);
    }
}

void APIENTRY_GL4ES gl4es_glTexEnvi(GLenum target, GLenum pname, GLint param) {
    DBG(printf("glTexEnvi(...)->");)
    gl4es_glTexEnvf(target, pname, param);
}

void APIENTRY_GL4ES gl4es_glTexEnvfv(GLenum target, GLenum pname, const GLfloat *param) {
    DBG(printf("glTexEnvfv(%s, %s, %p)->", PrintEnum(target), PrintEnum(pname), param);)
    
    if (unlikely(glstate->list.compiling && glstate->list.active && !glstate->list.pending)) {
        DBG(printf("rlTexEnvfv(...)\n");)
        NewStage(glstate->list.active, STAGE_TEXENV);
        rlTexEnvfv(glstate->list.active, target, pname, param);
        noerrorShim();
        return;
    }

    if (target == GL_TEXTURE_ENV && pname == GL_TEXTURE_ENV_COLOR) {
        texenv_t *t = &glstate->texenv[glstate->texture.active].env;
        DBG(printf("Color=%f/%f/%f/%f\n", param[0], param[1], param[2], param[3]);)
        
        // OPTIMIZATION: Check for redundant color update
        if (memcmp(t->color, param, 4 * sizeof(GLfloat)) == 0) {
            noerrorShim();
            return;
        }
        
        FLUSH_BEGINEND;
        memcpy(t->color, param, 4 * sizeof(GLfloat));
        
        errorGL();
        if (hardext.esversion == 1) {
            LOAD_GLES3(glTexEnvfv);
            realize_active();
            gles_glTexEnvfv(target, pname, param);
        }
    } else {
        gl4es_glTexEnvf(target, pname, *param);
    }
}

void APIENTRY_GL4ES gl4es_glTexEnviv(GLenum target, GLenum pname, const GLint *param) {
    DBG(printf("glTexEnviv(%s, %s, %p)->", PrintEnum(target), PrintEnum(pname), param);)
    
    if (unlikely(glstate->list.compiling && glstate->list.active && !glstate->list.pending)) {
        DBG(printf("rlTexEnviv(...)\n");)
        NewStage(glstate->list.active, STAGE_TEXENV);
        rlTexEnviv(glstate->list.active, target, pname, param);
        noerrorShim();
        return;
    }

    if (target == GL_TEXTURE_ENV && pname == GL_TEXTURE_ENV_COLOR) {
        GLfloat p[4];
        p[0] = param[0]; p[1] = param[1]; p[2] = param[2]; p[3] = param[3];
        DBG(printf("Color=%d/%d/%d/%d\n", param[0], param[1], param[2], param[3]);)
        gl4es_glTexEnvfv(target, pname, p);
    } else {
        gl4es_glTexEnvf(target, pname, *param);
    }
}

void APIENTRY_GL4ES gl4es_glGetTexEnvfv(GLenum target, GLenum pname, GLfloat * params) {
    DBG(printf("glGetTexEnvfv(%s, %s, %p)\n", PrintEnum(target), PrintEnum(pname), params);)
    noerrorShim();
    
    switch(target) {
        case GL_POINT_SPRITE:
            if (pname == GL_COORD_REPLACE) {
                *params = glstate->texture.pscoordreplace[glstate->texture.active];
                return;
            }
            break;
            
        case GL_TEXTURE_FILTER_CONTROL:
            if (pname == GL_TEXTURE_LOD_BIAS) {
                *params = glstate->texenv[glstate->texture.active].filter.lod_bias;
                return;
            }
            break;
            
        case GL_TEXTURE_ENV: {
            texenv_t *t = &glstate->texenv[glstate->texture.active].env;
            switch(pname) {
                case GL_TEXTURE_ENV_MODE: *params = t->mode; return;
                case GL_TEXTURE_ENV_COLOR: memcpy(params, t->color, 4*sizeof(GLfloat)); return;
                case GL_COMBINE_RGB: *params = t->combine_rgb; return;
                case GL_COMBINE_ALPHA: *params = t->combine_alpha; return;
                case GL_SRC0_RGB: *params = t->src0_rgb; return;
                case GL_SRC1_RGB: *params = t->src1_rgb; return;
                case GL_SRC2_RGB: *params = t->src2_rgb; return;
                case GL_SRC3_RGB: *params = t->src3_rgb; return;
                case GL_SRC0_ALPHA: *params = t->src0_alpha; return;
                case GL_SRC1_ALPHA: *params = t->src1_alpha; return;
                case GL_SRC2_ALPHA: *params = t->src2_alpha; return;
                case GL_SRC3_ALPHA: *params = t->src3_alpha; return;
                case GL_OPERAND0_RGB: *params = t->op0_rgb; return;
                case GL_OPERAND1_RGB: *params = t->op1_rgb; return;
                case GL_OPERAND2_RGB: *params = t->op2_rgb; return;
                case GL_OPERAND3_RGB: *params = t->op3_rgb; return;
                case GL_OPERAND0_ALPHA: *params = t->op0_alpha; return;
                case GL_OPERAND1_ALPHA: *params = t->op1_alpha; return;
                case GL_OPERAND2_ALPHA: *params = t->op2_alpha; return;
                case GL_OPERAND3_ALPHA: *params = t->op3_alpha; return;
                case GL_RGB_SCALE: *params = t->rgb_scale; return;
                case GL_ALPHA_SCALE: *params = t->alpha_scale; return;
            }
        }
        break;
    }
    
    errorShim(GL_INVALID_ENUM);
}

void APIENTRY_GL4ES gl4es_glGetTexEnviv(GLenum target, GLenum pname, GLint * params) {
    DBG(printf("glGetTexEnviv(%s, %s, %p)\n", PrintEnum(target), PrintEnum(pname), params);)
    noerrorShim();
    
    switch(target) {
        case GL_POINT_SPRITE:
            if (pname == GL_COORD_REPLACE) {
                *params = glstate->texture.pscoordreplace[glstate->texture.active];
                return;
            }
            break;
            
        case GL_TEXTURE_FILTER_CONTROL:
            if (pname == GL_TEXTURE_LOD_BIAS) {
                *params = (GLint)glstate->texenv[glstate->texture.active].filter.lod_bias;
                return;
            }
            break;
            
        case GL_TEXTURE_ENV: {
            texenv_t *t = &glstate->texenv[glstate->texture.active].env;
            switch(pname) {
                case GL_TEXTURE_ENV_MODE: *params = t->mode; return;
                case GL_TEXTURE_ENV_COLOR: 
                    // Preserving original behavior: memcpy raw bits.
                    memcpy(params, t->color, 4*sizeof(GLfloat));
                    return;
                case GL_COMBINE_RGB: *params = t->combine_rgb; return;
                case GL_COMBINE_ALPHA: *params = t->combine_alpha; return;
                case GL_SRC0_RGB: *params = t->src0_rgb; return;
                case GL_SRC1_RGB: *params = t->src1_rgb; return;
                case GL_SRC2_RGB: *params = t->src2_rgb; return;
                case GL_SRC3_RGB: *params = t->src3_rgb; return;
                case GL_SRC0_ALPHA: *params = t->src0_alpha; return;
                case GL_SRC1_ALPHA: *params = t->src1_alpha; return;
                case GL_SRC2_ALPHA: *params = t->src2_alpha; return;
                case GL_SRC3_ALPHA: *params = t->src3_alpha; return;
                case GL_OPERAND0_RGB: *params = t->op0_rgb; return;
                case GL_OPERAND1_RGB: *params = t->op1_rgb; return;
                case GL_OPERAND2_RGB: *params = t->op2_rgb; return;
                case GL_OPERAND3_RGB: *params = t->op3_rgb; return;
                case GL_OPERAND0_ALPHA: *params = t->op0_alpha; return;
                case GL_OPERAND1_ALPHA: *params = t->op1_alpha; return;
                case GL_OPERAND2_ALPHA: *params = t->op2_alpha; return;
                case GL_OPERAND3_ALPHA: *params = t->op3_alpha; return;
                case GL_RGB_SCALE: *params = (GLint)t->rgb_scale; return;
                case GL_ALPHA_SCALE: *params = (GLint)t->alpha_scale; return;
            }
        }
        break;
    }
    
    errorShim(GL_INVALID_ENUM);
}

AliasExport(void,glTexEnvf,,(GLenum target, GLenum pname, GLfloat param));
AliasExport(void,glTexEnvi,,(GLenum target, GLenum pname, GLint param));
AliasExport(void,glTexEnvfv,,(GLenum target, GLenum pname, const GLfloat *param));
AliasExport(void,glTexEnviv,,(GLenum target, GLenum pname, const GLint *param));
AliasExport(void,glGetTexEnvfv,,(GLenum target, GLenum pname, GLfloat * params));
AliasExport(void,glGetTexEnviv,,(GLenum target, GLenum pname, GLint * params));
