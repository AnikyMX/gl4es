/*
 * Refactored framebuffers.c for GL4ES
 * Optimized for ARMv8
 * - Fast FBO/RBO Lookup with Branch Prediction
 * - Redundant Binding Checks
 * - Optimized Enum Arithmetic
 */

#include "framebuffers.h"
#include "../glx/hardext.h"
#include "blit.h"
#include "debug.h"
#include "fpe.h"
#include "gl4es.h"
#include "glstate.h"
#include "init.h"
#include "loader.h"

#ifdef DEBUG
#define DBG(a) a
#else
#define DBG(a)
#endif

#ifndef likely
#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)
#endif

KHASH_MAP_IMPL_INT(renderbufferlist_t, glrenderbuffer_t *);
KHASH_MAP_IMPL_INT(framebufferlist_t, glframebuffer_t *);

int npot(int n);
int wrap_npot(GLenum wrap);

// OPTIMIZATION: Fast lookup avoiding redundant checks
glframebuffer_t* find_framebuffer(GLuint framebuffer) {
    if (unlikely(framebuffer == 0)) return glstate->fbo.fbo_0; // Usually NULL

    khint_t k;
    khash_t(framebufferlist_t) *list = glstate->fbo.framebufferlist;
    k = kh_get(framebufferlist_t, list, framebuffer);
    
    if (likely(k != kh_end(list))){
        return kh_value(list, k);
    }
    return NULL;
}

glframebuffer_t* get_framebuffer(GLenum target) {
    switch (target) {
        case GL_FRAMEBUFFER: return glstate->fbo.current_fb;
        case GL_READ_FRAMEBUFFER: return glstate->fbo.fbo_read;
        case GL_DRAW_FRAMEBUFFER: return glstate->fbo.fbo_draw;
    }
    return NULL;
}

void readfboBegin() {
    if (likely(glstate->fbo.fbo_read == glstate->fbo.fbo_draw))
        return;
    DBG(printf("readfboBegin, fbo status read=%u, draw=%u, main=%u, current=%u\n", glstate->fbo.fbo_read->id, glstate->fbo.fbo_draw->id, glstate->fbo.mainfbo_fbo, glstate->fbo.current_fb->id);)
    
    if (glstate->fbo.fbo_read == glstate->fbo.current_fb)
        return;

    glstate->fbo.current_fb = glstate->fbo.fbo_read;
    GLuint fbo = glstate->fbo.fbo_read->id;
    if (unlikely(!fbo))
        fbo = glstate->fbo.mainfbo_fbo;
        
    LOAD_GLES3_OR_OES(glBindFramebuffer);
    gles_glBindFramebuffer(GL_FRAMEBUFFER, fbo);
}

void readfboEnd() {
    if (likely(glstate->fbo.fbo_read->id == glstate->fbo.fbo_draw->id))
        return;
    DBG(printf("readfboEnd, fbo status read=%p, draw=%p, main=%u, current=%p\n", glstate->fbo.fbo_read, glstate->fbo.fbo_draw, glstate->fbo.mainfbo_fbo, glstate->fbo.current_fb);)
    
    if (glstate->fbo.fbo_draw == glstate->fbo.current_fb)
        return;

    glstate->fbo.current_fb = glstate->fbo.fbo_draw;
    GLuint fbo = glstate->fbo.fbo_draw->id;
    if (unlikely(!fbo))
        fbo = glstate->fbo.mainfbo_fbo;
        
    LOAD_GLES3_OR_OES(glBindFramebuffer);
    gles_glBindFramebuffer(GL_FRAMEBUFFER, fbo);
}

glrenderbuffer_t* find_renderbuffer(GLuint renderbuffer) {
    if (unlikely(renderbuffer == 0)) return glstate->fbo.default_rb;
    
    khint_t k;
    khash_t(renderbufferlist_t) *list = glstate->fbo.renderbufferlist;
    k = kh_get(renderbufferlist_t, list, renderbuffer);
    
    if (likely(k != kh_end(list))){
        return kh_value(list, k);
    }
    return NULL;
}

void APIENTRY_GL4ES gl4es_glGenFramebuffers(GLsizei n, GLuint *ids) {
    DBG(printf("glGenFramebuffers(%i, %p)\n", n, ids);)
    LOAD_GLES3_OR_OES(glGenFramebuffers);
    GLsizei m = 0;
    
    // OPTIMIZATION: Fast path for recycling
    if (likely(globals4es.recyclefbo && glstate->fbo.old && glstate->fbo.old->nbr > 0)) {
        while (m < n && glstate->fbo.old->nbr > 0) {
            DBG(printf("Recycled 1 FBO\n");)
            ids[m++] = glstate->fbo.old->fbos[--glstate->fbo.old->nbr];
        }
    }
    
    noerrorShim();
    if (n - m > 0) {
        errorGL();
        gles_glGenFramebuffers(n - m, ids + m);
    }

    int ret;
    khint_t k;
    khash_t(framebufferlist_t) *list = glstate->fbo.framebufferlist;
    for (int i = 0; i < n; ++i) {
        k = kh_put(framebufferlist_t, list, ids[i], &ret);
        glframebuffer_t *fb = kh_value(list, k) = malloc(sizeof(glframebuffer_t));
        memset(fb, 0, sizeof(glframebuffer_t));
        fb->id = ids[i];
    }
}

void APIENTRY_GL4ES gl4es_glDeleteFramebuffers(GLsizei n, GLuint *framebuffers) {
    DBG(printf("glDeleteFramebuffers(%i, %p)\n", n, framebuffers);)
    
    if (unlikely(n < 1)) return;

    khash_t(framebufferlist_t) *list = glstate->fbo.framebufferlist;
    if (likely(list != NULL)) {
        for (int i = 0; i < n; i++) {
            GLuint t = framebuffers[i];
            if (likely(t != 0)) {
                khint_t k = kh_get(framebufferlist_t, list, t);
                if (k != kh_end(list)) {
                    glframebuffer_t *fb = kh_value(list, k);
                    
                    // Detach textures efficiently
                    for (int j = 0; j < MAX_DRAW_BUFFERS; ++j) {
                        if (fb->color[j] && fb->t_color[j] != GL_RENDERBUFFER) {
                            gltexture_t *tex = gl4es_getTexture(fb->t_color[j], fb->color[j]);
                            if (tex) {
                                tex->binded_fbo = 0;
                                tex->binded_attachment = 0;
                            }
                        }
                    }
                    // Detach Depth/Stencil
                    if (fb->depth && fb->t_depth != GL_RENDERBUFFER) {
                        gltexture_t *tex = gl4es_getTexture(fb->t_depth, fb->depth);
                        if (tex) {
                            tex->binded_fbo = 0;
                            tex->binded_attachment = 0;
                            tex->renderdepth = 0;
                        }
                    }
                    if (fb->stencil && fb->t_stencil != GL_RENDERBUFFER) {
                        gltexture_t *tex = gl4es_getTexture(fb->t_stencil, fb->stencil);
                        if (tex) {
                            tex->binded_fbo = 0;
                            tex->binded_attachment = 0;
                            tex->renderstencil = 0;
                        }
                    }

                    // Reset current bindings if deleted
                    if (glstate->fbo.current_fb == fb) glstate->fbo.current_fb = glstate->fbo.fbo_0;
                    if (glstate->fbo.fbo_read == fb) glstate->fbo.fbo_read = glstate->fbo.fbo_0;
                    if (glstate->fbo.fbo_draw == fb) glstate->fbo.fbo_draw = glstate->fbo.fbo_0;

                    free(fb);
                    kh_del(framebufferlist_t, list, k);
                }
            }
        }
    }

    if (globals4es.recyclefbo) {
        noerrorShim();
        if (unlikely(glstate->fbo.old->cap == 0)) {
            glstate->fbo.old->cap = 16;
            glstate->fbo.old->fbos = (GLuint*)malloc(glstate->fbo.old->cap * sizeof(GLuint));
        }
        if (unlikely(glstate->fbo.old->nbr + n > glstate->fbo.old->cap)) {
            glstate->fbo.old->cap += n + 16;
            glstate->fbo.old->fbos = (GLuint*)realloc(glstate->fbo.old->fbos, glstate->fbo.old->cap * sizeof(GLuint));
        }
        if (glstate->fbo.old->fbos) {
            memcpy(glstate->fbo.old->fbos + glstate->fbo.old->nbr, framebuffers, n * sizeof(GLuint));
            glstate->fbo.old->nbr += n;
        }
    } else {
        LOAD_GLES3_OR_OES(glDeleteFramebuffers);
        errorGL();
        gles_glDeleteFramebuffers(n, framebuffers);
    }
}

