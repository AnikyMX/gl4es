/*
 * Refactored stack.c for GL4ES
 * Optimized for ARMv8
 * - Fixed struct member access errors (dither, clear_color, viewport, etc.)
 * - Reverted to safe Getter functions where struct layout is opaque
 * - Kept efficient memory management
 */

#include <stdio.h>
#include "stack.h"
#include "../glx/hardext.h"
#include "wrap/gl4es.h"
#include "matrix.h"
#include "debug.h"
#include "light.h"

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

void APIENTRY_GL4ES gl4es_glPushAttrib(GLbitfield mask) {
    DBG(printf("glPushAttrib(0x%04X)\n", mask);)
    
    realize_textures(0);
    noerrorShim();

    if (unlikely(glstate->list.active)) {
        if (glstate->list.compiling) {
            NewStage(glstate->list.active, STAGE_PUSH);
            glstate->list.active->pushattribute = mask;
            return;
        } else {
            gl4es_flush();
        }
    }

    if (unlikely(glstate->stack == NULL)) {
        glstate->stack = (glstack_t *)malloc(STACK_SIZE * sizeof(glstack_t));
        glstate->stack->len = 0;
        glstate->stack->cap = STACK_SIZE;
    } else if (unlikely(glstate->stack->len == glstate->stack->cap)) {
        glstate->stack->cap += STACK_SIZE;
        glstate->stack = (glstack_t *)realloc(glstate->stack, glstate->stack->cap * sizeof(glstack_t));
    }

    glstack_t *cur = glstate->stack + glstate->stack->len;
    cur->mask = mask;
    
    cur->clip_planes_enabled = NULL;
    cur->clip_planes = NULL;
    cur->lights_enabled = NULL;
    cur->lights = NULL;
    cur->materials = NULL;

    if (mask & GL_COLOR_BUFFER_BIT) {
        cur->alpha_test = gl4es_glIsEnabled(GL_ALPHA_TEST);
        gl4es_glGetIntegerv(GL_ALPHA_TEST_FUNC, &cur->alpha_test_func);
        gl4es_glGetFloatv(GL_ALPHA_TEST_REF, &cur->alpha_test_ref);

        cur->blend = gl4es_glIsEnabled(GL_BLEND);
        gl4es_glGetIntegerv(GL_BLEND_SRC, &cur->blend_src_func);
        gl4es_glGetIntegerv(GL_BLEND_DST, &cur->blend_dst_func);

        // FIXED: Use function instead of direct access
        cur->dither = gl4es_glIsEnabled(GL_DITHER);
        cur->color_logic_op = gl4es_glIsEnabled(GL_COLOR_LOGIC_OP);
        gl4es_glGetIntegerv(GL_LOGIC_OP_MODE, &cur->logic_op);

        // FIXED: clear_color is not in glstate struct directly
        gl4es_glGetFloatv(GL_COLOR_CLEAR_VALUE, cur->clear_color);
        
        // Colormask is in glstate (GLboolean colormask[4])
        memcpy(cur->color_mask, glstate->colormask, 4 * sizeof(GLboolean));
    }

    if (mask & GL_CURRENT_BIT) {
        memcpy(cur->color, glstate->color, 4 * sizeof(GLfloat));
        memcpy(cur->normal, glstate->normal, 3 * sizeof(GLfloat));
        memcpy(cur->tex, glstate->texcoord[glstate->texture.active], 4 * sizeof(GLfloat));
    }

    if (mask & GL_DEPTH_BUFFER_BIT) {
        cur->depth_test = gl4es_glIsEnabled(GL_DEPTH_TEST);
        cur->depth_func = glstate->depth.func;
        cur->clear_depth = glstate->depth.clear;
        cur->depth_mask = glstate->depth.mask;
    }

    if (mask & GL_ENABLE_BIT) {
        int i;
        cur->alpha_test = gl4es_glIsEnabled(GL_ALPHA_TEST);
        cur->autonormal = gl4es_glIsEnabled(GL_AUTO_NORMAL);
        cur->blend = gl4es_glIsEnabled(GL_BLEND);
        
        cur->clip_planes_enabled = (GLboolean *)malloc(hardext.maxplanes * sizeof(GLboolean));
        for (i = 0; i < hardext.maxplanes; i++) {
            cur->clip_planes_enabled[i] = gl4es_glIsEnabled(GL_CLIP_PLANE0 + i);
        }

        cur->colormaterial = gl4es_glIsEnabled(GL_COLOR_MATERIAL);
        cur->cull_face = gl4es_glIsEnabled(GL_CULL_FACE);
        cur->depth_test = gl4es_glIsEnabled(GL_DEPTH_TEST);
        cur->dither = gl4es_glIsEnabled(GL_DITHER);
        cur->fog = gl4es_glIsEnabled(GL_FOG);

        cur->lights_enabled = (GLboolean *)malloc(hardext.maxlights * sizeof(GLboolean));
        for (i = 0; i < hardext.maxlights; i++) {
            cur->lights_enabled[i] = gl4es_glIsEnabled(GL_LIGHT0 + i);
        }

        cur->lighting = gl4es_glIsEnabled(GL_LIGHTING);
        cur->line_smooth = gl4es_glIsEnabled(GL_LINE_SMOOTH);
        cur->line_stipple = gl4es_glIsEnabled(GL_LINE_STIPPLE);
        cur->color_logic_op = gl4es_glIsEnabled(GL_COLOR_LOGIC_OP);
        
        cur->multisample = gl4es_glIsEnabled(GL_MULTISAMPLE);
        cur->normalize = gl4es_glIsEnabled(GL_NORMALIZE);
        cur->point_smooth = gl4es_glIsEnabled(GL_POINT_SMOOTH);
        
        // FIXED: poly_offset_fill access
        cur->polygon_offset_fill = gl4es_glIsEnabled(GL_POLYGON_OFFSET_FILL);
        
        cur->sample_alpha_to_coverage = gl4es_glIsEnabled(GL_SAMPLE_ALPHA_TO_COVERAGE);
        cur->sample_alpha_to_one = gl4es_glIsEnabled(GL_SAMPLE_ALPHA_TO_ONE);
        cur->sample_coverage = gl4es_glIsEnabled(GL_SAMPLE_COVERAGE);
        
        // FIXED: scissor_test access
        cur->scissor_test = gl4es_glIsEnabled(GL_SCISSOR_TEST);
        cur->stencil_test = gl4es_glIsEnabled(GL_STENCIL_TEST);
        
        // Texture enables (using explicit check to be safe)
        for (int a = 0; a < hardext.maxtex; a++) {
            cur->tex_enabled[a] = glstate->enable.texture[a];
            cur->texgen_s[a] = glstate->enable.texgen_s[a];
            cur->texgen_r[a] = glstate->enable.texgen_r[a];
            cur->texgen_t[a] = glstate->enable.texgen_t[a];
            cur->texgen_q[a] = glstate->enable.texgen_q[a];
        }
        cur->pointsprite = gl4es_glIsEnabled(GL_POINT_SPRITE);
    }

    if (mask & GL_FOG_BIT) {
        cur->fog = gl4es_glIsEnabled(GL_FOG);
        memcpy(cur->fog_color, glstate->fog.color, 4 * sizeof(GLfloat));
        cur->fog_density = glstate->fog.density;
        cur->fog_start = glstate->fog.start;
        cur->fog_end = glstate->fog.end;
        cur->fog_mode = glstate->fog.mode;
    }

    if (mask & GL_HINT_BIT) {
        gl4es_glGetIntegerv(GL_PERSPECTIVE_CORRECTION_HINT, &cur->perspective_hint);
        gl4es_glGetIntegerv(GL_POINT_SMOOTH_HINT, &cur->point_smooth_hint);
        gl4es_glGetIntegerv(GL_LINE_SMOOTH_HINT, &cur->line_smooth_hint);
        gl4es_glGetIntegerv(GL_FOG_HINT, &cur->fog_hint);
        gl4es_glGetIntegerv(GL_GENERATE_MIPMAP_HINT, &cur->mipmap_hint);
        for (int i=GL4ES_HINT_FIRST; i<GL4ES_HINT_LAST; i++)
            gl4es_glGetIntegerv(i, &cur->gles4_hint[i-GL4ES_HINT_FIRST]);
    }

    if (mask & GL_LIGHTING_BIT) {
        cur->lighting = gl4es_glIsEnabled(GL_LIGHTING);
        memcpy(cur->light_model_ambient, glstate->light.ambient, 4 * sizeof(GLfloat));
        cur->light_model_two_side = glstate->light.two_side;

        cur->lights_enabled = (GLboolean *)malloc(hardext.maxlights * sizeof(GLboolean));
        cur->lights = (GLfloat *)malloc(hardext.maxlights * sizeof(GLfloat) * 40);
        
        float *ptr = cur->lights;
        for (int i = 0; i < hardext.maxlights; i++) {
            cur->lights_enabled[i] = gl4es_glIsEnabled(GL_LIGHT0 + i);
            
            // Direct copy from glstate->light.lights[i] structure
            memcpy(ptr, glstate->light.lights[i].ambient, 4 * sizeof(GLfloat)); ptr += 4;
            memcpy(ptr, glstate->light.lights[i].diffuse, 4 * sizeof(GLfloat)); ptr += 4;
            memcpy(ptr, glstate->light.lights[i].specular, 4 * sizeof(GLfloat)); ptr += 4;
            memcpy(ptr, glstate->light.lights[i].position, 4 * sizeof(GLfloat)); ptr += 4;
            *ptr++ = glstate->light.lights[i].spotCutoff;
            memcpy(ptr, glstate->light.lights[i].spotDirection, 3 * sizeof(GLfloat)); ptr += 3;
            *ptr++ = glstate->light.lights[i].spotExponent;
            *ptr++ = glstate->light.lights[i].constantAttenuation;
            *ptr++ = glstate->light.lights[i].linearAttenuation;
            *ptr++ = glstate->light.lights[i].quadraticAttenuation;
        }

        // Materials
        cur->materials = (GLfloat *)malloc(2 * sizeof(GLfloat) * 20);
        ptr = cur->materials;
        
        // Back
        memcpy(ptr, glstate->material.back.ambient, 4*sizeof(GLfloat)); ptr+=4;
        memcpy(ptr, glstate->material.back.diffuse, 4*sizeof(GLfloat)); ptr+=4;
        memcpy(ptr, glstate->material.back.specular, 4*sizeof(GLfloat)); ptr+=4;
        memcpy(ptr, glstate->material.back.emission, 4*sizeof(GLfloat)); ptr+=4;
        *ptr++ = glstate->material.back.shininess;
        
        // Front
        memcpy(ptr, glstate->material.front.ambient, 4*sizeof(GLfloat)); ptr+=4;
        memcpy(ptr, glstate->material.front.diffuse, 4*sizeof(GLfloat)); ptr+=4;
        memcpy(ptr, glstate->material.front.specular, 4*sizeof(GLfloat)); ptr+=4;
        memcpy(ptr, glstate->material.front.emission, 4*sizeof(GLfloat)); ptr+=4;
        *ptr++ = glstate->material.front.shininess;

        cur->shade_model = glstate->shademodel;
    }
    if (mask & GL_LINE_BIT) {
        cur->line_smooth = gl4es_glIsEnabled(GL_LINE_SMOOTH);
        gl4es_glGetFloatv(GL_LINE_WIDTH, &cur->line_width);
        // Stipple state is usually not supported fully in GLES/GL4ES, skipping for speed
    }

    if (mask & GL_LIST_BIT) {
        cur->list_base = glstate->list.base;
    }

    if (mask & GL_MULTISAMPLE_BIT) {
        cur->multisample = gl4es_glIsEnabled(GL_MULTISAMPLE);
        cur->sample_alpha_to_coverage = gl4es_glIsEnabled(GL_SAMPLE_ALPHA_TO_COVERAGE);
        cur->sample_alpha_to_one = gl4es_glIsEnabled(GL_SAMPLE_ALPHA_TO_ONE);
        cur->sample_coverage = gl4es_glIsEnabled(GL_SAMPLE_COVERAGE);
    }

    if (mask & GL_PIXEL_MODE_BIT) {
        GLenum pixel_name[] = {GL_RED_BIAS, GL_RED_SCALE, GL_GREEN_BIAS, GL_GREEN_SCALE, GL_BLUE_BIAS, GL_BLUE_SCALE, GL_ALPHA_BIAS, GL_ALPHA_SCALE};
        for (int i=0; i<8; i++) 
            gl4es_glGetFloatv(pixel_name[i], &cur->pixel_scale_bias[i]);
        
        gl4es_glGetFloatv(GL_ZOOM_X, &cur->pixel_zoomx);
        gl4es_glGetFloatv(GL_ZOOM_Y, &cur->pixel_zoomy);
    }
    
    if (mask & GL_POINT_BIT) {
        cur->point_smooth = gl4es_glIsEnabled(GL_POINT_SMOOTH);
        cur->point_size = glstate->pointsprite.size;
        if(hardext.pointsprite) {
            cur->pointsprite = gl4es_glIsEnabled(GL_POINT_SPRITE);
            for (int a=0; a<hardext.maxtex; a++) {
                cur->pscoordreplace[a] = glstate->texture.pscoordreplace[a];
            }
        }
    }

    if (mask & GL_SCISSOR_BIT) {
        // FIXED: Use Getter
        cur->scissor_test = gl4es_glIsEnabled(GL_SCISSOR_TEST);
        gl4es_glGetFloatv(GL_SCISSOR_BOX, cur->scissor_box);
    }

    if (mask & GL_STENCIL_BUFFER_BIT) {
        cur->stencil_test = gl4es_glIsEnabled(GL_STENCIL_TEST);
        
        // Direct access valid here based on glstate.h
        cur->stencil_func = glstate->stencil.func[0];
        cur->stencil_mask = glstate->stencil.mask[0];
        cur->stencil_ref = glstate->stencil.f_ref[0];
        
        cur->stencil_sfail = glstate->stencil.sfail[0];
        cur->stencil_dpfail = glstate->stencil.dpfail[0];
        cur->stencil_dppass = glstate->stencil.dppass[0];
        
        cur->stencil_clearvalue = glstate->stencil.clear;
    }

    if (mask & GL_TEXTURE_BIT) {
        cur->active = glstate->texture.active;
        for (int a=0; a<hardext.maxtex; a++) {
            cur->texgen_r[a] = glstate->enable.texgen_r[a];
            cur->texgen_s[a] = glstate->enable.texgen_s[a];
            cur->texgen_t[a] = glstate->enable.texgen_t[a];
            cur->texgen_q[a] = glstate->enable.texgen_q[a];
            cur->texgen[a] = glstate->texgen[a];
            for (int j=0; j<ENABLED_TEXTURE_LAST; j++)
                cur->texture[a][j] = glstate->texture.bound[a][j]->texture;
        }
    }

    if (mask & GL_TRANSFORM_BIT) {
        if (!(mask & GL_ENABLE_BIT)) {
            cur->clip_planes_enabled = (GLboolean *)malloc(hardext.maxplanes * sizeof(GLboolean));
            for (int i = 0; i < hardext.maxplanes; i++) {
                cur->clip_planes_enabled[i] = gl4es_glIsEnabled(GL_CLIP_PLANE0 + i);
            }
        }
        cur->matrix_mode = glstate->matrix_mode;
        // FIXED: Use Getter
        cur->rescale_normal_flag = gl4es_glIsEnabled(GL_RESCALE_NORMAL);
        cur->normalize_flag = gl4es_glIsEnabled(GL_NORMALIZE);
    }

    if (mask & GL_VIEWPORT_BIT) {
        // FIXED: Use Getter (glstate.h has 'vp' but error says 'viewport' in raster struct invalid)
        gl4es_glGetIntegerv(GL_VIEWPORT, cur->viewport_size);
        cur->depth_range[0] = glstate->depth.Near;
        cur->depth_range[1] = glstate->depth.Far;
    }
        
    glstate->stack->len++;
}

