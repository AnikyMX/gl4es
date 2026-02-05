/*
 * Refactored listdraw.c for GL4ES
 * Optimized for ARMv8
 * - Replaced Bubble Sort with Insertion Sort for VBO offsets
 * - Optimized VBO generation logic
 * - Reduced branch misprediction in list execution
 */

#include "list.h"
#include "../glx/hardext.h"
#include "wrap/gl4es.h"
#include "fpe.h"
#include "init.h"
#include "line.h"
#include "loader.h"
#include "matrix.h"
#include "texgen.h"
#include "render.h"

#ifndef likely
#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)
#endif

/* return 1 if failed, 2 if succeed */
typedef struct array2vbo_s {
    uintptr_t   real_base;
    uint32_t    real_size;
    uint32_t    stride;
    uintptr_t   vbo_base;
    uintptr_t   vbo_basebase;
} array2vbo_t;

// Optimized VBO generator for Display Lists
int list2VBO(renderlist_t* list)
{
    LOAD_GLES3(glGenBuffers);
    LOAD_GLES3(glBufferData);
    LOAD_GLES3(glBufferSubData);
    
    array2vbo_t work[ATT_MAX] = {0};
    int imax = 0;
    int len = list->len;
    
    // Macro to populate work array fast
    #define ADD_WORK(PTR, STRIDE, DEF_STRIDE) \
        if (PTR) { \
            work[imax].real_base = (uintptr_t)PTR; \
            work[imax].stride = STRIDE; \
            if (!work[imax].stride) work[imax].stride = DEF_STRIDE; \
            work[imax].real_size = work[imax].stride * len; \
            imax++; \
        }

    ADD_WORK(list->vert, list->vert_stride, 16);      // 4 floats
    ADD_WORK(list->color, list->color_stride, 16);    // 4 floats
    ADD_WORK(list->secondary, list->secondary_stride, 16);
    ADD_WORK(list->fogcoord, list->fogcoord_stride, 4);
    ADD_WORK(list->normal, list->normal_stride, 12);  // 3 floats

    for (int a = 0; a < list->maxtex; ++a) {
        ADD_WORK(list->tex[a], list->tex_stride[a], 16);
    }
    #undef ADD_WORK

    // Sort by real address to optimize memory copy order
    // Use Insertion Sort (faster for small N=16)
    int sorted[ATT_MAX];
    for (int i = 0; i < imax; ++i) sorted[i] = i;
    
    for (int i = 1; i < imax; ++i) {
        int key = sorted[i];
        int j = i - 1;
        while (j >= 0 && work[sorted[j]].real_base > work[key].real_base) {
            sorted[j + 1] = sorted[j];
            j--;
        }
        sorted[j + 1] = key;
    }

    // Calculate VBO layout (deduplicating overlapping regions)
    uintptr_t vbo_base = 0;
    for (int i = 0; i < imax; ++i) {
        uintptr_t base = vbo_base;
        uintptr_t basebase = vbo_base;
        array2vbo_t *r = &work[sorted[i]];
        
        // Check overlap with previous attribute
        if (i > 0) {
            array2vbo_t *t = &work[sorted[i-1]];
            if (r->real_base < t->real_base + t->real_size) {
                // Overlap detected (e.g. interleaved arrays)
                base = r->vbo_base + (r->real_base - t->real_base);
                basebase = r->vbo_basebase;
            }
        }
        
        r->vbo_base = base;
        r->vbo_basebase = basebase;
        
        // Only advance vbo_base if this is a new block
        if (base == basebase)
            vbo_base += r->real_size;
    }

    if (!vbo_base) return 1;

    // Create and fill VBO
    gles_glGenBuffers(1, &list->vbo_array);
    bindBuffer(GL_ARRAY_BUFFER, list->vbo_array);
    gles_glBufferData(GL_ARRAY_BUFFER, vbo_base, NULL, GL_STATIC_DRAW);
    
    for (int i = 0; i < imax; ++i) {
        array2vbo_t *r = &work[sorted[i]];
        // Only upload unique data blocks
        if (r->vbo_base == r->vbo_basebase)
            gles_glBufferSubData(GL_ARRAY_BUFFER, r->vbo_basebase, r->real_size, (void*)r->real_base);
    }

    // Map VBO offsets back to list
    // Note: We iterate in original order (not sorted) to match list pointers
    imax = 0;
    #define SET_VBO_PTR(PTR, VBO_PTR) \
        if (PTR) { VBO_PTR = (GLfloat*)work[imax].vbo_base; imax++; }

    SET_VBO_PTR(list->vert, list->vbo_vert);
    SET_VBO_PTR(list->color, list->vbo_color);
    SET_VBO_PTR(list->secondary, list->vbo_secondary);
    SET_VBO_PTR(list->fogcoord, list->vbo_fogcoord);
    SET_VBO_PTR(list->normal, list->vbo_normal);
    
    for (int a = 0; a < list->maxtex; ++a) {
        SET_VBO_PTR(list->tex[a], list->vbo_tex[a]);
    }
    #undef SET_VBO_PTR

    return 2;
}