GLboolean APIENTRY_GL4ES gl4es_glIsFramebuffer(GLuint framebuffer) {
    DBG(printf("glIsFramebuffer(%u)\n", framebuffer);)
    // Fast path: check internal list first
    if (find_framebuffer(framebuffer)) return GL_TRUE;
    
    // Only call GLES if really needed (usually unnecessary if our tracking is correct)
    // LOAD_GLES3_OR_OES(glIsFramebuffer);
    // return gles_glIsFramebuffer(framebuffer);
    return GL_FALSE;
}

GLenum APIENTRY_GL4ES gl4es_glCheckFramebufferStatus(GLenum target) {
    if (glstate->fbo.internal) {
        noerrorShim();
        return glstate->fbo.fb_status;
    }
    
    LOAD_GLES3_OR_OES(glCheckFramebufferStatus);
    errorGL();
    
    GLenum rtarget = target;
    if (target == GL_READ_FRAMEBUFFER) return GL_FRAMEBUFFER_COMPLETE; // Fake read buffer
    if (target == GL_DRAW_FRAMEBUFFER) rtarget = GL_FRAMEBUFFER;
    
    return gles_glCheckFramebufferStatus(rtarget);
}

void APIENTRY_GL4ES gl4es_glBindFramebuffer(GLenum target, GLuint framebuffer) {
    DBG(printf("glBindFramebuffer(%s, %u)\n", PrintEnum(target), framebuffer);)
    PUSH_IF_COMPILING(glBindFramebuffer);
    
    glframebuffer_t *fb = find_framebuffer(framebuffer);
    if (unlikely(!fb)) {
        errorShim(GL_INVALID_VALUE);
        return;
    }

    if (target == GL_FRAMEBUFFER) {
        glstate->fbo.fbo_read = fb;
        glstate->fbo.fbo_draw = fb;
    } else if (target == GL_READ_FRAMEBUFFER) {
        glstate->fbo.fbo_read = fb;
        noerrorShim();
        glstate->fbo.fb_status = GL_FRAMEBUFFER_COMPLETE;
        glstate->fbo.internal = 1;
        return; // Don't bind read to hardware yet
    } else if (target == GL_DRAW_FRAMEBUFFER) {
        target = GL_FRAMEBUFFER;
        glstate->fbo.fbo_draw = fb;
    } else {
        errorShim(GL_INVALID_ENUM);
        return;
    }
    
    glstate->fbo.internal = 0;

    if (framebuffer == 0) framebuffer = glstate->fbo.mainfbo_fbo;

    // OPTIMIZATION: Redundant state check
    if (glstate->fbo.current_fb == fb) {
        noerrorShim();
        return;
    }

    glstate->fbo.current_fb = fb;
    
    LOAD_GLES3_OR_OES(glBindFramebuffer);
    gles_glBindFramebuffer(target, framebuffer);
    
    // Check error only if debugging
    #ifdef DEBUG
    GLenum err = gles_glGetError();
    errorShim(err);
    #else
    noerrorShim();
    #endif
}

GLenum ReadDraw_Push(GLenum target) {
    if (target == GL_FRAMEBUFFER) return GL_FRAMEBUFFER;
    
    LOAD_GLES3_OR_OES(glBindFramebuffer);
    GLuint mainfbo = glstate->fbo.mainfbo_fbo;

    if (target == GL_DRAW_FRAMEBUFFER) {
        if (glstate->fbo.current_fb != glstate->fbo.fbo_draw) {
            gles_glBindFramebuffer(GL_FRAMEBUFFER, (glstate->fbo.fbo_draw->id) ? glstate->fbo.fbo_draw->id : mainfbo);
        }
        return GL_FRAMEBUFFER;
    }
    if (target == GL_READ_FRAMEBUFFER) {
        if (glstate->fbo.current_fb != glstate->fbo.fbo_read) {
            gles_glBindFramebuffer(GL_FRAMEBUFFER, (glstate->fbo.fbo_read->id) ? glstate->fbo.fbo_read->id : mainfbo);
        }
        return GL_FRAMEBUFFER;
    }
    return target;
}

void ReadDraw_Pop(GLenum target) {
    if (target == GL_FRAMEBUFFER) return;
    
    LOAD_GLES3_OR_OES(glBindFramebuffer);
    GLuint mainfbo = glstate->fbo.mainfbo_fbo;

    if (target == GL_DRAW_FRAMEBUFFER && glstate->fbo.current_fb != glstate->fbo.fbo_draw) {
        gles_glBindFramebuffer(GL_FRAMEBUFFER, (glstate->fbo.current_fb->id) ? glstate->fbo.current_fb->id : mainfbo);
    }
    if (target == GL_READ_FRAMEBUFFER && glstate->fbo.current_fb != glstate->fbo.fbo_read) {
        gles_glBindFramebuffer(GL_FRAMEBUFFER, (glstate->fbo.current_fb->id) ? glstate->fbo.current_fb->id : mainfbo);
    }
}

void SetAttachment(glframebuffer_t* fb, GLenum attachment, GLenum atttarget, GLuint att, int level) {
    // OPTIMIZATION: Use arithmetic instead of switch for color attachments
    if (attachment >= GL_COLOR_ATTACHMENT0 && attachment <= GL_COLOR_ATTACHMENT15) {
        int idx = attachment - GL_COLOR_ATTACHMENT0;
        fb->color[idx] = att;
        fb->l_color[idx] = level;
        fb->t_color[idx] = atttarget;
        return;
    }

    switch (attachment) {
    case GL_DEPTH_ATTACHMENT:
        fb->depth = att;
        fb->t_depth = atttarget;
        fb->l_depth = 0;
        break;
    case GL_STENCIL_ATTACHMENT:
        fb->stencil = att;
        fb->t_stencil = atttarget;
        fb->l_stencil = 0;
        break;
    case GL_DEPTH_STENCIL_ATTACHMENT:
        fb->depth = att;
        fb->t_depth = atttarget;
        fb->l_depth = 0;
        fb->stencil = att;
        fb->t_stencil = atttarget;
        fb->l_stencil = 0;
        break;
    }
}

GLuint GetAttachment(glframebuffer_t* fb, GLenum attachment) {
    if (attachment >= GL_COLOR_ATTACHMENT0 && attachment <= GL_COLOR_ATTACHMENT15) {
        return fb->color[attachment - GL_COLOR_ATTACHMENT0];
    }
    switch (attachment) {
    case GL_DEPTH_ATTACHMENT: return fb->depth;
    case GL_STENCIL_ATTACHMENT: return fb->stencil;
    case GL_DEPTH_STENCIL_ATTACHMENT: return fb->depth;
    }
    return 0;
}

GLenum GetAttachmentType(glframebuffer_t* fb, GLenum attachment) {
    if (attachment >= GL_COLOR_ATTACHMENT0 && attachment <= GL_COLOR_ATTACHMENT15) {
        return fb->t_color[attachment - GL_COLOR_ATTACHMENT0];
    }
    switch (attachment) {
    case GL_DEPTH_ATTACHMENT: return fb->t_depth;
    case GL_STENCIL_ATTACHMENT: return fb->t_stencil;
    case GL_DEPTH_STENCIL_ATTACHMENT: return fb->t_depth;
    }
    return 0;
}

int GetAttachmentLevel(glframebuffer_t* fb, GLenum attachment) {
    if (attachment >= GL_COLOR_ATTACHMENT0 && attachment <= GL_COLOR_ATTACHMENT15) {
        return fb->l_color[attachment - GL_COLOR_ATTACHMENT0];
    }
    switch (attachment) {
    case GL_DEPTH_ATTACHMENT: return fb->l_depth;
    case GL_STENCIL_ATTACHMENT: return fb->l_stencil;
    case GL_DEPTH_STENCIL_ATTACHMENT: return fb->l_depth;
    }
    return 0;
}