void APIENTRY_GL4ES gl4es_glPushClientAttrib(GLbitfield mask) {
    DBG(printf("glPushClientAttrib(0x%04X)\n", mask);)
    noerrorShim();
    
    if (unlikely(glstate->clientStack == NULL)) {
        glstate->clientStack = (glclientstack_t *)malloc(STACK_SIZE * sizeof(glclientstack_t));
        glstate->clientStack->len = 0;
        glstate->clientStack->cap = STACK_SIZE;
    } else if (unlikely(glstate->clientStack->len == glstate->clientStack->cap)) {
        glstate->clientStack->cap += STACK_SIZE;
        glstate->clientStack = (glclientstack_t *)realloc(glstate->clientStack, glstate->clientStack->cap * sizeof(glclientstack_t));
    }

    glclientstack_t *cur = glstate->clientStack + glstate->clientStack->len;
    cur->mask = mask;

    if (mask & GL_CLIENT_PIXEL_STORE_BIT) {
        cur->pack_align = glstate->texture.pack_align;
        cur->unpack_align = glstate->texture.unpack_align;
        cur->unpack_row_length = glstate->texture.unpack_row_length;
        cur->unpack_skip_pixels = glstate->texture.unpack_skip_pixels;
        cur->unpack_skip_rows = glstate->texture.unpack_skip_rows;
        cur->pack_row_length = glstate->texture.pack_row_length;
        cur->pack_skip_pixels = glstate->texture.pack_skip_pixels;
        cur->pack_skip_rows = glstate->texture.pack_skip_rows;
    }

    if (mask & GL_CLIENT_VERTEX_ARRAY_BIT) {
        memcpy(cur->vertexattrib, glstate->vao->vertexattrib, sizeof(glstate->vao->vertexattrib));
        cur->client = glstate->texture.client;
    }

    glstate->clientStack->len++;
}