typedef struct save_vbo_s {
    GLuint          real_buffer;
    const GLvoid* real_pointer;
    glbuffer_t* buffer;
} save_vbo_t;

void listActiveVBO(renderlist_t* list, save_vbo_t* saved) {
    #define ACTIVATE_VBO(ID, PTR, VBO_PTR) \
    if (PTR) { \
        saved[ID].real_buffer = glstate->vao->vertexattrib[ID].real_buffer; \
        saved[ID].real_pointer = glstate->vao->vertexattrib[ID].real_pointer; \
        saved[ID].buffer = glstate->vao->vertexattrib[ID].buffer; \
        glstate->vao->vertexattrib[ID].real_buffer = list->vbo_array; \
        glstate->vao->vertexattrib[ID].real_pointer = VBO_PTR; \
        glstate->vao->vertexattrib[ID].buffer = NULL; \
    }

    ACTIVATE_VBO(ATT_VERTEX, list->vert, list->vbo_vert);
    ACTIVATE_VBO(ATT_COLOR, list->color, list->vbo_color);
    ACTIVATE_VBO(ATT_SECONDARY, list->secondary, list->vbo_secondary);
    ACTIVATE_VBO(ATT_FOGCOORD, list->fogcoord, list->vbo_fogcoord);
    ACTIVATE_VBO(ATT_NORMAL, list->normal, list->vbo_normal);
    
    for (int a = 0; a < list->maxtex; ++a) {
        ACTIVATE_VBO(ATT_MULTITEXCOORD0+a, list->tex[a], list->vbo_tex[a]);
    }
    #undef ACTIVATE_VBO
}

void listInactiveVBO(renderlist_t* list, save_vbo_t* saved) {
    #define DEACTIVATE_VBO(ID, PTR) \
    if (PTR) { \
        glstate->vao->vertexattrib[ID].real_buffer = saved[ID].real_buffer; \
        glstate->vao->vertexattrib[ID].real_pointer = saved[ID].real_pointer; \
        glstate->vao->vertexattrib[ID].buffer = saved[ID].buffer; \
    }

    DEACTIVATE_VBO(ATT_VERTEX, list->vert);
    DEACTIVATE_VBO(ATT_COLOR, list->color);
    DEACTIVATE_VBO(ATT_SECONDARY, list->secondary);
    DEACTIVATE_VBO(ATT_FOGCOORD, list->fogcoord);
    DEACTIVATE_VBO(ATT_NORMAL, list->normal);
    
    for (int a = 0; a < list->maxtex; ++a) {
        DEACTIVATE_VBO(ATT_MULTITEXCOORD0+a, list->tex[a]);
    }
    #undef DEACTIVATE_VBO
}