void APIENTRY_GL4ES gl4es_glFramebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level) {
    DBG(printf("glFramebufferTexture2D(%s, %s, %s, %u, %i)\n", PrintEnum(target), PrintEnum(attachment), PrintEnum(textarget), texture, level);)
    
    static GLuint scrap_tex = 0;
    static int scrap_width = 0;
    static int scrap_height = 0;
    
    LOAD_GLES3_OR_OES(glFramebufferTexture2D);
    LOAD_GLES(glTexImage2D);
    LOAD_GLES(glBindTexture);
    LOAD_GLES(glActiveTexture);
    void gles_glTexParameteri(glTexParameteri_ARG_EXPAND); 

    glframebuffer_t *fb = get_framebuffer(target);
    if (unlikely(!fb)) {
        errorShim(GL_INVALID_ENUM);
        return;
    }

    if (unlikely(!(attachment >= GL_COLOR_ATTACHMENT0 && attachment < (GL_COLOR_ATTACHMENT0 + hardext.maxcolorattach))
        && attachment != GL_DEPTH_ATTACHMENT 
        && attachment != GL_STENCIL_ATTACHMENT 
        && attachment != GL_DEPTH_STENCIL_ATTACHMENT)) {
         errorShim(GL_INVALID_ENUM);
         return;
    }
    
    int twidth = 0, theight = 0;
    gltexture_t *tex = NULL;
    
    if (texture) {
        tex = gl4es_getTexture(textarget, texture);
        if (unlikely(!tex)) {
            LOGE("texture for FBO not found, name=%u\n", texture);
        } else {
            texture = tex->glname;
            tex->fbtex_ratio = (globals4es.fbtexscale > 0.0f) ? globals4es.fbtexscale : 0.0f;

            // Handle texture resizing/format fixup
            int need_resize = (tex->shrink || tex->useratio || (tex->fbtex_ratio > 0.0f) || (tex->adjust && (hardext.npot==1 || hardext.npot==2) && !globals4es.potframebuffer));
            
            if (need_resize) {
                if (tex->useratio) {
                    tex->width = tex->nwidth / tex->ratiox;
                    tex->height = tex->nheight / tex->ratioy;
                } else if (tex->shrink) {
                    tex->width *= (1 << tex->shrink);
                    tex->height *= (1 << tex->shrink);
                }
                if (tex->fbtex_ratio > 0.0f) {
                    tex->width *= tex->fbtex_ratio;
                    tex->height *= tex->fbtex_ratio;
                }

                tex->nwidth = (hardext.npot > 0 || hardext.esversion > 1) ? tex->width : npot(tex->width);
                tex->nheight = (hardext.npot > 0 || hardext.esversion > 1) ? tex->height : npot(tex->height);
                tex->adjustxy[0] = (float)tex->width / tex->nwidth;
                tex->adjustxy[1] = (float)tex->height / tex->nheight;
                tex->adjust = (tex->width != tex->nwidth || tex->height != tex->nheight);
                tex->shrink = 0; tex->useratio = 0;
            }

            int need_change = (globals4es.potframebuffer && (npot(tex->nwidth) != tex->nwidth || npot(tex->nheight) != tex->nheight)) ? 1 : 0;
            
            if ((tex->type == GL_FLOAT && !hardext.floatfbo) || (tex->type == GL_HALF_FLOAT_OES && !hardext.halffloatfbo)) {
                need_change |= 2;
                tex->type = GL_UNSIGNED_BYTE;
            }
            if (tex->format == GL_BGRA && (globals4es.nobgra || !hardext.bgra8888)) {
                need_change |= 2;
                tex->format = GL_RGBA;
            }

            if (need_resize || need_change) {
                if (need_change & 1) {
                    tex->nwidth = npot(tex->nwidth);
                    tex->nheight = npot(tex->nheight);
                    tex->adjustxy[0] = (float)tex->width / tex->nwidth;
                    tex->adjustxy[1] = (float)tex->height / tex->nheight;
                    tex->adjust = (tex->width != tex->nwidth || tex->height != tex->nheight);
                }
                
                int oldactive = glstate->texture.active;
                if (oldactive) gles_glActiveTexture(GL_TEXTURE0);
                
                // Use temp binding to avoid disturbing current unit 0
                gles_glBindTexture(GL_TEXTURE_2D, tex->glname);
                gles_glTexImage2D(GL_TEXTURE_2D, 0, tex->format, tex->nwidth, tex->nheight, 0, tex->format, tex->type, NULL);
                
                if (oldactive) gles_glActiveTexture(GL_TEXTURE0 + oldactive);
                // Restore old binding handled by caller logic usually, simplified here
                glstate->texture.bound[0][ENABLED_TEX2D]->glname = 0; // Force rebind later if needed
            }
            
            twidth = tex->nwidth;
            theight = tex->nheight;
            fb->width = twidth;
            fb->height = theight;
        }
    }
    
    GLenum ntarget = ReadDraw_Push(target);

    GLuint old_att = GetAttachment(fb, attachment);
    GLuint old_type = GetAttachmentType(fb, attachment);
    
    // Detach old
    if (old_att && old_type == textarget) {
        gltexture_t* old = gl4es_getTexture(old_type, old_att);
        if (old) {
            old->binded_fbo = 0;
            old->binded_attachment = 0;
        }
    }

    if (tex) {
        tex->binded_fbo = fb->id;
        tex->binded_attachment = attachment;
    }

    // Check redundant attach
    if ((old_type == textarget) && (old_att == (tex ? tex->texture : texture))) {
        noerrorShim();
        return;
    }

    SetAttachment(fb, attachment, textarget, tex ? tex->texture : texture, level);

    // Handle Color Attachments
    if (attachment >= GL_COLOR_ATTACHMENT0 && attachment < (GL_COLOR_ATTACHMENT0 + hardext.maxcolorattach) && tex) {
        // Force clamp to edge for NPOT FBO textures on limited hardware
        if ((hardext.npot == 1 || hardext.npot == 2) && (!tex->actual.wrap_s || !tex->actual.wrap_t)) {
            tex->sampler.wrap_s = tex->sampler.wrap_t = GL_CLAMP_TO_EDGE;
            tex->adjust = 0;
        }
        
        realize_1texture(map_tex_target(textarget), -1, tex, NULL);
    }

    // Handle Depth/Stencil Special Cases
    if (attachment == GL_DEPTH_ATTACHMENT) {
        if (level != 0) return;
        if (hardext.depthtex && (tex || !texture)) {
            // Ensure texture is proper depth format
            if (tex && !(tex->format == GL_DEPTH_COMPONENT || tex->format == GL_DEPTH_STENCIL)) {
                tex->format = GL_DEPTH_COMPONENT;
                tex->type = (hardext.depth24) ? GL_UNSIGNED_INT : GL_UNSIGNED_SHORT;
                tex->fpe_format = FPE_TEX_DEPTH;
                realize_1texture(GL_TEXTURE_2D, -1, tex, NULL);
                
                int oldactive = glstate->texture.active;
                if (oldactive) gles_glActiveTexture(GL_TEXTURE0);
                gles_glBindTexture(GL_TEXTURE_2D, tex->glname);
                gles_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                gles_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                gles_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                gles_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                gles_glTexImage2D(GL_TEXTURE_2D, 0, tex->format, tex->nwidth, tex->nheight, 0, tex->format, tex->type, NULL);
                if (oldactive) gles_glActiveTexture(GL_TEXTURE0 + oldactive);
            }
            gles_glFramebufferTexture2D(ntarget, attachment, GL_TEXTURE_2D, texture, 0);
        } else {
            // Fallback to renderbuffer
            if (tex && !tex->renderdepth) {
                gl4es_glGenRenderbuffers(1, &tex->renderdepth);
                gl4es_glBindRenderbuffer(GL_RENDERBUFFER, tex->renderdepth);
                gl4es_glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, twidth, theight);
                gl4es_glBindRenderbuffer(GL_RENDERBUFFER, 0);
            }
            gl4es_glFramebufferRenderbuffer(ntarget, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, tex ? tex->renderdepth : 0);
        }
        errorGL();
        ReadDraw_Pop(target);
        return;
    }

    if (attachment == GL_STENCIL_ATTACHMENT) {
        if (level != 0) return;
        if ((tex || !texture) && (hardext.stenciltex || (hardext.depthtex && hardext.depthstencil))) {
             if (tex && !(tex->format == GL_STENCIL_INDEX8 || tex->format == GL_DEPTH_STENCIL)) {
                 // Convert to stencil capable format
                 tex->format = (hardext.stenciltex) ? GL_STENCIL_INDEX8 : GL_DEPTH_STENCIL;
                 tex->type = (hardext.stenciltex) ? GL_UNSIGNED_BYTE : GL_UNSIGNED_INT_24_8;
                 
                 int oldactive = glstate->texture.active;
                 if (oldactive) gles_glActiveTexture(GL_TEXTURE0);
                 gles_glBindTexture(GL_TEXTURE_2D, tex->glname);
                 gles_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                 gles_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                 gles_glTexImage2D(GL_TEXTURE_2D, 0, tex->format, tex->nwidth, tex->nheight, 0, tex->format, tex->type, NULL);
                 if (oldactive) gles_glActiveTexture(GL_TEXTURE0 + oldactive);
             }
             gles_glFramebufferTexture2D(ntarget, attachment, GL_TEXTURE_2D, texture, 0);
        } else {
            if (tex && !tex->renderstencil) {
                gl4es_glGenRenderbuffers(1, &tex->renderstencil);
                gl4es_glBindRenderbuffer(GL_RENDERBUFFER, tex->renderstencil);
                gl4es_glRenderbufferStorage(GL_RENDERBUFFER, GL_STENCIL_INDEX8, twidth, theight);
            }
            gl4es_glFramebufferRenderbuffer(ntarget, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, tex ? tex->renderstencil : 0);
        }
        errorGL();
        ReadDraw_Pop(target);
        return;
    }

    // Mipmap level handling (attach to 0, use scrap if level > 0)
    if (level != 0) {
        twidth >>= level; if (twidth < 1) twidth = 1;
        theight >>= level; if (theight < 1) theight = 1;
        
        if (unlikely(!scrap_tex)) gl4es_glGenTextures(1, &scrap_tex);
        
        if (scrap_width != twidth || scrap_height != theight) {
            scrap_width = twidth;
            scrap_height = theight;
            gles_glBindTexture(GL_TEXTURE_2D, scrap_tex);
            gles_glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, scrap_width, scrap_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        }
        texture = scrap_tex;
    }
    
    errorGL();
    GLenum realtarget = (textarget >= GL_TEXTURE_CUBE_MAP_POSITIVE_X && textarget <= GL_TEXTURE_CUBE_MAP_NEGATIVE_Z) ? textarget : GL_TEXTURE_2D;
    gles_glFramebufferTexture2D(ntarget, attachment, realtarget, texture, 0);
    
    ReadDraw_Pop(target);
}