#define maybe_free(x) if (x) free(x)

#define enable_disable(pname, enabled) \
    if (enabled) gl4es_glEnable(pname); \
    else gl4es_glDisable(pname)

#define v2(c) c[0], c[1]
#define v3(c) v2(c), c[2]
#define v4(c) v3(c), c[3]

void APIENTRY_GL4ES gl4es_glPopAttrib(void) {
    DBG(printf("glPopAttrib()\n");)
    noerrorShim();
    
    if (glstate->list.active) {
        if (glstate->list.compiling) {
            NewStage(glstate->list.active, STAGE_POP);
            glstate->list.active->popattribute = true;
            return;
        } else {
            gl4es_flush();
        }
    }

    if (unlikely(glstate->stack == NULL || glstate->stack->len == 0)) {
        errorShim(GL_STACK_UNDERFLOW);
        return;
    }

    glstack_t *cur = glstate->stack + glstate->stack->len - 1;

    // Restore States
    
    if (cur->mask & GL_COLOR_BUFFER_BIT) {
        enable_disable(GL_ALPHA_TEST, cur->alpha_test);
        gl4es_glAlphaFunc(cur->alpha_test_func, cur->alpha_test_ref);

        enable_disable(GL_BLEND, cur->blend);
        gl4es_glBlendFunc(cur->blend_src_func, cur->blend_dst_func);

        enable_disable(GL_DITHER, cur->dither);
        enable_disable(GL_COLOR_LOGIC_OP, cur->color_logic_op);
        gl4es_glLogicOp(cur->logic_op);

        gl4es_glClearColor(v4(cur->clear_color));
        gl4es_glColorMask(v4(cur->color_mask));
    }

    if (cur->mask & GL_CURRENT_BIT) {
        gl4es_glColor4f(v4(cur->color));
        gl4es_glNormal3f(v3(cur->normal));
        gl4es_glTexCoord4f(v4(cur->tex));
    }

    if (cur->mask & GL_DEPTH_BUFFER_BIT) {
        enable_disable(GL_DEPTH_TEST, cur->depth_test);
        gl4es_glDepthFunc(cur->depth_func);
        gl4es_glClearDepth(cur->clear_depth);
        gl4es_glDepthMask(cur->depth_mask);
    }

    if (cur->mask & GL_ENABLE_BIT) {
        int i;
        enable_disable(GL_ALPHA_TEST, cur->alpha_test);
        enable_disable(GL_AUTO_NORMAL, cur->autonormal);
        enable_disable(GL_BLEND, cur->blend);

        for (i = 0; i < hardext.maxplanes; i++) {
            enable_disable(GL_CLIP_PLANE0 + i, *(cur->clip_planes_enabled + i));
        }

        enable_disable(GL_COLOR_MATERIAL, cur->colormaterial);
        enable_disable(GL_CULL_FACE, cur->cull_face);
        enable_disable(GL_DEPTH_TEST, cur->depth_test);
        enable_disable(GL_DITHER, cur->dither);
        enable_disable(GL_FOG, cur->fog);

        for (i = 0; i < hardext.maxlights; i++) {
            enable_disable(GL_LIGHT0 + i, *(cur->lights_enabled + i));
        }

        enable_disable(GL_LIGHTING, cur->lighting);
        enable_disable(GL_LINE_SMOOTH, cur->line_smooth);
        enable_disable(GL_LINE_STIPPLE, cur->line_stipple);
        enable_disable(GL_COLOR_LOGIC_OP, cur->color_logic_op);
        
        enable_disable(GL_MULTISAMPLE, cur->multisample);
        enable_disable(GL_NORMALIZE, cur->normalize);
        enable_disable(GL_POINT_SMOOTH, cur->point_smooth);
        enable_disable(GL_POLYGON_OFFSET_FILL, cur->polygon_offset_fill);
        
        enable_disable(GL_SAMPLE_ALPHA_TO_COVERAGE, cur->sample_alpha_to_coverage);
        enable_disable(GL_SAMPLE_ALPHA_TO_ONE, cur->sample_alpha_to_one);
        enable_disable(GL_SAMPLE_COVERAGE, cur->sample_coverage);
        enable_disable(GL_SCISSOR_TEST, cur->scissor_test);
        enable_disable(GL_STENCIL_TEST, cur->stencil_test);
        enable_disable(GL_POINT_SPRITE, cur->pointsprite);
        
        int a;
        int old_tex = glstate->texture.active;
        for (a = 0; a < hardext.maxtex; a++) {
            if (glstate->enable.texture[a] != cur->tex_enabled[a]) {
                for (int j = 0; j < ENABLED_TEXTURE_LAST; j++) {
                    const GLuint t = cur->tex_enabled[a] & (1 << j);
                    if ((glstate->enable.texture[a] & (1 << j)) != t) {
                        if (glstate->texture.active != a) gl4es_glActiveTexture(GL_TEXTURE0 + a);
                        enable_disable(to_target(j), t); 
                    }
                }
            }
            glstate->enable.texgen_r[a] = cur->texgen_r[a];
            glstate->enable.texgen_s[a] = cur->texgen_s[a];
            glstate->enable.texgen_t[a] = cur->texgen_t[a];
            glstate->enable.texgen_q[a] = cur->texgen_q[a];
        }
        if (glstate->texture.active != old_tex) gl4es_glActiveTexture(GL_TEXTURE0 + old_tex);
    }

    if (cur->mask & GL_FOG_BIT) {
        enable_disable(GL_FOG, cur->fog);
        gl4es_glFogfv(GL_FOG_COLOR, cur->fog_color);
        gl4es_glFogf(GL_FOG_DENSITY, cur->fog_density);
        gl4es_glFogf(GL_FOG_START, cur->fog_start);
        gl4es_glFogf(GL_FOG_END, cur->fog_end);
        gl4es_glFogf(GL_FOG_MODE, cur->fog_mode);
    }

    if (cur->mask & GL_HINT_BIT) {
        gl4es_glHint(GL_PERSPECTIVE_CORRECTION_HINT, cur->perspective_hint);
        gl4es_glHint(GL_POINT_SMOOTH_HINT, cur->point_smooth_hint);
        gl4es_glHint(GL_LINE_SMOOTH_HINT, cur->line_smooth_hint);
        gl4es_glHint(GL_FOG_HINT, cur->fog_hint);
        gl4es_glHint(GL_GENERATE_MIPMAP_HINT, cur->mipmap_hint);
        for (int i = GL4ES_HINT_FIRST; i < GL4ES_HINT_LAST; i++)
            gl4es_glHint(i, cur->gles4_hint[i - GL4ES_HINT_FIRST]);
    }

    if (cur->mask & GL_LIGHTING_BIT) {
        enable_disable(GL_LIGHTING, cur->lighting);
        gl4es_glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cur->light_model_ambient);
        gl4es_glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, cur->light_model_two_side);

        int i;
        int j = 0;
        int old_matrixmode = glstate->matrix_mode;
        int identity = is_identity(getMVMat());
        
        if (!identity) {
            if (old_matrixmode != GL_MODELVIEW) gl4es_glMatrixMode(GL_MODELVIEW);
            gl4es_glPushMatrix();
            gl4es_glLoadIdentity();
        }

        for (i = 0; i < hardext.maxlights; i++) {
            enable_disable(GL_LIGHT0 + i, *(cur->lights_enabled + i));
            
            // Revert redundancy check to standard calls for safety/simplicity
            gl4es_glLightfv(GL_LIGHT0 + i, GL_AMBIENT, cur->lights + j); j += 4;
            gl4es_glLightfv(GL_LIGHT0 + i, GL_DIFFUSE, cur->lights + j); j += 4;
            gl4es_glLightfv(GL_LIGHT0 + i, GL_SPECULAR, cur->lights + j); j += 4;
            gl4es_glLightfv(GL_LIGHT0 + i, GL_POSITION, cur->lights + j); j += 4;
            
            gl4es_glLightf(GL_LIGHT0 + i, GL_SPOT_CUTOFF, cur->lights[j++]);
            gl4es_glLightfv(GL_LIGHT0 + i, GL_SPOT_DIRECTION, cur->lights + j); j += 3;
            
            gl4es_glLightf(GL_LIGHT0 + i, GL_SPOT_EXPONENT, cur->lights[j++]);
            gl4es_glLightf(GL_LIGHT0 + i, GL_CONSTANT_ATTENUATION, cur->lights[j++]);
            gl4es_glLightf(GL_LIGHT0 + i, GL_LINEAR_ATTENUATION, cur->lights[j++]);
            gl4es_glLightf(GL_LIGHT0 + i, GL_QUADRATIC_ATTENUATION, cur->lights[j++]);
        }

        if (!identity) {
            gl4es_glPopMatrix();
            if (old_matrixmode != GL_MODELVIEW) gl4es_glMatrixMode(old_matrixmode);
        }
        
        j = 0;
        // Material
        if (memcmp(cur->materials + j, cur->materials + j + 4, 4 * sizeof(GLfloat)) == 0) {
            gl4es_glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, cur->materials + j); j += 8;
        } else {
            gl4es_glMaterialfv(GL_BACK, GL_AMBIENT, cur->materials + j); j += 4;
            gl4es_glMaterialfv(GL_FRONT, GL_AMBIENT, cur->materials + j); j += 4;
        }
        
        if (memcmp(cur->materials + j, cur->materials + j + 4, 4 * sizeof(GLfloat)) == 0) {
            gl4es_glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, cur->materials + j); j += 8;
        } else {
            gl4es_glMaterialfv(GL_BACK, GL_DIFFUSE, cur->materials + j); j += 4;
            gl4es_glMaterialfv(GL_FRONT, GL_DIFFUSE, cur->materials + j); j += 4;
        }
        
        if (memcmp(cur->materials + j, cur->materials + j + 4, 4 * sizeof(GLfloat)) == 0) {
            gl4es_glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, cur->materials + j); j += 8;
        } else {
            gl4es_glMaterialfv(GL_BACK, GL_SPECULAR, cur->materials + j); j += 4;
            gl4es_glMaterialfv(GL_FRONT, GL_SPECULAR, cur->materials + j); j += 4;
        }
        
        if (memcmp(cur->materials + j, cur->materials + j + 4, 4 * sizeof(GLfloat)) == 0) {
            gl4es_glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, cur->materials + j); j += 8;
        } else {
            gl4es_glMaterialfv(GL_BACK, GL_EMISSION, cur->materials + j); j += 4;
            gl4es_glMaterialfv(GL_FRONT, GL_EMISSION, cur->materials + j); j += 4;
        }
        
        if (cur->materials[j] == cur->materials[j+1]) {
            gl4es_glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, cur->materials[j]); j += 2;
        } else {
            gl4es_glMaterialf(GL_BACK, GL_SHININESS, cur->materials[j++]); 
            gl4es_glMaterialf(GL_FRONT, GL_SHININESS, cur->materials[j++]);
        }

        gl4es_glShadeModel(cur->shade_model);
    }

    if (cur->mask & GL_LIST_BIT) {
        gl4es_glListBase(cur->list_base);
    }

    if (cur->mask & GL_LINE_BIT) {
        enable_disable(GL_LINE_SMOOTH, cur->line_smooth);
        gl4es_glLineWidth(cur->line_width);
    }

    if (cur->mask & GL_MULTISAMPLE_BIT) {
        enable_disable(GL_MULTISAMPLE, cur->multisample);
        enable_disable(GL_SAMPLE_ALPHA_TO_COVERAGE, cur->sample_alpha_to_coverage);
        enable_disable(GL_SAMPLE_ALPHA_TO_ONE, cur->sample_alpha_to_one);
        enable_disable(GL_SAMPLE_COVERAGE, cur->sample_coverage);
    }

    if (cur->mask & GL_POINT_BIT) {
        enable_disable(GL_POINT_SMOOTH, cur->point_smooth);
        gl4es_glPointSize(cur->point_size);
        if (hardext.pointsprite) {
            enable_disable(GL_POINT_SPRITE, cur->pointsprite);
            int old_tex = glstate->texture.active;
            for (int a = 0; a < hardext.maxtex; a++) {
                if (glstate->texture.pscoordreplace[a] != cur->pscoordreplace[a]) {
                    if (glstate->texture.active != a) gl4es_glActiveTexture(GL_TEXTURE0 + a);
                    gl4es_glTexEnvi(GL_POINT_SPRITE, GL_COORD_REPLACE, cur->pscoordreplace[a]);
                }
            }
            if (glstate->texture.active != old_tex) gl4es_glActiveTexture(GL_TEXTURE0 + old_tex);
        }
    }

    if (cur->mask & GL_SCISSOR_BIT) {
        enable_disable(GL_SCISSOR_TEST, cur->scissor_test);
        gl4es_glScissor(v4(cur->scissor_box));
    }

    if (cur->mask & GL_STENCIL_BUFFER_BIT) {
        enable_disable(GL_STENCIL_TEST, cur->stencil_test);
        gl4es_glStencilFunc(cur->stencil_func, cur->stencil_ref, cur->stencil_mask);
        gl4es_glStencilOp(cur->stencil_sfail, cur->stencil_dpfail, cur->stencil_dppass);
        gl4es_glClearStencil(cur->stencil_clearvalue);
    }

    if (cur->mask & GL_TEXTURE_BIT) {
        int old_tex = glstate->texture.active;
        for (int a = 0; a < hardext.maxtex; a++) {
            glstate->enable.texgen_r[a] = cur->texgen_r[a];
            glstate->enable.texgen_s[a] = cur->texgen_s[a];
            glstate->enable.texgen_t[a] = cur->texgen_t[a];
            glstate->enable.texgen_q[a] = cur->texgen_q[a];
            glstate->texgen[a] = cur->texgen[a];
            for (int j = 0; j < ENABLED_TEXTURE_LAST; j++)
                if (cur->texture[a][j] != glstate->texture.bound[a][j]->texture) {
                    if (glstate->texture.active != a) gl4es_glActiveTexture(GL_TEXTURE0 + a);
                    gl4es_glBindTexture(to_target(j), cur->texture[a][j]);
                }
        }
        if (glstate->texture.active != old_tex) gl4es_glActiveTexture(GL_TEXTURE0 + old_tex);
    }
    
    if (cur->mask & GL_PIXEL_MODE_BIT) {
        GLenum pixel_name[] = {GL_RED_BIAS, GL_RED_SCALE, GL_GREEN_BIAS, GL_GREEN_SCALE, GL_BLUE_BIAS, GL_BLUE_SCALE, GL_ALPHA_BIAS, GL_ALPHA_SCALE};
        for (int i = 0; i < 8; i++) 
            gl4es_glPixelTransferf(pixel_name[i], cur->pixel_scale_bias[i]);
        gl4es_glPixelZoom(cur->pixel_zoomx, cur->pixel_zoomy);
    }

    if (cur->mask & GL_TRANSFORM_BIT) {
        if (!(cur->mask & GL_ENABLE_BIT)) {
            for (int i = 0; i < hardext.maxplanes; i++) {
                enable_disable(GL_CLIP_PLANE0 + i, *(cur->clip_planes_enabled + i));
            }
        }
        gl4es_glMatrixMode(cur->matrix_mode);
        enable_disable(GL_NORMALIZE, cur->normalize_flag);        
        enable_disable(GL_RESCALE_NORMAL, cur->rescale_normal_flag);        
    }

    if (cur->mask & GL_VIEWPORT_BIT) {
        gl4es_glViewport(cur->viewport_size[0], cur->viewport_size[1], cur->viewport_size[2], cur->viewport_size[3]);
        gl4es_glDepthRangef(cur->depth_range[0], cur->depth_range[1]);
    }
    
    maybe_free(cur->clip_planes_enabled);
    maybe_free(cur->clip_planes);
    maybe_free(cur->lights_enabled);
    maybe_free(cur->lights);
    maybe_free(cur->materials);
    glstate->stack->len--;
}

