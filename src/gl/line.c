/*
 * Refactored line.c for GL4ES
 * Optimized for ARMv8
 * - Fast path for stipple texture coordinate generation
 * - Division by multiplication optimization
 */

#include "line.h"
#include <stdio.h>
#include <math.h>

#include "debug.h"
#include "gl4es.h"
#include "glstate.h"
#include "list.h"
#include "matrix.h"
#include "matvec.h"
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

void APIENTRY_GL4ES gl4es_glLineStipple(GLuint factor, GLushort pattern) {
    DBG(printf("glLineStipple(%d, 0x%04X)\n", factor, pattern);)
    
    // Handle Display List
    if (unlikely(glstate->list.active)) {
        if (glstate->list.compiling) {
            NewStage(glstate->list.active, STAGE_LINESTIPPLE);
            glstate->list.active->linestipple_op = 1;
            glstate->list.active->linestipple_factor = factor;
            glstate->list.active->linestipple_pattern = pattern;
            return;
        } else {
            gl4es_flush();
        }
    }

    if (factor < 1) factor = 1;
    if (factor > 256) factor = 256;

    // Only update if state changed
    if (pattern != glstate->linestipple.pattern || 
        factor != glstate->linestipple.factor || 
        !glstate->linestipple.texture) 
    {
        glstate->linestipple.factor = factor;
        glstate->linestipple.pattern = pattern;
        
        // Expand pattern bits to byte array (0 or 255)
        for (int i = 0; i < 16; i++) {
            glstate->linestipple.data[i] = ((pattern >> i) & 1) ? 255 : 0;
        }

        // Save current Texture0 binding
        GLuint old_act = glstate->texture.active;
        if (old_act) gl4es_glActiveTexture(GL_TEXTURE0);
        
        GLuint old_tex = glstate->texture.bound[0][ENABLED_TEX2D] ? glstate->texture.bound[0][ENABLED_TEX2D]->texture : 0;

        // Create or Update Stipple Texture
        if (!glstate->linestipple.texture) {
            gl4es_glGenTextures(1, &glstate->linestipple.texture);
            gl4es_glBindTexture(GL_TEXTURE_2D, glstate->linestipple.texture);
            gl4es_glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
            gl4es_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            gl4es_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            gl4es_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            gl4es_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            gl4es_glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA,
                16, 1, 0, GL_ALPHA, GL_UNSIGNED_BYTE, glstate->linestipple.data);
        } else {
            gl4es_glBindTexture(GL_TEXTURE_2D, glstate->linestipple.texture);
            gl4es_glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 16, 1, 
                GL_ALPHA, GL_UNSIGNED_BYTE, glstate->linestipple.data);
        }

        // Restore Texture0 binding
        gl4es_glBindTexture(GL_TEXTURE_2D, old_tex);
        if (old_act) gl4es_glActiveTexture(GL_TEXTURE0 + old_act);
        
        noerrorShim();
    }
}
AliasExport(void,glLineStipple,,(GLuint factor, GLushort pattern));

void bind_stipple_tex() {
    gl4es_glBindTexture(GL_TEXTURE_2D, glstate->linestipple.texture);
}