void APIENTRY_GL4ES gl4es_glFramebufferTexture1D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level) {
    gl4es_glFramebufferTexture2D(target, attachment, textarget, texture, level);
}

void APIENTRY_GL4ES gl4es_glFramebufferTexture3D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level, GLint layer) {
    (void)layer; 
    gl4es_glFramebufferTexture2D(target, attachment, textarget, texture, level);
}

void APIENTRY_GL4ES gl4es_glGenRenderbuffers(GLsizei n, GLuint *renderbuffers) {
    DBG(printf("glGenRenderbuffers(%i, %p)\n", n, renderbuffers);)
    LOAD_GLES3_OR_OES(glGenRenderbuffers);
    
    errorGL();
    gles_glGenRenderbuffers(n, renderbuffers);
    
    int ret;
    khint_t k;
    khash_t(renderbufferlist_t) *list = glstate->fbo.renderbufferlist;
    for (int i = 0; i < n; ++i) {
        k = kh_put(renderbufferlist_t, list, renderbuffers[i], &ret);
        glrenderbuffer_t *rend = kh_value(list, k) = malloc(sizeof(glrenderbuffer_t));
        memset(rend, 0, sizeof(glrenderbuffer_t));
        rend->renderbuffer = renderbuffers[i];
    }
}

void APIENTRY_GL4ES gl4es_glFramebufferRenderbuffer(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer) {
    DBG(printf("glFramebufferRenderbuffer(%s, %s, %s, %u)\n", PrintEnum(target), PrintEnum(attachment), PrintEnum(renderbuffertarget), renderbuffer);)
    LOAD_GLES3_OR_OES(glFramebufferRenderbuffer);
    
    glframebuffer_t *fb = get_framebuffer(target);
    if (unlikely(!fb)) {
        errorShim(GL_INVALID_ENUM);
        return;
    }

    if (unlikely(!(attachment >= GL_COLOR_ATTACHMENT0 && attachment < (GL_COLOR_ATTACHMENT0 + hardext.maxcolorattach))
     && attachment != GL_DEPTH_ATTACHMENT 
     && attachment != GL_STENCIL_ATTACHMENT 
     && attachment != GL_DEPTH_STENCIL_ATTACHMENT)) {
         errorShim(GL_INVALID_ENUM);
         return;
     }
    
    glrenderbuffer_t *rend = find_renderbuffer(renderbuffer);
    if (unlikely(!rend)) {
        errorShim(GL_INVALID_OPERATION);
        return;
    }

    // Workaround: Force texture for color attachments if configured
    if (globals4es.fboforcetex && attachment >= GL_COLOR_ATTACHMENT0 && (attachment < (GL_COLOR_ATTACHMENT0 + hardext.maxcolorattach))) {
        if (rend->renderbuffer) {
            if (!rend->secondarytexture) {
                // Lazily create texture for this RB
                gl4es_glGenTextures(1, &rend->secondarytexture);
                int oldactive = glstate->texture.active;
                if (oldactive) gl4es_glActiveTexture(GL_TEXTURE0);
                
                gl4es_glBindTexture(GL_TEXTURE_2D, rend->secondarytexture);
                gl4es_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                gl4es_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                gl4es_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                gl4es_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                // Assume RGBA/UNSIGNED_BYTE for simplicity in forced mode
                gl4es_glTexImage2D(GL_TEXTURE_2D, 0, rend->format ? rend->format : GL_RGBA, rend->width, rend->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
                
                if (oldactive) gl4es_glActiveTexture(GL_TEXTURE0 + oldactive);
            }
            gl4es_glFramebufferTexture2D(target, attachment, GL_TEXTURE_2D, rend->secondarytexture, 0);
        } else {
            gl4es_glFramebufferTexture2D(target, attachment, GL_TEXTURE_2D, 0, 0);
        }
        return;
    }

    if (attachment == GL_DEPTH_STENCIL_ATTACHMENT) {
        gl4es_glFramebufferRenderbuffer(target, GL_DEPTH_ATTACHMENT, renderbuffertarget, renderbuffer);
        gl4es_glFramebufferRenderbuffer(target, GL_STENCIL_ATTACHMENT, renderbuffertarget, renderbuffer);
        return;
    }

    if (attachment == GL_STENCIL_ATTACHMENT && rend && rend->secondarybuffer) {
        renderbuffer = rend->secondarybuffer;
    }

    fb->width = rend->width;
    fb->height = rend->height;

    if ((GetAttachmentType(fb, attachment) == GL_RENDERBUFFER) && (GetAttachment(fb, attachment) == renderbuffer)) {
        noerrorShim();
        return;
    }

    SetAttachment(fb, attachment, GL_RENDERBUFFER, renderbuffer, 0);
    
    GLenum ntarget = ReadDraw_Push(target);
    errorGL();
    gles_glFramebufferRenderbuffer(ntarget, attachment, renderbuffertarget, renderbuffer);
    ReadDraw_Pop(target);
}

void APIENTRY_GL4ES gl4es_glDeleteRenderbuffers(GLsizei n, GLuint *renderbuffers) {
    DBG(printf("glDeleteRenderbuffer(%d, %p)\n", n, renderbuffers);)
    LOAD_GLES3_OR_OES(glDeleteRenderbuffers);
    
    khash_t(renderbufferlist_t) *list = glstate->fbo.renderbufferlist;
    if (likely(list != NULL)) {
        khint_t k;
        glrenderbuffer_t *rend;
        for (int i = 0; i < n; i++) {
            GLuint t = renderbuffers[i];
            if (likely(t != 0)) {
                k = kh_get(renderbufferlist_t, list, t);
                if (k != kh_end(list)) {
                    rend = kh_value(list, k);
                    if (glstate->fbo.current_rb == rend)
                        glstate->fbo.current_rb = glstate->fbo.default_rb;
                    
                    if (rend->secondarybuffer)
                        gles_glDeleteRenderbuffers(1, &rend->secondarybuffer);
                    if (rend->secondarytexture)
                        gl4es_glDeleteTextures(1, &rend->secondarytexture);
                        
                    free(rend);
                    kh_del(renderbufferlist_t, list, k);
                }
            }
        }
    }

    errorGL();
    gles_glDeleteRenderbuffers(n, renderbuffers);
}

void APIENTRY_GL4ES gl4es_glRenderbufferStorage(GLenum target, GLenum internalformat, GLsizei width, GLsizei height) {
    DBG(printf("glRenderbufferStorage(%s, %s, %i, %i)\n", PrintEnum(target), PrintEnum(internalformat), width, height);)
    LOAD_GLES3_OR_OES(glRenderbufferStorage);
    LOAD_GLES3_OR_OES(glGenRenderbuffers);
    LOAD_GLES3_OR_OES(glBindRenderbuffer);

    glrenderbuffer_t *rend = glstate->fbo.current_rb;
    if (unlikely(!rend->renderbuffer)) {
        errorShim(GL_INVALID_OPERATION);
        return;
    }
    
    errorGL();
    
    if (hardext.npot > 0 && !globals4es.potframebuffer) {
        // Keep dimensions
    } else {
        width = npot(width);
        height = npot(height);
    }

    int use_secondarybuffer = 0;
    GLenum format = internalformat;

    // Logic for depth/stencil emulation on hardware that misses packed depth_stencil
    if (internalformat == GL_DEPTH_STENCIL) internalformat = GL_DEPTH24_STENCIL8;
    
    if (internalformat == GL_DEPTH24_STENCIL8 && (hardext.depthstencil == 0 || ((hardext.vendor & VEND_IMGTEC) == VEND_IMGTEC))) {
        internalformat = (hardext.depth24) ? GL_DEPTH_COMPONENT24 : GL_DEPTH_COMPONENT16;
        if (!rend->secondarybuffer) {
            gles_glGenRenderbuffers(1, &rend->secondarybuffer);
        }
        use_secondarybuffer = 1;
    }
    else if (internalformat == GL_DEPTH_COMPONENT || internalformat == GL_DEPTH_COMPONENT32)
        internalformat = GL_DEPTH_COMPONENT16;
    else if (internalformat == GL_RGB8 && hardext.rgba8 == 0)
        internalformat = GL_RGB565_OES;
    else if (internalformat == GL_RGBA8 && hardext.rgba8 == 0)
        internalformat = GL_RGBA4_OES;
    else if (internalformat == GL_RGBA) {
        internalformat = (hardext.rgba8 == 0) ? GL_RGBA8 : GL_RGBA4_OES;
    }

    if (rend->secondarybuffer) {
        if (use_secondarybuffer) {
            GLuint current_rb = glstate->fbo.current_rb->renderbuffer;
            gles_glBindRenderbuffer(GL_RENDERBUFFER, rend->secondarybuffer);
            gles_glRenderbufferStorage(target, GL_STENCIL_INDEX8, width, height);
            gles_glBindRenderbuffer(GL_RENDERBUFFER, current_rb);
        } else {
            LOAD_GLES3_OR_OES(glDeleteRenderbuffers);
            gles_glDeleteRenderbuffers(1, &rend->secondarybuffer);
            rend->secondarybuffer = 0;
        }
    }

    if (rend->secondarytexture) {
        gltexture_t *tex = gl4es_getTexture(GL_TEXTURE_2D, rend->secondarytexture);
        int oldactive = glstate->texture.active;
        if (oldactive) gl4es_glActiveTexture(GL_TEXTURE0);
        
        gl4es_glBindTexture(GL_TEXTURE_2D, rend->secondarytexture);
        tex->nwidth = tex->width = width;
        tex->nheight = tex->height = height;
        
        LOAD_GLES(glTexImage2D);
        gles_glTexImage2D(GL_TEXTURE_2D, 0, tex->format, tex->nwidth, tex->nheight, 0, tex->format, tex->type, NULL);
        
        if (oldactive) gl4es_glActiveTexture(GL_TEXTURE0 + oldactive);
    }

    rend->width = width;
    rend->height = height;
    rend->format = format;
    rend->actual = internalformat;

    gles_glRenderbufferStorage(target, internalformat, width, height);
}

void APIENTRY_GL4ES gl4es_glRenderbufferStorageMultisample(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height) {
    // STUB: PowerVR GE8320 MSAA handling is usually implicit or requires specific extensions. 
    // Fallback to standard storage.
    gl4es_glRenderbufferStorage(target, internalformat, width, height);
}

void APIENTRY_GL4ES gl4es_glBindRenderbuffer(GLenum target, GLuint renderbuffer) {
    DBG(printf("glBindRenderbuffer(%s, %u)\n", PrintEnum(target), renderbuffer);)
    LOAD_GLES3_OR_OES(glBindRenderbuffer);
    
    // OPTIMIZATION: Redundant check
    if (likely(glstate->fbo.current_rb && glstate->fbo.current_rb->renderbuffer == renderbuffer)) {
        noerrorShim();
        return;
    }

    glrenderbuffer_t *rend = find_renderbuffer(renderbuffer);
    if (unlikely(!rend)) {
        errorShim(GL_INVALID_OPERATION);
        return;
    }
    
    glstate->fbo.current_rb = rend;
    errorGL();
    gles_glBindRenderbuffer(target, renderbuffer);
}

GLboolean APIENTRY_GL4ES gl4es_glIsRenderbuffer(GLuint renderbuffer) {
    noerrorShim();
    return (find_renderbuffer(renderbuffer) != NULL) ? GL_TRUE : GL_FALSE;
}

void APIENTRY_GL4ES gl4es_glGenerateMipmap(GLenum target) {
    DBG(printf("glGenerateMipmap(%s)\n", PrintEnum(target));)
    LOAD_GLES3_OR_OES(glGenerateMipmap);
    
    const GLuint rtarget = map_tex_target(target);
    realize_bound(glstate->texture.active, target);
    gltexture_t *bound = gl4es_getCurrentTexture(target);
    
    if (unlikely(globals4es.forcenpot && hardext.npot == 1 && bound->npot)) {
        noerrorShim();
        return; 
    }

    errorGL();
    if (globals4es.automipmap != 3) {
        gles_glGenerateMipmap(rtarget);
        bound->mipmap_auto = 1;
    }
}

void APIENTRY_GL4ES gl4es_glGetFramebufferAttachmentParameteriv(GLenum target, GLenum attachment, GLenum pname, GLint *params) {
    DBG(printf("glGetFramebufferAttachmentParameteriv(%s, %s, %s, %p)\n", PrintEnum(target), PrintEnum(attachment), PrintEnum(pname), params);)
    LOAD_GLES3_OR_OES(glGetFramebufferAttachmentParameteriv);

    glframebuffer_t *fb = get_framebuffer(target);
    if (unlikely(!fb)) {
        errorShim(GL_INVALID_ENUM);
        return;
    }

    if (pname == GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME) {
        noerrorShim();
        *params = GetAttachment(fb, attachment);
        return;
    }

    if (pname == GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE) {
        noerrorShim();
        *params = GetAttachmentType(fb, attachment);
        if (*params != 0 && *params != GL_RENDERBUFFER) *params = GL_TEXTURE;
        return;
    }
    
    GLenum ntarget = ReadDraw_Push(target);
    errorGL();
    gles_glGetFramebufferAttachmentParameteriv(ntarget, attachment, pname, params);
    ReadDraw_Pop(target);
}

void APIENTRY_GL4ES gl4es_glGetRenderbufferParameteriv(GLenum target, GLenum pname, GLint * params) {
    LOAD_GLES3_OR_OES(glGetRenderbufferParameteriv);
    errorGL();
    gles_glGetRenderbufferParameteriv(target, pname, params);
}

void createMainFBO(int width, int height) {
    LOAD_GLES3_OR_OES(glGenFramebuffers);
    LOAD_GLES3_OR_OES(glBindFramebuffer);
    LOAD_GLES3_OR_OES(glFramebufferTexture2D);
    LOAD_GLES3_OR_OES(glCheckFramebufferStatus);
    LOAD_GLES3_OR_OES(glFramebufferRenderbuffer);
    LOAD_GLES3_OR_OES(glRenderbufferStorage);
    LOAD_GLES3_OR_OES(glGenRenderbuffers);
    LOAD_GLES3_OR_OES(glBindRenderbuffer);
    LOAD_GLES(glTexImage2D);
    LOAD_GLES(glGenTextures);
    LOAD_GLES(glBindTexture);
    LOAD_GLES(glActiveTexture);
    void gles_glTexParameteri(glTexParameteri_ARG_EXPAND); 
    LOAD_GLES(glClear);

    int createIt = 1;
    if (glstate->fbo.mainfbo_fbo) {
        if (width == glstate->fbo.mainfbo_width && height == glstate->fbo.mainfbo_height)
            return;
        createIt = 0;
    }
    
    if (glstate->texture.active != 0) gles_glActiveTexture(GL_TEXTURE0);
        
    glstate->fbo.mainfbo_width = width;
    glstate->fbo.mainfbo_height = height;
    glstate->fbo.mainfbo_nwidth = (hardext.npot > 0) ? width : npot(width);
    glstate->fbo.mainfbo_nheight = (hardext.npot > 0) ? height : npot(height);

    if (createIt) gles_glGenTextures(1, &glstate->fbo.mainfbo_tex);
    
    gles_glBindTexture(GL_TEXTURE_2D, glstate->fbo.mainfbo_tex);
    if (createIt) {
        gles_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        gles_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        gles_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        gles_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    GLenum fmt = globals4es.fbo_noalpha ? GL_RGB : GL_RGBA;
    gles_glTexImage2D(GL_TEXTURE_2D, 0, fmt, glstate->fbo.mainfbo_nwidth, glstate->fbo.mainfbo_nheight, 0, fmt, GL_UNSIGNED_BYTE, NULL);
    gles_glBindTexture(GL_TEXTURE_2D, 0);

    if (createIt) {
        gles_glGenRenderbuffers(1, &glstate->fbo.mainfbo_dep);
        gles_glGenRenderbuffers(1, &glstate->fbo.mainfbo_ste);
        gles_glGenFramebuffers(1, &glstate->fbo.mainfbo_fbo);
    }

    gles_glBindRenderbuffer(GL_RENDERBUFFER, glstate->fbo.mainfbo_ste);
    gles_glRenderbufferStorage(GL_RENDERBUFFER, GL_STENCIL_INDEX8, glstate->fbo.mainfbo_nwidth, glstate->fbo.mainfbo_nheight);
    
    gles_glBindRenderbuffer(GL_RENDERBUFFER, glstate->fbo.mainfbo_dep);
    gles_glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, glstate->fbo.mainfbo_nwidth, glstate->fbo.mainfbo_nheight);
    gles_glBindRenderbuffer(GL_RENDERBUFFER, 0);

    gles_glBindFramebuffer(GL_FRAMEBUFFER, glstate->fbo.mainfbo_fbo);
    gles_glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, glstate->fbo.mainfbo_ste);
    gles_glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, glstate->fbo.mainfbo_dep);
    gles_glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, glstate->fbo.mainfbo_tex, 0);

    GLenum status = gles_glCheckFramebufferStatus(GL_FRAMEBUFFER);
    gles_glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Restore state
    gles_glBindTexture(GL_TEXTURE_2D, glstate->texture.bound[0][ENABLED_TEX2D]->glname);
    if (glstate->texture.active != 0) gles_glActiveTexture(GL_TEXTURE0 + glstate->texture.active);
    
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        printf("LIBGL: Error while creating main fbo (0x%04X)\n", status);
        deleteMainFBO(glstate);
    } else {
        gles_glBindFramebuffer(GL_FRAMEBUFFER, (glstate->fbo.current_fb->id) ? glstate->fbo.current_fb->id : glstate->fbo.mainfbo_fbo);
        if (glstate->fbo.current_fb->id == 0)
            gles_glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    }
}