#undef enable_disable
#define enable_disable(pname, enabled) \
    if (enabled) gl4es_glEnableClientState(pname); \
    else gl4es_glDisableClientState(pname)

void APIENTRY_GL4ES gl4es_glPopClientAttrib(void) {
    DBG(printf("glPopClientAttrib()\n");)
    noerrorShim();

    if (unlikely(glstate->clientStack == NULL || glstate->clientStack->len == 0)) {
        errorShim(GL_STACK_UNDERFLOW);
        return;
    }

    glclientstack_t *cur = glstate->clientStack + glstate->clientStack->len - 1;
    
    if (cur->mask & GL_CLIENT_PIXEL_STORE_BIT) {
        gl4es_glPixelStorei(GL_PACK_ALIGNMENT, cur->pack_align);
        gl4es_glPixelStorei(GL_UNPACK_ALIGNMENT, cur->unpack_align);
        gl4es_glPixelStorei(GL_UNPACK_ROW_LENGTH, cur->unpack_row_length);
        gl4es_glPixelStorei(GL_UNPACK_SKIP_PIXELS, cur->unpack_skip_pixels);
        gl4es_glPixelStorei(GL_UNPACK_SKIP_ROWS, cur->unpack_skip_rows);
        gl4es_glPixelStorei(GL_PACK_ROW_LENGTH, cur->pack_row_length);
        gl4es_glPixelStorei(GL_PACK_SKIP_PIXELS, cur->pack_skip_pixels);
        gl4es_glPixelStorei(GL_PACK_SKIP_ROWS, cur->pack_skip_rows);
    }

    if (cur->mask & GL_CLIENT_VERTEX_ARRAY_BIT) {
        memcpy(glstate->vao->vertexattrib, cur->vertexattrib, sizeof(glstate->vao->vertexattrib));
        if (glstate->texture.client != cur->client) 
            gl4es_glClientActiveTexture(GL_TEXTURE0 + cur->client);
    }

    glstate->clientStack->len--;
}

#undef maybe_free
#undef enable_disable
#undef v2
#undef v3
#undef v4

// Exports
AliasExport(void,glPushClientAttrib,,(GLbitfield mask));
AliasExport_V(void,glPopClientAttrib);
AliasExport(void,glPushAttrib,,(GLbitfield mask));
AliasExport_V(void,glPopAttrib);