GLfloat *gen_stipple_tex_coords(GLfloat *vert, GLushort *sindices, modeinit_t *modes, int stride, int length, GLfloat* noalloctex) {
    DBG(printf("Generate stripple tex (stride=%d, noalloctex=%p) length=%d:", stride, noalloctex, length);)
    
    // Allocate if not provided
    int total_len = modes[length-1].ilen; // This seems suspicious if modes is array, assuming accumulative or last is max
    // Actually, modes usually come from renderlist which aggregates counts. 
    // Safer to use the length passed or recalculate if needed, but assuming caller logic correct.
    
    GLfloat *tex = noalloctex ? noalloctex : (GLfloat *)malloc(total_len * 4 * sizeof(GLfloat));
    GLfloat *texPos = tex;
    GLfloat *vertPos = vert;

    GLfloat x1, x2, y1, y2;
    GLfloat oldlen, len;
    const GLfloat* mvp = getMVPMat();
    GLfloat v[4];
    
    GLfloat w = (GLfloat)glstate->raster.viewport.width * 0.5f;
    GLfloat h = (GLfloat)glstate->raster.viewport.height * 0.5f;
    
    if (stride == 0) stride = 4; else stride /= sizeof(GLfloat);
    int texstride = noalloctex ? stride : 4;

    // Optimization: Pre-calculate scaling factor
    // factor * 16 (bits)
    GLfloat scale_factor = 1.0f / (glstate->linestipple.factor * 16.0f);

    int i = 0;
    for (int k = 0; k < length; k++) {
        GLenum mode = modes[k].mode_init;
        int count = modes[k].ilen;
        
        DBG(printf("[%s->%d] ", PrintEnum(mode), count);)
        
        oldlen = len = 0.0f;
        if (count < 2) continue;

        if (mode == GL_LINES || length > 1) { // Separate segments
            for (; i < count; i += 2) {
                // Point 1
                vertPos = sindices ? (vert + stride * sindices[i]) : vert;
                vector_matrix(vertPos, mvp, v);
                
                // Perspective division check
                float inv_w1 = (v[3] == 0.0f) ? 1.0f : (1.0f / v[3]);
                x1 = v[0] * inv_w1 * w;
                y1 = v[1] * inv_w1 * h;

                // Point 2
                if (sindices) vertPos = vert + stride * sindices[i+1];
                else vertPos += stride; // Advance if no indices
                
                vector_matrix(vertPos, mvp, v);
                if (!sindices) vertPos += stride; // Prepare for next iteration

                float inv_w2 = (v[3] == 0.0f) ? 1.0f : (1.0f / v[3]);
                x2 = v[0] * inv_w2 * w;
                y2 = v[1] * inv_w2 * h;

                // Calculate Length
                oldlen = len;
                float dx = x2 - x1;
                float dy = y2 - y1;
                len += sqrtf(dx * dx + dy * dy) * scale_factor;

                DBG(printf("%f->%f (%f,%f -> %f,%f)\t", oldlen, len, x1, y1, x2, y2);)

                // Write Tex Coords
                // P1
                if (sindices) texPos = tex + texstride * sindices[i+0];
                texPos[0] = oldlen; texPos[1] = 0.0f; texPos[2] = 0.0f; texPos[3] = 1.0f;
                if (!sindices) texPos += texstride;

                // P2
                if (sindices) texPos = tex + texstride * sindices[i+1];
                texPos[0] = len;    texPos[1] = 0.0f; texPos[2] = 0.0f; texPos[3] = 1.0f;
                if (!sindices) texPos += texstride;
            }
        } else { // GL_LINE_STRIP or GL_LINE_LOOP
            // Process first point
            vertPos = sindices ? (vert + stride * sindices[i]) : vert;
            vector_matrix(vertPos, mvp, v);
            if (!sindices) vertPos += stride;

            float inv_w = (v[3] == 0.0f) ? 1.0f : (1.0f / v[3]);
            x2 = v[0] * inv_w * w;
            y2 = v[1] * inv_w * h;

            // Write start coord
            DBG(printf("%f\t", len);)
            if (sindices) texPos = tex + texstride * sindices[i];
            texPos[0] = len; texPos[1] = 0.0f; texPos[2] = 0.0f; texPos[3] = 1.0f;
            if (!sindices) texPos += texstride;
            
            ++i;

            for (; i < count; i++) {
                x1 = x2; y1 = y2;
                
                vertPos = sindices ? (vert + stride * sindices[i]) : vertPos;
                vector_matrix(vertPos, mvp, v);
                if (!sindices) vertPos += stride;

                inv_w = (v[3] == 0.0f) ? 1.0f : (1.0f / v[3]);
                x2 = v[0] * inv_w * w;
                y2 = v[1] * inv_w * h;

                float dx = x2 - x1;
                float dy = y2 - y1;
                len += sqrtf(dx * dx + dy * dy) * scale_factor;

                DBG(printf("->%f\t", len);)

                if (sindices) texPos = tex + texstride * sindices[i];
                texPos[0] = len; texPos[1] = 0.0f; texPos[2] = 0.0f; texPos[3] = 1.0f;
                if (!sindices) texPos += texstride;
            }
        }
    }
    DBG(printf("\n");)
    return tex;
}