// Helper to convert primitives to GL_LINES indices
// Optimized loop unrolling and pointer access
int fill_lineIndices(modeinit_t *modes, int length, GLenum mode, GLushort* indices, GLushort *ind_line)
{
    int k = 0;
    int i = 0;
    
    // Macro for safe index access
    #define IND(x) (indices ? indices[x] : (GLushort)(x))

    for (int m = 0; m < length; m++) {
        GLenum mode_init = modes[m].mode_init;
        int len = modes[m].ilen;
        
        if (len <= 0) continue;

        switch (mode) {
            case GL_TRIANGLE_STRIP:
                if (len > 2) {
                    ind_line[k++] = IND(i+0); ind_line[k++] = IND(i+1);
                    i += 2;
                    for (; i < len; i++) {
                        ind_line[k++] = IND(i-2); ind_line[k++] = IND(i);
                        ind_line[k++] = IND(i-1); ind_line[k++] = IND(i);
                    }
                } else {
                    i += len;
                }
                break;
                
            case GL_TRIANGLE_FAN:
                if (mode_init == GL_QUAD_STRIP) {
                    if (len > 3) {
                        ind_line[k++] = IND(i+0); ind_line[k++] = IND(i+1);
                        i += 2;
                        for (; i < len - 1; i += 2) {
                            ind_line[k++] = IND(i-1); ind_line[k++] = IND(i);
                            ind_line[k++] = IND(i-2); ind_line[k++] = IND(i+1);
                            ind_line[k++] = IND(i+0); ind_line[k++] = IND(i+1);
                        }
                        if (i < len) i++; // Handle odd remaining
                    } else {
                        i += len;
                    }
                } else if (mode_init == GL_POLYGON) {
                    if (len > 1) {
                        int z = i;
                        ind_line[k++] = IND(i+0); ind_line[k++] = IND(i+1);
                        ++i;
                        for (; i < len; i++) {
                            ind_line[k++] = IND(i-1); ind_line[k++] = IND(i);
                        }
                        ind_line[k++] = IND(len-1); ind_line[k++] = IND(z);
                    } else {
                        i += len;
                    }
                } else {
                    // Standard Fan
                    if (len > 2) {
                        int z = i;
                        ind_line[k++] = IND(i+0); ind_line[k++] = IND(i+1);
                        i += 2;
                        for (; i < len; i++) {
                            ind_line[k++] = IND(z);   ind_line[k++] = IND(i);
                            ind_line[k++] = IND(i-1); ind_line[k++] = IND(i);
                        }
                    } else {
                        i += len;
                    }
                }
                break;
                
            case GL_TRIANGLES:
                switch (mode_init) {
                    case GL_TRIANGLE_STRIP:
                    case GL_TRIANGLE_FAN:
                    case GL_TRIANGLES:
                        if (len > 2) {
                            for (; i < len - 2; i += 3) {
                                GLushort v0 = IND(i+0);
                                GLushort v1 = IND(i+1);
                                GLushort v2 = IND(i+2);
                                ind_line[k++] = v0; ind_line[k++] = v1;
                                ind_line[k++] = v1; ind_line[k++] = v2;
                                ind_line[k++] = v2; ind_line[k++] = v0;
                            }
                            i += (len % 3); // Skip remainder
                        } else {
                            i += len;
                        }
                        break;
                        
                    case GL_QUADS:
                        if (len > 3) {
                            if (len == 4) {
                                GLushort v0=IND(i), v1=IND(i+1), v2=IND(i+2), v3=IND(i+3);
                                ind_line[k++] = v0; ind_line[k++] = v1;
                                ind_line[k++] = v1; ind_line[k++] = v2;
                                ind_line[k++] = v2; ind_line[k++] = v3;
                                ind_line[k++] = v3; ind_line[k++] = v0;
                                i += 4;
                            } else {
                                // Triangulated Quads (2 triangles per quad)
                                for (; i < len - 5; i += 6) {
                                    GLushort v0=IND(i), v1=IND(i+1), v2=IND(i+2), v3=IND(i+5);
                                    ind_line[k++] = v0; ind_line[k++] = v1;
                                    ind_line[k++] = v1; ind_line[k++] = v2;
                                    ind_line[k++] = v2; ind_line[k++] = v3;
                                    ind_line[k++] = v3; ind_line[k++] = v0;
                                }
                                i += (len % 6);
                            }
                        } else {
                            i += len;
                        }
                        break;
                        
                    case GL_QUAD_STRIP:
                        if (len > 3) {
                            ind_line[k++] = IND(i+0); ind_line[k++] = IND(i+1);
                            i += 2;
                            for (; i < len - 1; i += 2) {
                                ind_line[k++] = IND(i-1); ind_line[k++] = IND(i);
                                ind_line[k++] = IND(i-2); ind_line[k++] = IND(i+1);
                                ind_line[k++] = IND(i+0); ind_line[k++] = IND(i+1);
                            }
                            if (i < len) i++;
                        } else {
                            i += len;
                        }
                        break;
                        
                    case GL_POLYGON:
                        if (len > 1) {
                            int z = i;
                            ind_line[k++] = IND(i+0); ind_line[k++] = IND(i+1);
                            ++i;
                            for (; i < len; i++) {
                                ind_line[k++] = IND(i-1); ind_line[k++] = IND(i);
                            }
                            ind_line[k++] = IND(len-1); ind_line[k++] = IND(z);
                        } else {
                            i += len;
                        }
                        break;
                        
                    default:
                        i += len; 
                        break;
                }
                break;
            default:
                i += len;
                break;
        }
    }
    #undef IND
    return k;
}