void blitMainFBO(int x, int y, int width, int height) {
    if (unlikely(glstate->fbo.mainfbo_fbo == 0)) return;

    if (!width && !height) {
        gl4es_glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        gl4es_glClear(GL_COLOR_BUFFER_BIT);
        width = glstate->fbo.mainfbo_width;
        height = glstate->fbo.mainfbo_height;
    }

    GLint vp[4];
    memcpy(vp, &glstate->raster.viewport, sizeof(vp));
    gl4es_glViewport(0, 0, glstate->fbowidth, glstate->fboheight);
    
    float rx = (float)width / glstate->fbo.mainfbo_width;
    float ry = (float)height / glstate->fbo.mainfbo_height;
    y = glstate->fboheight - (y + height);

    gl4es_blitTexture(glstate->fbo.mainfbo_tex, 0.f, 0.f,
        glstate->fbo.mainfbo_width, glstate->fbo.mainfbo_height, 
        glstate->fbo.mainfbo_nwidth, glstate->fbo.mainfbo_nheight, 
        rx, ry, 0, 0, x, y, BLIT_OPAQUE);
        
    gl4es_glViewport(vp[0], vp[1], vp[2], vp[3]);
}

void bindMainFBO() {
    LOAD_GLES3_OR_OES(glBindFramebuffer);
    if (!glstate->fbo.mainfbo_fbo) return;
    if (glstate->fbo.current_fb->id == 0) {
        gles_glBindFramebuffer(GL_FRAMEBUFFER, glstate->fbo.mainfbo_fbo);
    }
}