void draw_renderlist(renderlist_t *list) {
    if (unlikely(!list)) return;
    
    // Rewind to start
    while (list->prev) list = list->prev;

    LOAD_GLES_FPE(glDrawArrays);
    LOAD_GLES_FPE(glDrawElements);
    LOAD_GLES_FPE(glVertexPointer);
    LOAD_GLES_FPE(glNormalPointer);
    LOAD_GLES_FPE(glColorPointer);
    LOAD_GLES_FPE(glTexCoordPointer);
    LOAD_GLES_FPE(glEnable);
    LOAD_GLES_FPE(glDisable);
    
    // Push client state once for the whole list execution to prevent side effects
    gl4es_glPushClientAttrib(GL_CLIENT_VERTEX_ARRAY_BIT);

    int old_tex = glstate->texture.client;
    GLuint cur_tex = old_tex;
    
    // Cache array tracking
    GLushort *indices;
    int use_texgen[MAX_TEX] = {0};
    GLint needclean[MAX_TEX] = {0};
    
    // Stipple State Caching
    bool stipple;
    int stipple_tmu;
    GLenum stipple_env;
    GLenum stipple_afunc;
    GLfloat stipple_aref;
    int stipple_tex2d;
    int stipple_alpha;
    int stipple_old;
    int stipple_texgen[4];

    do {
        if (list->open) list = end_renderlist(list);
        
        if (list->pushattribute) gl4es_glPushAttrib(list->pushattribute);
        if (list->popattribute) gl4es_glPopAttrib();
        
        // Execute Calls
        if (list->calls.len > 0) {
            for (int i = 0; i < list->calls.len; i++) {
                glPackedCall(list->calls.calls[i]);
            }
        }

        // Render Ops
        if (list->render_op) {
            switch(list->render_op) {
                case 1: gl4es_glInitNames(); break;
                case 2: gl4es_glPopName(); break;
                case 3: gl4es_glPushName(list->render_arg); break;
                case 4: gl4es_glLoadName(list->render_arg); break;
            }
        }

        if (list->fog_op) gl4es_glFogfv(GL_FOG_COLOR, list->fog_val);
        
        if (list->pointparam_op == 1) 
            gl4es_glPointParameterfv(GL_POINT_DISTANCE_ATTENUATION , list->pointparam_val);

        if (list->matrix_op) {
            if (list->matrix_op == 1) gl4es_glLoadMatrixf(list->matrix_val);
            else if (list->matrix_op == 2) gl4es_glMultMatrixf(list->matrix_val);
        }

        if (list->set_tmu) gl4es_glActiveTexture(GL_TEXTURE0 + list->tmu);
        if (list->set_texture) gl4es_glBindTexture(list->target_texture, list->texture);

        // Raster Ops (Optimized Switch)
        if (list->raster_op) {
            switch(list->raster_op & 0xFFFF) {
                case 1: gl4es_glRasterPos3f(list->raster_xyz[0], list->raster_xyz[1], list->raster_xyz[2]); break;
                case 2: gl4es_glWindowPos3f(list->raster_xyz[0], list->raster_xyz[1], list->raster_xyz[2]); break;
                case 3: gl4es_glPixelZoom(list->raster_xyz[0], list->raster_xyz[1]); break;
                default: 
                    if ((list->raster_op & 0x10000) == 0x10000)
                        gl4es_glPixelTransferf(list->raster_op & 0xFFFF, list->raster_xyz[0]);
                    break;
            }
        }

        if (list->raster) render_raster_list(list->raster);

        if (list->bitmaps) {
            for (int i = 0; i < list->bitmaps->count; i++) {
                bitmap_list_t *l = &list->bitmaps->list[i];
                gl4es_glBitmap(l->width, l->height, l->xorig, l->yorig, l->xmove, l->ymove, l->bitmap);
            }
        }

        // Material Updates
        if (list->material) {
            rendermaterial_t *m;
            kh_foreach_value(list->material, m,
                if (m->pname == GL_SHININESS) gl4es_glMaterialf(m->face, m->pname, m->color[0]);
                else gl4es_glMaterialfv(m->face, m->pname, m->color);
            )
        }

        if (list->colormat_face) gl4es_glColorMaterial(list->colormat_face, list->colormat_mode);

        // Light Updates
        if (list->light) {
            renderlight_t *m;
            kh_foreach_value(list->light, m,
                gl4es_glLightfv(m->which, m->pname, m->color);
            )
        }

        if (list->lightmodel) gl4es_glLightModelfv(list->lightmodelparam, list->lightmodel);

        if (list->linestipple_op) gl4es_glLineStipple(list->linestipple_factor, list->linestipple_pattern);
        
        // Texture Environment
        if (list->texenv) {
            rendertexenv_t *m;
            kh_foreach_value(list->texenv, m,
                gl4es_glTexEnvfv(m->target, m->pname, m->params);
            )
        }

        // Texture Gen
        if (list->texgen) {
            rendertexgen_t *m;
            kh_foreach_value(list->texgen, m,
                gl4es_glTexGenfv(m->coord, m->pname, m->color);
            )
        }
        
        if (list->polygon_mode) gl4es_glPolygonMode(GL_FRONT_AND_BACK, list->polygon_mode);

        // === Drawing Section ===
        if (!list->len) continue;

        int use_vbo_array = list->use_vbo_array;
        if (!use_vbo_array && (hardext.esversion == 1 || globals4es.usevbo == 0 || !list->name)) {
            use_vbo_array = 1; // Fallback to Client Arrays
        }
        
        int use_vbo_indices = list->use_vbo_indices;
        if (!use_vbo_indices && (hardext.esversion == 1 || globals4es.usevbo == 0 || !list->name)) {
            use_vbo_indices = 1;
        }

        // Vertex Pointer Setup
        if (list->vert) {
            fpe_glEnableClientState(GL_VERTEX_ARRAY);
            gles_glVertexPointer(4, GL_FLOAT, list->vert_stride, list->vert);
        } else {
            fpe_glDisableClientState(GL_VERTEX_ARRAY);
        }

        if (list->normal) {
            fpe_glEnableClientState(GL_NORMAL_ARRAY);
            gles_glNormalPointer(GL_FLOAT, list->normal_stride, list->normal);
        } else {
            fpe_glDisableClientState(GL_NORMAL_ARRAY);
        }
        // Indices setup
        indices = list->indices;

        if (unlikely(glstate->raster.bm_drawing))
            bitmap_flush();

        // --- Color Pointer ---
        if (list->color) {
            fpe_glEnableClientState(GL_COLOR_ARRAY);
            // Handle ES1.1 Color Sum (Primary + Secondary) manually if needed
            if (glstate->enable.color_sum && list->secondary && hardext.esversion == 1 && !list->use_glstate) {
                if (!list->final_colors) {
                    list->final_colors = (GLfloat*)malloc(list->len * 4 * sizeof(GLfloat));
                    if (indices) {
                        for (int i = 0; i < list->ilen; i++) {
                            int k = indices[i] * 4;
                            for (int j = 0; j < 4; j++)
                                list->final_colors[k+j] = list->color[k+j] + list->secondary[k+j];
                        }
                    } else {
                        for (int i = 0; i < list->len * 4; i++)
                            list->final_colors[i] = list->color[i] + list->secondary[i];
                    }
                }
                gles_glColorPointer(4, GL_FLOAT, 0, list->final_colors);
            } else {
                gles_glColorPointer(4, GL_FLOAT, list->color_stride, list->color);
            }
        } else {
            fpe_glDisableClientState(GL_COLOR_ARRAY);
        }

        // --- ES2+ Attributes ---
        if (hardext.esversion > 1) {
            if (glstate->enable.color_sum && list->secondary) {
                fpe_glEnableClientState(GL_SECONDARY_COLOR_ARRAY);
                fpe_glSecondaryColorPointer(4, GL_FLOAT, list->secondary_stride, list->secondary);
            } else {
                fpe_glDisableClientState(GL_SECONDARY_COLOR_ARRAY);
            }

            if ((glstate->fog.coord_src == GL_FOG_COORD) && list->fogcoord) {
                fpe_glEnableClientState(GL_FOG_COORD_ARRAY);
                fpe_glFogCoordPointer(GL_FLOAT, list->fogcoord_stride, list->fogcoord);
            } else {
                fpe_glDisableClientState(GL_FOG_COORD_ARRAY);
            }
        }

        // --- Texture Coordinate Setup ---
        #define TEXTURE(A) if (cur_tex != A) { gl4es_glClientActiveTexture(A + GL_TEXTURE0); cur_tex = A; }
        
        stipple = false;
        if ((list->mode == GL_LINES || list->mode == GL_LINE_STRIP || list->mode == GL_LINE_LOOP) && glstate->enable.line_stipple) {
            stipple = true;
            stipple_tmu = (get_target(glstate->enable.texture[0]) != -1) ? 1 : 0;
        }

        if (stipple) {
            if (!use_vbo_array) use_vbo_array = 1;
            stipple_old = glstate->gleshard->active;
            
            if (glstate->gleshard->active != stipple_tmu) {
                LOAD_GLES(glActiveTexture);
                gl4es_glActiveTexture(GL_TEXTURE0 + stipple_tmu);
            }
            
            TEXTURE(stipple_tmu);
            
            // Setup Stipple Texture Matrix
            GLenum matmode;
            gl4es_glGetIntegerv(GL_MATRIX_MODE, (GLint*)&matmode);
            gl4es_glMatrixMode(GL_TEXTURE);
            gl4es_glPushMatrix();
            gl4es_glLoadIdentity();
            gl4es_glMatrixMode(matmode);
            
            stipple_env = glstate->texenv[stipple_tmu].env.mode;
            gl4es_glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
            
            stipple_tex2d = gl4es_glIsEnabled(GL_TEXTURE_2D);
            stipple_alpha = gl4es_glIsEnabled(GL_ALPHA_TEST);
            
            gl4es_glEnable(GL_TEXTURE_2D);
            gl4es_glEnable(GL_ALPHA_TEST);
            
            for (int k = 0; k < 4; k++) {
                stipple_texgen[k] = gl4es_glIsEnabled(GL_TEXTURE_GEN_S + k);
                if (stipple_texgen[k]) gl4es_glDisable(GL_TEXTURE_GEN_S + k);
            }
            
            stipple_afunc = glstate->alphafunc;
            stipple_aref = glstate->alpharef;
            gl4es_glAlphaFunc(GL_GREATER, 0.0f);
            
            bind_stipple_tex();
            
            modeinit_t tmp; 
            tmp.mode_init = list->mode_init; 
            tmp.ilen = list->ilen ? list->ilen : list->len;
            
            list->tex[stipple_tmu] = gen_stipple_tex_coords(list->vert, list->indices, list->mode_inits ? list->mode_inits : &tmp, 
                                                            list->vert_stride, list->mode_inits ? list->mode_init_len : 1, 
                                                            (list->use_glstate) ? (list->vert + 8 + stipple_tmu * 4) : NULL);
        }

        #define RS(A, LEN) \
            if (glstate->texgenedsz[A] < LEN) { \
                free(glstate->texgened[A]); \
                glstate->texgened[A] = malloc(4 * sizeof(GLfloat) * LEN); \
                glstate->texgenedsz[A] = LEN; \
            } \
            use_texgen[A] = 1

        if (hardext.esversion == 1) {
            for (int a = 0; a < hardext.maxtex; a++) {
                if (glstate->enable.texture[a] || (stipple && a == stipple_tmu)) {
                    const GLint itarget = (stipple && a == stipple_tmu) ? ENABLED_TEX2D : get_target(glstate->enable.texture[a]);
                    needclean[a] = 0;
                    use_texgen[a] = 0;

                    // TexGen or Missing Coords generation
                    if (glstate->enable.texgen_s[a] || glstate->enable.texgen_t[a] || glstate->enable.texgen_r[a] || glstate->enable.texgen_q[a]) {
                        TEXTURE(a);
                        RS(a, list->len);
                        gen_tex_coords(list->vert, list->normal, &glstate->texgened[a], list->len, &needclean[a], a, (list->ilen < list->len) ? indices : NULL, (list->ilen < list->len) ? list->ilen : 0);
                    } else if ((list->tex[a] == NULL) && !(list->mode == GL_POINT && glstate->texture.pscoordreplace[a])) {
                        RS(a, list->len);
                        gen_tex_coords(list->vert, list->normal, &glstate->texgened[a], list->len, &needclean[a], a, (list->ilen < list->len) ? indices : NULL, (list->ilen < list->len) ? list->ilen : 0);
                    }

                    // Texture Matrix Adjustment
                    gltexture_t *bound = glstate->texture.bound[a][itarget];
                    if ((list->tex[a] || (use_texgen[a] && !needclean[a])) && ((!(globals4es.texmat || glstate->texture_matrix[a]->identity)) || (bound->adjust))) {
                        if (!use_texgen[a]) {
                            RS(a, list->len);
                            if (list->tex_stride[a]) {
                                GLfloat *src = list->tex[a];
                                GLfloat *dst = glstate->texgened[a];
                                int stride = list->tex_stride[a] >> 2;
                                for (int ii = 0; ii < list->len; ii++) {
                                    memcpy(dst, src, 4 * sizeof(GLfloat));
                                    src += stride;
                                    dst += 4;
                                }
                            } else {
                                memcpy(glstate->texgened[a], list->tex[a], 4 * sizeof(GLfloat) * list->len);
                            }
                        }
                        if (!(globals4es.texmat || glstate->texture_matrix[a]->identity))
                            tex_coord_matrix(glstate->texgened[a], list->len, getTexMat(a));
                        if (bound->adjust)
                            tex_coord_npot(glstate->texgened[a], list->len, bound->width, bound->height, bound->nwidth, bound->nheight);
                    }
                }

                if (list->tex[a] || (use_texgen[a] && !needclean[a])) {
                    TEXTURE(a);
                    fpe_glEnableClientState(GL_TEXTURE_COORD_ARRAY);
                    gles_glTexCoordPointer(4, GL_FLOAT, (use_texgen[a]) ? 0 : list->tex_stride[a], (use_texgen[a]) ? glstate->texgened[a] : list->tex[a]);
                } else {
                    if (glstate->gleshard->vertexattrib[ATT_MULTITEXCOORD0 + a].enabled || (hardext.esversion != 1)) {
                        TEXTURE(a);
                        fpe_glDisableClientState(GL_TEXTURE_COORD_ARRAY);
                    }
                }

                // Enable/Disable 2D texture state if implicit (ES1)
                if (!IS_TEX2D(glstate->enable.texture[a]) && (IS_ANYTEX(glstate->enable.texture[a]))) {
                    TEXTURE(a);
                    gl4es_glActiveTexture(GL_TEXTURE0 + a);
                    realize_active();
                    gles_glEnable(GL_TEXTURE_2D);
                }
            }
        } else {
            // ES2+ Texture Loop (Simpler)
            for (int a = 0; a < hardext.maxtex; a++) {
                if (list->tex[a]) {
                    TEXTURE(a);
                    fpe_glEnableClientState(GL_TEXTURE_COORD_ARRAY);
                    gles_glTexCoordPointer(4, GL_FLOAT, list->tex_stride[a], list->tex[a]);
                } else {
                    TEXTURE(a);
                    fpe_glDisableClientState(GL_TEXTURE_COORD_ARRAY);
                }
            }
        }
        
        if (glstate->texture.client != old_tex) TEXTURE(old_tex);
        #undef RS
        #undef TEXTURE

        realize_textures(1);

        // --- VBO Handling ---
        if (use_vbo_array == 0) {
            if ((glstate->render_mode == GL_SELECT) || (glstate->polygon_mode == GL_LINE) || (glstate->polygon_mode == GL_POINT))
                use_vbo_array = 1; // Don't use VBO for Select or special polygon modes
            else
                use_vbo_array = list2VBO(list);
        }
        
        save_vbo_t saved[NB_VA];
        if (use_vbo_array == 2)
            listActiveVBO(list, saved);
            
        if (list->use_vbo_array != use_vbo_array)
            list->use_vbo_array = use_vbo_array;
        
        // --- Mode Adjustment ---
        GLenum mode = list->mode;
        if ((glstate->polygon_mode == GL_LINE) && (mode >= GL_TRIANGLES)) mode = GL_LINES;
        if ((glstate->polygon_mode == GL_POINT) && (mode >= GL_TRIANGLES)) mode = GL_POINTS;

        // --- Execution ---
        if (indices) {
            if (glstate->render_mode == GL_SELECT) {
                vertexattrib_t vtx = {0};
                vtx.pointer = list->vert;
                vtx.type = GL_FLOAT;
                vtx.normalized = GL_FALSE;
                vtx.size = 4;
                vtx.stride = 0;
                select_glDrawElements(&vtx, list->mode, list->ilen, GL_UNSIGNED_SHORT, indices);
                use_vbo_indices = 1;
            } else {
                GLuint old_index = wantBufferIndex(0);
                
                // Wireframe Emulation
                if (glstate->polygon_mode == GL_LINE && list->mode_init >= GL_TRIANGLES) {
                    int ilen = list->ilen;
                    if (!list->ind_lines) {
                        list->ind_lines = (GLushort*)malloc(sizeof(GLushort) * ilen * 4 + 2);
                        modeinit_t tmp; tmp.mode_init = list->mode_init; tmp.ilen = list->ilen;
                        int k = fill_lineIndices(list->mode_inits ? list->mode_inits : &tmp, list->mode_inits ? list->mode_init_len : 1, list->mode, indices, list->ind_lines);
                        list->ind_line = k;
                    }
                    bindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
                    gles_glDrawElements(mode, list->ind_line, GL_UNSIGNED_SHORT, list->ind_lines);
                    use_vbo_indices = 1;
                } else {
                    int vbo_indices = 0;
                    if (!use_vbo_indices) {
                        LOAD_GLES3(glGenBuffers);
                        LOAD_GLES3(glBufferData);
                        gles_glGenBuffers(1, &list->vbo_indices);
                        bindBuffer(GL_ELEMENT_ARRAY_BUFFER, list->vbo_indices);
                        gles_glBufferData(GL_ELEMENT_ARRAY_BUFFER, list->ilen * sizeof(GLushort), indices, GL_STATIC_DRAW);
                        use_vbo_indices = 2;
                        vbo_indices = 1;
                    } else if (use_vbo_indices == 2) {
                        bindBuffer(GL_ELEMENT_ARRAY_BUFFER, list->vbo_indices);
                        vbo_indices = 1;
                    } else {
                        realize_bufferIndex();
                    }

                    if (list->instanceCount == 1)
                        gles_glDrawElements(mode, list->ilen, GL_UNSIGNED_SHORT, vbo_indices ? NULL : indices);
                    else {
                        // Instanced Draw Loop (or Extension call if available)
                        // FPE Instancing usually handles loop internally if ext missing
                        for (glstate->instanceID = 0; glstate->instanceID < list->instanceCount; ++glstate->instanceID)
                            gles_glDrawElements(mode, list->ilen, GL_UNSIGNED_SHORT, vbo_indices ? NULL : indices);
                        glstate->instanceID = 0;
                    }
                }
                wantBufferIndex(old_index);
            }
        } else {
            // Arrays (No Indices)
            if (glstate->render_mode == GL_SELECT) {    
                vertexattrib_t vtx = {0};
                vtx.pointer = list->vert;
                vtx.type = GL_FLOAT;
                vtx.size = 4;
                vtx.normalized = GL_FALSE;
                vtx.stride = 0;
                select_glDrawArrays(&vtx, list->mode, 0, list->len);
            } else {
                int len = list->len;
                if ((glstate->polygon_mode == GL_LINE) && (list->mode_init >= GL_TRIANGLES)) {
                    if (!list->ind_lines) {
                        list->ind_lines = (GLushort*)malloc(sizeof(GLushort) * len * 4 + 2);
                        modeinit_t tmp; tmp.mode_init = list->mode_init; tmp.ilen = len;
                        int k = fill_lineIndices(list->mode_inits ? list->mode_inits : &tmp, list->mode_inits ? list->mode_init_len : 1, list->mode, NULL, list->ind_lines);
                        list->ind_line = k;
                    }
                    bindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
                    gles_glDrawElements(mode, list->ind_line, GL_UNSIGNED_SHORT, list->ind_lines);
                } else {
                    if (list->instanceCount == 1)
                        gles_glDrawArrays(mode, 0, len);
                    else {
                        for (glstate->instanceID = 0; glstate->instanceID < list->instanceCount; ++glstate->instanceID)
                            gles_glDrawArrays(mode, 0, len);
                        glstate->instanceID = 0;
                    }
                }
            }
        }

        if (list->use_vbo_indices != use_vbo_indices)
            list->use_vbo_indices = use_vbo_indices;
            
        if (use_vbo_array == 2)
            listInactiveVBO(list, saved);

        // --- Cleanup & Restoration ---
        #define TEXTURE(A) if (cur_tex != A) { gl4es_glClientActiveTexture(A + GL_TEXTURE0); cur_tex = A; }
        
        if (hardext.esversion == 1) {
            for (int a = 0; a < hardext.maxtex; a++) {
                if (needclean[a]) {
                    TEXTURE(a);
                    gen_tex_clean(needclean[a], a);
                }
                if (!IS_TEX2D(glstate->enable.texture[a]) && (IS_ANYTEX(glstate->enable.texture[a]))) {
                    TEXTURE(a);
                    gles_glDisable(GL_TEXTURE_2D);
                }
            }
        }
        
        if (glstate->texture.client != old_tex) TEXTURE(old_tex);
        #undef TEXTURE

        if (stipple) {
            if (!list->use_glstate) free(list->tex[stipple_tmu]);
            list->tex[stipple_tmu] = NULL;
            
            LOAD_GLES(glActiveTexture);
            if (glstate->gleshard->active != stipple_tmu)
                gl4es_glActiveTexture(GL_TEXTURE0 + stipple_tmu);
            
            GLenum matmode;
            gl4es_glGetIntegerv(GL_MATRIX_MODE, (GLint*)&matmode);
            gl4es_glMatrixMode(GL_TEXTURE);
            gl4es_glPopMatrix();
            gl4es_glMatrixMode(matmode);
            
            gl4es_glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, stipple_env);
            gl4es_glAlphaFunc(stipple_afunc, stipple_aref);
            
            if (stipple_tex2d) gl4es_glEnable(GL_TEXTURE_2D);
            else gl4es_glDisable(GL_TEXTURE_2D);
            
            if (stipple_alpha) gl4es_glEnable(GL_ALPHA_TEST);
            else gl4es_glDisable(GL_ALPHA_TEST);
            
            for (int k = 0; k < 4; k++) {
                if (stipple_texgen[k]) gl4es_glEnable(GL_TEXTURE_GEN_S + k);
            }
            
            if (glstate->gleshard->active != stipple_old)
                gl4es_glActiveTexture(GL_TEXTURE0 + stipple_old);
        }

        if (list->post_color) gl4es_glColor4fv(list->post_colors);
        if (list->post_normal) gl4es_glNormal3fv(list->post_normals);

    } while ((list = list->next)); // Iterate through next list in batch

    gl4es_glPopClientAttrib();
}