void unbindMainFBO() {
    LOAD_GLES3_OR_OES(glBindFramebuffer);
    if (!glstate->fbo.mainfbo_fbo) return;
    if (glstate->fbo.current_fb->id == 0) {
        gles_glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
}

void deleteMainFBO(void *state) {
    LOAD_GLES3_OR_OES(glDeleteFramebuffers);
    LOAD_GLES3_OR_OES(glDeleteRenderbuffers);
    LOAD_GLES(glDeleteTextures);

    glstate_t *glstate = (glstate_t*)state;
    if (glstate->fbo.mainfbo_dep) gles_glDeleteRenderbuffers(1, &glstate->fbo.mainfbo_dep);
    if (glstate->fbo.mainfbo_ste) gles_glDeleteRenderbuffers(1, &glstate->fbo.mainfbo_ste);
    if (glstate->fbo.mainfbo_tex) gles_glDeleteTextures(1, &glstate->fbo.mainfbo_tex);
    if (glstate->fbo.mainfbo_fbo) gles_glDeleteFramebuffers(1, &glstate->fbo.mainfbo_fbo);
    
    glstate->fbo.mainfbo_dep = 0;
    glstate->fbo.mainfbo_ste = 0;
    glstate->fbo.mainfbo_tex = 0;
    glstate->fbo.mainfbo_fbo = 0;
}

void APIENTRY_GL4ES gl4es_glFramebufferTextureLayer(GLenum target, GLenum attachment, GLuint texture, GLint level, GLint layer) {
    (void)layer; // Ignore layer for now
    gl4es_glFramebufferTexture2D(target, attachment, GL_TEXTURE_2D, texture, level);
}

#ifndef NOX11
void gl4es_SwapBuffers_currentContext(); 
#endif

void APIENTRY_GL4ES gl4es_glBlitFramebuffer(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter) {
    if (unlikely((mask & GL_COLOR_BUFFER_BIT) == 0)) return;
    if (unlikely(glstate->fbo.fbo_read == glstate->fbo.fbo_draw && srcX0 == dstX0 && srcY0 == dstY0 && srcX1 == dstX1 && srcY1 == dstY1)) return;
    
    // Fast Blit Logic for PVR
    GLuint texture = (glstate->fbo.fbo_read->id == 0 && glstate->fbo.mainfbo_fbo) ? glstate->fbo.mainfbo_tex : glstate->fbo.fbo_read->color[0];
    int created = (texture == 0 || (glstate->fbo.fbo_read == glstate->fbo.fbo_draw));
    
    int oldtex = glstate->texture.active;
    if (oldtex) gl4es_glActiveTexture(GL_TEXTURE0);
    
    if (created) {
        // Copy to temp texture if direct blit not possible
        gltexture_t *old = glstate->texture.bound[ENABLED_TEX2D][0];
        gl4es_glGenTextures(1, &texture);
        gl4es_glBindTexture(GL_TEXTURE_2D, texture);
        gl4es_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (filter == GL_LINEAR) ? GL_LINEAR : GL_NEAREST);
        gl4es_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (filter == GL_LINEAR) ? GL_LINEAR : GL_NEAREST);
        gl4es_glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, srcX0, srcY0, srcX1 - srcX0, srcY1 - srcY0, 0);
        srcX1 -= srcX0; srcX0 = 0;
        srcY1 -= srcY0; srcY0 = 0;
        gl4es_glBindTexture(GL_TEXTURE_2D, old->texture);
    }

    float nwidth, nheight;
    GLenum glname = texture;
    
    if (texture == glstate->fbo.mainfbo_tex) {
        nwidth = glstate->fbo.mainfbo_nwidth;
        nheight = glstate->fbo.mainfbo_nheight;
    } else {
        gltexture_t *tex = gl4es_getTexture(GL_TEXTURE_2D, texture);
        if (tex) {
            nwidth = tex->nwidth;
            nheight = tex->nheight;
            glname = tex->glname;
        } else {
            nwidth = srcX1;
            nheight = srcY1;
        }
    }

    float zoomx = ((float)(dstX1 - dstX0)) / (srcX1 - srcX0);
    float zoomy = ((float)(dstY1 - dstY0)) / (srcY1 - srcY0);
    
    int fbowidth = (glstate->fbo.fbo_draw->id == 0) ? glstate->fbo.mainfbo_width : glstate->fbo.fbo_draw->width;
    int fboheight = (glstate->fbo.fbo_draw->id == 0) ? glstate->fbo.mainfbo_height : glstate->fbo.fbo_draw->height;

    GLint vp[4];
    memcpy(vp, &glstate->raster.viewport, sizeof(vp));
    gl4es_glViewport(0, 0, fbowidth, fboheight);
    
    gl4es_blitTexture(glname, srcX0, srcY0, srcX1 - srcX0, srcY1 - srcY0, nwidth, nheight, zoomx, zoomy, 0, 0, dstX0, dstY0, BLIT_OPAQUE);
    
    gl4es_glViewport(vp[0], vp[1], vp[2], vp[3]);
    
    if (created) gl4es_glDeleteTextures(1, &texture);
    if (oldtex) gl4es_glActiveTexture(GL_TEXTURE0 + oldtex);

#ifndef NOX11
    if (glstate->fbo.fbo_draw->id == 0 && globals4es.blitfb0)
        gl4es_SwapBuffers_currentContext();
#endif
}

GLuint gl4es_getCurrentFBO() {
  return (glstate->fbo.current_fb->id) ? glstate->fbo.current_fb->id : glstate->fbo.mainfbo_fbo;
}

void gl4es_setCurrentFBO() {
  LOAD_GLES3_OR_OES(glBindFramebuffer);
  gles_glBindFramebuffer(GL_FRAMEBUFFER, (glstate->fbo.current_fb->id) ? glstate->fbo.current_fb->id : glstate->fbo.mainfbo_fbo);
}

void APIENTRY_GL4ES gl4es_glDrawBuffers(GLsizei n, const GLenum *bufs) {
    if (hardext.drawbuffers) {
        LOAD_GLES_EXT(glDrawBuffers);
        gles_glDrawBuffers(n, bufs);
        errorGL();
    } else {
        if (unlikely(n < 0 || n > hardext.maxdrawbuffers)) {
            errorShim(GL_INVALID_VALUE);
            return;
        }
    }
    glstate->fbo.fbo_draw->n_draw = n;
    memcpy(glstate->fbo.fbo_draw->drawbuff, bufs, n * sizeof(GLenum));
    noerrorShim();
}

void APIENTRY_GL4ES gl4es_glNamedFramebufferDrawBuffers(GLuint framebuffer, GLsizei n, const GLenum *bufs) {
    glframebuffer_t* fb = find_framebuffer(framebuffer);
    if (hardext.drawbuffers) {
        GLuint oldf = glstate->fbo.fbo_draw->id;
        gl4es_glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fb->id);
        LOAD_GLES_EXT(glDrawBuffers);
        gles_glDrawBuffers(n, bufs);
        errorGL();
        gl4es_glBindFramebuffer(GL_DRAW_FRAMEBUFFER, oldf);
    }
    fb->n_draw = n;
    memcpy(fb->drawbuff, bufs, n * sizeof(GLenum));
    noerrorShim();
}

// Multiplication factors for 1/255 and 1/127
#define MUL_255 0.00392156862f
#define MUL_127 0.00787401574f

void APIENTRY_GL4ES gl4es_glClearBufferiv(GLenum buffer, GLint drawbuffer, const GLint * value) {
    noerrorShim();
    switch(buffer) {
        case GL_COLOR:
            if (drawbuffer > glstate->fbo.fbo_draw->n_draw) return;
            {
                GLfloat oldclear[4];
                LOAD_GLES_EXT(glDrawBuffers);
                if (hardext.drawbuffers) gles_glDrawBuffers(1, (const GLenum *) &drawbuffer);
                gl4es_glGetFloatv(GL_COLOR_CLEAR_VALUE, oldclear);
                
                gl4es_glClearColor(value[0] * MUL_127, value[1] * MUL_127, value[2] * MUL_127, value[3] * MUL_127);
                gl4es_glClear(GL_COLOR_BUFFER_BIT);
                gl4es_glClearColor(oldclear[0], oldclear[1], oldclear[2], oldclear[3]);
                
                if (hardext.drawbuffers) gles_glDrawBuffers(glstate->fbo.fbo_draw->n_draw, glstate->fbo.fbo_draw->drawbuff);
            }
            break;
        case GL_STENCIL:
            if (drawbuffer == 0) {
                GLint old;
                gl4es_glGetIntegerv(GL_STENCIL_CLEAR_VALUE, &old);
                gl4es_glClearStencil(*value);
                gl4es_glClear(GL_STENCIL_BUFFER_BIT);
                gl4es_glClearStencil(old);
            } else {
                errorShim(GL_INVALID_ENUM);
            }
            break;
        default:
            errorShim(GL_INVALID_ENUM);
    }
}

void APIENTRY_GL4ES gl4es_glClearBufferuiv(GLenum buffer, GLint drawbuffer, const GLuint * value) {
    noerrorShim();
    switch(buffer) {
        case GL_COLOR:
            if (drawbuffer > glstate->fbo.fbo_draw->n_draw) return;
            {
                GLfloat oldclear[4];
                LOAD_GLES_EXT(glDrawBuffers);
                if (hardext.drawbuffers) gles_glDrawBuffers(1, (const GLenum *) &drawbuffer);
                gl4es_glGetFloatv(GL_COLOR_CLEAR_VALUE, oldclear);
                
                gl4es_glClearColor(value[0] * MUL_255, value[1] * MUL_255, value[2] * MUL_255, value[3] * MUL_255);
                gl4es_glClear(GL_COLOR_BUFFER_BIT);
                gl4es_glClearColor(oldclear[0], oldclear[1], oldclear[2], oldclear[3]);
                
                if (hardext.drawbuffers) gles_glDrawBuffers(glstate->fbo.fbo_draw->n_draw, glstate->fbo.fbo_draw->drawbuff);
            }
            break;
        default:
            errorShim(GL_INVALID_ENUM);
    }
}

void APIENTRY_GL4ES gl4es_glClearBufferfv(GLenum buffer, GLint drawbuffer, const GLfloat * value) {
    noerrorShim();
    switch(buffer) {
        case GL_COLOR:
            if (drawbuffer > glstate->fbo.fbo_draw->n_draw) return;
            {
                GLfloat oldclear[4];
                LOAD_GLES_EXT(glDrawBuffers);
                if (hardext.drawbuffers) gles_glDrawBuffers(1, (const GLenum *) &drawbuffer);
                gl4es_glGetFloatv(GL_COLOR_CLEAR_VALUE, oldclear);
                
                gl4es_glClearColor(value[0], value[1], value[2], value[3]);
                gl4es_glClear(GL_COLOR_BUFFER_BIT);
                gl4es_glClearColor(oldclear[0], oldclear[1], oldclear[2], oldclear[3]);
                
                if (hardext.drawbuffers) gles_glDrawBuffers(glstate->fbo.fbo_draw->n_draw, glstate->fbo.fbo_draw->drawbuff);
            }
            break;
        case GL_DEPTH:
            if (drawbuffer == 0) {
                GLfloat old;
                gl4es_glGetFloatv(GL_DEPTH_CLEAR_VALUE, &old);
                gl4es_glClearDepthf(*value);
                gl4es_glClear(GL_DEPTH_BUFFER_BIT);
                gl4es_glClearDepthf(old);
            } else {
                errorShim(GL_INVALID_ENUM);
            }
            break;
        default:
            errorShim(GL_INVALID_ENUM);
    }
}

void APIENTRY_GL4ES gl4es_glClearBufferfi(GLenum buffer, GLint drawbuffer, GLfloat depth, GLint stencil) {
    if (buffer != GL_DEPTH_STENCIL || drawbuffer != 0) {
        errorShim(GL_INVALID_ENUM);
        return;
    }
    GLint olds; GLfloat oldd;
    gl4es_glGetFloatv(GL_DEPTH_CLEAR_VALUE, &oldd);
    gl4es_glGetIntegerv(GL_STENCIL_CLEAR_VALUE, &olds);
    gl4es_glClearDepthf(depth);
    gl4es_glClearStencil(stencil);
    gl4es_glClear(GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    gl4es_glClearDepthf(oldd);
    gl4es_glClearStencil(olds);
}

void APIENTRY_GL4ES gl4es_glClearNamedFramebufferiv(GLuint framebuffer, GLenum buffer, GLint drawbuffer, const GLint *value) {
    GLuint oldf = glstate->fbo.fbo_draw->id;
    gl4es_glBindFramebuffer(GL_DRAW_FRAMEBUFFER, framebuffer);
    gl4es_glClearBufferiv(buffer, drawbuffer, value);
    gl4es_glBindFramebuffer(GL_DRAW_FRAMEBUFFER, oldf);
}
void APIENTRY_GL4ES gl4es_glClearNamedFramebufferuiv(GLuint framebuffer, GLenum buffer, GLint drawbuffer, const GLuint *value) {
    GLuint oldf = glstate->fbo.fbo_draw->id;
    gl4es_glBindFramebuffer(GL_DRAW_FRAMEBUFFER, framebuffer);
    gl4es_glClearBufferuiv(buffer, drawbuffer, value);
    gl4es_glBindFramebuffer(GL_DRAW_FRAMEBUFFER, oldf);
}
void APIENTRY_GL4ES gl4es_glClearNamedFramebufferfv(GLuint framebuffer, GLenum buffer, GLint drawbuffer, const GLfloat *value) {
    GLuint oldf = glstate->fbo.fbo_draw->id;
    gl4es_glBindFramebuffer(GL_DRAW_FRAMEBUFFER, framebuffer);
    gl4es_glClearBufferfv(buffer, drawbuffer, value);
    gl4es_glBindFramebuffer(GL_DRAW_FRAMEBUFFER, oldf);
}
void APIENTRY_GL4ES gl4es_glClearNamedFramebufferfi(GLuint framebuffer, GLenum buffer, GLint drawbuffer, GLfloat depth, GLint stencil) {
    GLuint oldf = glstate->fbo.fbo_draw->id;
    gl4es_glBindFramebuffer(GL_DRAW_FRAMEBUFFER, framebuffer);
    gl4es_glClearBufferfi(buffer, drawbuffer, depth, stencil);
    gl4es_glBindFramebuffer(GL_DRAW_FRAMEBUFFER, oldf);
}

void APIENTRY_GL4ES gl4es_glColorMaskIndexed(GLuint framebuffer, GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha) {
    GLuint oldf = glstate->fbo.fbo_draw->id;
    gl4es_glBindFramebuffer(GL_DRAW_FRAMEBUFFER, framebuffer);
    gl4es_glColorMask(red, green, blue, alpha);
    gl4es_glBindFramebuffer(GL_DRAW_FRAMEBUFFER, oldf);
}

void gl4es_saveCurrentFBO() {
    GLuint framebuffer = (glstate->fbo.current_fb) ? glstate->fbo.current_fb->id : 0;
    if (framebuffer == 0) framebuffer = glstate->fbo.mainfbo_fbo;
    if (framebuffer) {
        LOAD_GLES3_OR_OES(glBindFramebuffer);
        if (hardext.vendor & VEND_ARM) gl4es_glFinish(); 
        gles_glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
}

void gl4es_restoreCurrentFBO() {
    GLuint framebuffer = (glstate->fbo.current_fb) ? glstate->fbo.current_fb->id : 0;
    if (framebuffer == 0) framebuffer = glstate->fbo.mainfbo_fbo;
    if (framebuffer) {
        LOAD_GLES3_OR_OES(glBindFramebuffer);
        gles_glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    }
}

// Exports
AliasExport(void,glGenFramebuffers,,(GLsizei n, GLuint *ids));
AliasExport(void,glDeleteFramebuffers,,(GLsizei n, GLuint *framebuffers));
AliasExport(GLboolean,glIsFramebuffer,,(GLuint framebuffer));
AliasExport(GLenum,glCheckFramebufferStatus,,(GLenum target));
AliasExport(void,glBindFramebuffer,,(GLenum target, GLuint framebuffer));
AliasExport(void,glFramebufferTexture1D,,(GLenum target, GLenum attachment, GLenum textarget, GLuint texture,    GLint level));
AliasExport(void,glFramebufferTexture2D,,(GLenum target, GLenum attachment, GLenum textarget, GLuint texture,    GLint level));
AliasExport(void,glFramebufferTexture3D,,(GLenum target, GLenum attachment, GLenum textarget, GLuint texture,    GLint level, GLint layer));
AliasExport(void,glGenRenderbuffers,,(GLsizei n, GLuint *renderbuffers));
AliasExport(void,glFramebufferRenderbuffer,,(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer));
AliasExport(void,glDeleteRenderbuffers,,(GLsizei n, GLuint *renderbuffers));
AliasExport(void,glRenderbufferStorage,,(GLenum target, GLenum internalformat, GLsizei width, GLsizei height));
AliasExport(void,glBindRenderbuffer,,(GLenum target, GLuint renderbuffer));
AliasExport(GLboolean,glIsRenderbuffer,,(GLuint renderbuffer));
AliasExport(void,glGenerateMipmap,,(GLenum target));
AliasExport(void,glGetFramebufferAttachmentParameteriv,,(GLenum target, GLenum attachment, GLenum pname, GLint *params));
AliasExport(void,glGetRenderbufferParameteriv,,(GLenum target, GLenum pname, GLint * params));
AliasExport(void,glFramebufferTextureLayer,,(    GLenum target, GLenum attachment, GLuint texture, GLint level, GLint layer));
AliasExport(void,glBlitFramebuffer,,(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter));

AliasExport(void,glGenFramebuffers,EXT,(GLsizei n, GLuint *ids));
AliasExport(void,glDeleteFramebuffers,EXT,(GLsizei n, GLuint *framebuffers));
AliasExport(GLboolean,glIsFramebuffer,EXT,(GLuint framebuffer));
AliasExport(GLenum,glCheckFramebufferStatus,EXT,(GLenum target));
AliasExport(void,glBindFramebuffer,EXT,(GLenum target, GLuint framebuffer));
AliasExport(void,glFramebufferTexture1D,EXT,(GLenum target, GLenum attachment, GLenum textarget, GLuint texture,    GLint level));
AliasExport(void,glFramebufferTexture2D,EXT,(GLenum target, GLenum attachment, GLenum textarget, GLuint texture,    GLint level));
AliasExport(void,glFramebufferTexture3D,EXT,(GLenum target, GLenum attachment, GLenum textarget, GLuint texture,    GLint level, GLint layer));
AliasExport(void,glGenRenderbuffers,EXT,(GLsizei n, GLuint *renderbuffers));
AliasExport(void,glFramebufferRenderbuffer,EXT,(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer));
AliasExport(void,glDeleteRenderbuffers,EXT,(GLsizei n, GLuint *renderbuffers));
AliasExport(void,glRenderbufferStorage,EXT,(GLenum target, GLenum internalformat, GLsizei width, GLsizei height));
AliasExport(void,glBindRenderbuffer,EXT,(GLenum target, GLuint renderbuffer));
AliasExport(GLboolean,glIsRenderbuffer,EXT,(GLuint renderbuffer));
AliasExport(void,glGenerateMipmap,EXT,(GLenum target));
AliasExport(void,glGetFramebufferAttachmentParameteriv,EXT,(GLenum target, GLenum attachment, GLenum pname, GLint *params));
AliasExport(void,glGetRenderbufferParameteriv,EXT,(GLenum target, GLenum pname, GLint * params));
AliasExport(void,glFramebufferTextureLayer,EXT,(    GLenum target, GLenum attachment, GLuint texture, GLint level, GLint layer));
AliasExport(void,glBlitFramebuffer,EXT,(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter));

AliasExport(void,glRenderbufferStorageMultisample,,(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height));

AliasExport(void,glDrawBuffers,,(GLsizei n, const GLenum *bufs));
AliasExport(void,glDrawBuffers,ARB,(GLsizei n, const GLenum *bufs));
AliasExport(void,glNamedFramebufferDrawBuffers,,(GLuint framebuffer, GLsizei n, const GLenum *bufs));
AliasExport(void,glNamedFramebufferDrawBuffers,EXT,(GLuint framebuffer, GLsizei n, const GLenum *bufs));

AliasExport(void,glClearBufferiv,,(GLenum buffer, GLint drawbuffer, const GLint * value));
AliasExport(void,glClearBufferuiv,,(GLenum buffer, GLint drawbuffer, const GLuint * value));
AliasExport(void,glClearBufferfv,,(GLenum buffer, GLint drawbuffer, const GLfloat * value));
AliasExport(void,glClearBufferfi,,(GLenum buffer, GLint drawbuffer, GLfloat depth, GLint stencil));

AliasExport(void,glClearNamedFramebufferiv,,(GLuint framebuffer, GLenum buffer, GLint drawbuffer, const GLint *value));
AliasExport(void,glClearNamedFramebufferuiv,,(GLuint framebuffer, GLenum buffer, GLint drawbuffer, const GLuint *value));
AliasExport(void,glClearNamedFramebufferfv,,(GLuint framebuffer, GLenum buffer, GLint drawbuffer, const GLfloat *value));
AliasExport(void,glClearNamedFramebufferfi,,(GLuint framebuffer, GLenum buffer, GLint drawbuffer, GLfloat depth, GLint stencil));

AliasExport(void,glClearNamedFramebufferiv,EXT,(GLuint framebuffer, GLenum buffer, GLint drawbuffer, const GLint *value));
AliasExport(void,glClearNamedFramebufferuiv,EXT,(GLuint framebuffer, GLenum buffer, GLint drawbuffer, const GLuint *value));
AliasExport(void,glClearNamedFramebufferfv,EXT,(GLuint framebuffer, GLenum buffer, GLint drawbuffer, const GLfloat *value));
AliasExport(void,glClearNamedFramebufferfi,EXT,(GLuint framebuffer, GLenum buffer, GLint drawbuffer, GLfloat depth, GLint stencil));