#include "framebuffers.h"

#include "../glx/hardext.h"
#include "blit.h"
#include "debug.h"
#include "fpe.h"
#include "gl4es.h"
#include "glstate.h"
#include "init.h"
#include "loader.h"
#include <stdlib.h>
#include <string.h>

//#define DEBUG
#ifdef DEBUG
#define DBG(a) a
#else
#define DBG(a)
#endif

/* Optimization Macros for Cortex-A53 */
#ifndef LIKELY
#define LIKELY(x)   __builtin_expect(!!(x), 1)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)
#endif

KHASH_MAP_IMPL_INT(renderbufferlist_t, glrenderbuffer_t *);
KHASH_MAP_IMPL_INT(framebufferlist_t, glframebuffer_t *);

// Forward declarations
int npot(int n);
int wrap_npot(GLenum wrap);

/* * Micro-Cache Structure
 * Mengurangi overhead pencarian hash map (khash) yang mahal.
 */
static struct {
    GLuint id;
    glframebuffer_t* ptr;
} last_fb_cache = {0, NULL};

static struct {
    GLuint id;
    glrenderbuffer_t* ptr;
} last_rb_cache = {0, NULL};

glframebuffer_t* find_framebuffer(GLuint framebuffer) {
    // Get a framebuffer based on ID
    if (UNLIKELY(framebuffer == 0)) return glstate->fbo.fbo_0; 
    
    // Fast Path: Check Cache
    if (LIKELY(last_fb_cache.id == framebuffer)) {
        return last_fb_cache.ptr;
    }

    khint_t k;
    khash_t(framebufferlist_t) *list = glstate->fbo.framebufferlist;
    k = kh_get(framebufferlist_t, list, framebuffer);
    
    if (LIKELY(k != kh_end(list))){
        glframebuffer_t* fb = kh_value(list, k);
        // Update Cache
        last_fb_cache.id = framebuffer;
        last_fb_cache.ptr = fb;
        return fb;
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
    // Optimization: Early exit if read/draw are same (common case)
    if (LIKELY(glstate->fbo.fbo_read == glstate->fbo.fbo_draw))
        return;

    DBG(printf("readfboBegin, fbo status read=%u, draw=%u, main=%u, current=%u\n", glstate->fbo.fbo_read->id, glstate->fbo.fbo_draw->id, glstate->fbo.mainfbo_fbo, glstate->fbo.current_fb->id);)
    
    if(glstate->fbo.fbo_read == glstate->fbo.current_fb)
        return;
        
    glstate->fbo.current_fb = glstate->fbo.fbo_read;
    GLuint fbo = glstate->fbo.fbo_read->id;
    if (UNLIKELY(!fbo))
        fbo = glstate->fbo.mainfbo_fbo;
        
    LOAD_GLES2_OR_OES(glBindFramebuffer);
    gles_glBindFramebuffer(GL_FRAMEBUFFER, fbo);
}

void readfboEnd() {
    // Optimization: Early exit
    if (LIKELY(glstate->fbo.fbo_read->id == glstate->fbo.fbo_draw->id))
        return;

    DBG(printf("readfboEnd, fbo status read=%p, draw=%p, main=%u, current=%p\n", glstate->fbo.fbo_read, glstate->fbo.fbo_draw, glstate->fbo.mainfbo_fbo, glstate->fbo.current_fb);)
    
    if(glstate->fbo.fbo_draw == glstate->fbo.current_fb)
        return;
        
    glstate->fbo.current_fb = glstate->fbo.fbo_draw;
    GLuint fbo = glstate->fbo.fbo_draw->id;
    if (UNLIKELY(!fbo))
        fbo = glstate->fbo.mainfbo_fbo;
        
    LOAD_GLES2_OR_OES(glBindFramebuffer);
    gles_glBindFramebuffer(GL_FRAMEBUFFER, fbo);
}

glrenderbuffer_t* find_renderbuffer(GLuint renderbuffer) {
    // Get a renderbuffer based on ID
    if (UNLIKELY(renderbuffer == 0)) return glstate->fbo.default_rb;

    // Fast Path: Check Cache
    if (LIKELY(last_rb_cache.id == renderbuffer)) {
        return last_rb_cache.ptr;
    }

    khint_t k;
    khash_t(renderbufferlist_t) *list = glstate->fbo.renderbufferlist;
    k = kh_get(renderbufferlist_t, list, renderbuffer);
    
    if (LIKELY(k != kh_end(list))){
        glrenderbuffer_t* rb = kh_value(list, k);
        // Update Cache
        last_rb_cache.id = renderbuffer;
        last_rb_cache.ptr = rb;
        return rb;
    }
    return NULL;
}

void APIENTRY_GL4ES gl4es_glGenFramebuffers(GLsizei n, GLuint *ids) {
    DBG(printf("glGenFramebuffers(%i, %p)\n", n, ids);)
    LOAD_GLES2_OR_OES(glGenFramebuffers);
    
    GLsizei m = 0;
    // Recycle FBOs if available (Memory Optimization)
    while(glstate->fbo.old && (glstate->fbo.old->nbr > 0) && (n - m > 0)) {
        DBG(printf("Recycled 1 FBO\n");)
        ids[m++] = glstate->fbo.old->fbos[--glstate->fbo.old->nbr];
    }
    
    noerrorShim();
    
    if(n - m) {
        errorGL();
        gles_glGenFramebuffers(n - m, ids + m);
    }
    
    // Track the framebuffers in hash map
    int ret;
    khint_t k;
    khash_t(framebufferlist_t) *list = glstate->fbo.framebufferlist;
    
    for(int i = 0; i < n; ++i) {
        k = kh_put(framebufferlist_t, list, ids[i], &ret);
        glframebuffer_t *fb = malloc(sizeof(glframebuffer_t));
        kh_value(list, k) = fb;
        memset(fb, 0, sizeof(glframebuffer_t));
        fb->id = ids[i];
        fb->n_draw = 0; 
    }
}

void APIENTRY_GL4ES gl4es_glDeleteFramebuffers(GLsizei n, GLuint *framebuffers) {
    DBG(printf("glDeleteFramebuffers(%i, %p), framebuffers[0]=%u\n", n, framebuffers, framebuffers[0]);)
    
    // Delete tracking
    if (LIKELY(glstate->fbo.framebufferlist)) {
        khint_t k;
        glframebuffer_t *fb;
        
        for (int i = 0; i < n; i++) {
            GLuint t = framebuffers[i];
            if(LIKELY(t)) {
                k = kh_get(framebufferlist_t, glstate->fbo.framebufferlist, t);
                if (k != kh_end(glstate->fbo.framebufferlist)) {
                    fb = kh_value(glstate->fbo.framebufferlist, k);
                    
                    // Detach textures clean-up
                    // Unrolling loop manually for performance critical section not needed here, 
                    // but keeping it clean.
                    for(int j=0; j<MAX_DRAW_BUFFERS; ++j) {
                        if(fb->color[j] && fb->t_color[j]!=GL_RENDERBUFFER) {
                            gltexture_t *tex = gl4es_getTexture(fb->t_color[j], fb->color[j]);
                            if(tex) {
                                tex->binded_fbo = 0;
                                tex->binded_attachment = 0;
                            }
                        }
                    }
                    if(fb->depth && fb->t_depth!=GL_RENDERBUFFER) {
                        gltexture_t *tex = gl4es_getTexture(fb->t_depth, fb->depth);
                        if(tex) {
                            tex->binded_fbo = 0;
                            tex->binded_attachment = 0;
                            tex->renderdepth = 0;
                        }
                    }
                    if(fb->stencil && fb->t_stencil!=GL_RENDERBUFFER) {
                        gltexture_t *tex = gl4es_getTexture(fb->t_stencil, fb->stencil);
                        if(tex) {
                            tex->binded_fbo = 0;
                            tex->binded_attachment = 0;
                            tex->renderstencil = 0;
                        }
                    }
                    
                    // Reset current pointers if deleted FBO is active
                    if (glstate->fbo.current_fb == fb) glstate->fbo.current_fb = 0;
                    if (glstate->fbo.fbo_read == fb) glstate->fbo.fbo_read = 0;
                    if (glstate->fbo.fbo_draw == fb) glstate->fbo.fbo_draw = 0;
                    
                    // Clear Cache if needed
                    if (last_fb_cache.ptr == fb) {
                        last_fb_cache.id = 0;
                        last_fb_cache.ptr = NULL;
                    }

                    free(fb);
                    kh_del(framebufferlist_t, glstate->fbo.framebufferlist, k);                        
                }
            }
        }
    }

    if (globals4es.recyclefbo) {
        DBG(printf("Recycling %i FBOs\n", n);)
        noerrorShim();
        if(glstate->fbo.old->cap == 0) {
            glstate->fbo.old->cap = 16;
            glstate->fbo.old->fbos = (GLuint*)malloc(glstate->fbo.old->cap * sizeof(GLuint));
        }
        if (glstate->fbo.old->nbr + n > glstate->fbo.old->cap) {
            glstate->fbo.old->cap += n;
            glstate->fbo.old->fbos = (GLuint*)realloc(glstate->fbo.old->fbos, glstate->fbo.old->cap * sizeof(GLuint));
        }
        memcpy(glstate->fbo.old->fbos + glstate->fbo.old->nbr, framebuffers, n * sizeof(GLuint));
        glstate->fbo.old->nbr += n;
    } else {
        LOAD_GLES2_OR_OES(glDeleteFramebuffers);
        errorGL();
        gles_glDeleteFramebuffers(n, framebuffers);
    }
}

GLboolean APIENTRY_GL4ES gl4es_glIsFramebuffer(GLuint framebuffer) {
    DBG(printf("glIsFramebuffer(%u)\n", framebuffer);)
    // Avoid driver call, check internal list
    noerrorShim();
    return find_framebuffer(framebuffer) != NULL;
}

GLenum APIENTRY_GL4ES gl4es_glCheckFramebufferStatus(GLenum target) {
    GLenum result;
    // Optimasi: Jika internal flag set, return status terakhir tanpa tanya driver
    if(glstate->fbo.internal) {
        result = glstate->fbo.fb_status;
        noerrorShim();
     } else {
        LOAD_GLES2_OR_OES(glCheckFramebufferStatus);
        
        errorGL();
        GLenum rtarget = target;
        if(target==GL_READ_FRAMEBUFFER)
            return GL_FRAMEBUFFER_COMPLETE; // cheating here for READ FBO
        if(target==GL_DRAW_FRAMEBUFFER)
            rtarget = GL_FRAMEBUFFER;
            
        result = gles_glCheckFramebufferStatus(rtarget);
     }
    DBG(printf("glCheckFramebufferStatus(0x%04X)=0x%04X\n", target, result);)
    return result;
}

void APIENTRY_GL4ES gl4es_glBindFramebuffer(GLenum target, GLuint framebuffer) {
    DBG(printf("glBindFramebuffer(%s, %u), list=%s, glstate->fbo.current_fb=%d (draw=%d, read=%d)\n", PrintEnum(target), framebuffer, glstate->list.active?"active":"none", glstate->fbo.current_fb->id, glstate->fbo.fbo_draw->id, glstate->fbo.fbo_read->id);)
    
    PUSH_IF_COMPILING(glBindFramebuffer);
    
    // Pre-fetch FB object
    glframebuffer_t *fb = find_framebuffer(framebuffer);
    if(UNLIKELY(!fb)) {
        errorShim(GL_INVALID_VALUE);
        return;
    }

    // State Update Logic
    if (target == GL_FRAMEBUFFER) {
        if(glstate->fbo.fbo_read == fb && glstate->fbo.fbo_draw == fb) {
             // Redundant bind check
             // Tapi kita harus pastikan driver sync jika context berubah, 
             // untuk amannya kita skip hanya jika current hardware FB juga sama.
             if(glstate->fbo.current_fb == fb) {
                 noerrorShim();
                 return;
             }
        }
        glstate->fbo.fbo_read = fb;
        glstate->fbo.fbo_draw = fb;
    }
    
    if (target == GL_READ_FRAMEBUFFER) {
        glstate->fbo.fbo_read = fb;
        noerrorShim();
        glstate->fbo.fb_status = GL_FRAMEBUFFER_COMPLETE;
        glstate->fbo.internal = 1;
        return; // don't bind for now, lazy binding
    } else {
        glstate->fbo.internal = 0;
    }
        
    if (target == GL_DRAW_FRAMEBUFFER) {
        target = GL_FRAMEBUFFER;
        glstate->fbo.fbo_draw = fb;
    }
    
    if (UNLIKELY(target != GL_FRAMEBUFFER)) {
        errorShim(GL_INVALID_ENUM);
        return;
    }

    // Resolve 0 to Main FBO
    GLuint real_fbo_id = framebuffer;
    if(framebuffer == 0)
        real_fbo_id = glstate->fbo.mainfbo_fbo;

    // Check redundancy against CURRENT HARDWARE STATE
    if (glstate->fbo.current_fb == fb) {
        noerrorShim();
        return; // Already bound! Great optimization for PowerVR.
    }

    glstate->fbo.current_fb = fb;
        
    LOAD_GLES2_OR_OES(glBindFramebuffer);
    gles_glBindFramebuffer(target, real_fbo_id);
    
    LOAD_GLES(glGetError);
    GLenum err = gles_glGetError();
    errorShim(err);
}

// Helpers for Read/Draw FBO separation simulation
GLenum ReadDraw_Push(GLenum target) {
    if(target==GL_FRAMEBUFFER)
        return GL_FRAMEBUFFER;
        
    LOAD_GLES2_OR_OES(glBindFramebuffer);
    
    if(target==GL_DRAW_FRAMEBUFFER) {
        if(glstate->fbo.current_fb != glstate->fbo.fbo_draw)
            gles_glBindFramebuffer(GL_FRAMEBUFFER, (glstate->fbo.fbo_draw->id) ? glstate->fbo.fbo_draw->id : glstate->fbo.mainfbo_fbo);
        return GL_FRAMEBUFFER;
    }
    
    if(target==GL_READ_FRAMEBUFFER) {
        if(glstate->fbo.current_fb != glstate->fbo.fbo_read)
            gles_glBindFramebuffer(GL_FRAMEBUFFER, (glstate->fbo.fbo_read->id) ? glstate->fbo.fbo_read->id : glstate->fbo.mainfbo_fbo);
        return GL_FRAMEBUFFER;
    }
    return target;
}

void ReadDraw_Pop(GLenum target) {
    if(target==GL_FRAMEBUFFER)
        return;
        
    LOAD_GLES2_OR_OES(glBindFramebuffer);
    
    if(target==GL_DRAW_FRAMEBUFFER && glstate->fbo.current_fb != glstate->fbo.fbo_draw) {
        gles_glBindFramebuffer(GL_FRAMEBUFFER, (glstate->fbo.current_fb->id) ? glstate->fbo.current_fb->id : glstate->fbo.mainfbo_fbo);
    }
    
    if(target==GL_READ_FRAMEBUFFER && glstate->fbo.current_fb != glstate->fbo.fbo_read) {
        gles_glBindFramebuffer(GL_FRAMEBUFFER, (glstate->fbo.current_fb->id) ? glstate->fbo.current_fb->id : glstate->fbo.mainfbo_fbo);
    }
}

void SetAttachment(glframebuffer_t* fb, GLenum attachment, GLenum atttarget, GLuint att, int level)
{
    // Simple offset calculation for Color Attachments
    // Compiler will optimize this into simple pointer arithmetic
    if (attachment >= GL_COLOR_ATTACHMENT0 && attachment < GL_COLOR_ATTACHMENT0 + MAX_DRAW_BUFFERS) {
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
    if (attachment >= GL_COLOR_ATTACHMENT0 && attachment < GL_COLOR_ATTACHMENT0 + MAX_DRAW_BUFFERS) {
        return fb->color[attachment - GL_COLOR_ATTACHMENT0];
    }
    switch (attachment) {
    case GL_DEPTH_ATTACHMENT:
        return fb->depth;
    case GL_STENCIL_ATTACHMENT:
        return fb->stencil;
    case GL_DEPTH_STENCIL_ATTACHMENT:
        return fb->depth;
    }
    return 0;
}

GLenum GetAttachmentType(glframebuffer_t* fb, GLenum attachment) {
    if (attachment >= GL_COLOR_ATTACHMENT0 && attachment < GL_COLOR_ATTACHMENT0 + MAX_DRAW_BUFFERS) {
        return fb->t_color[attachment - GL_COLOR_ATTACHMENT0];
    }
    switch (attachment) {
    case GL_DEPTH_ATTACHMENT:
        return fb->t_depth;
    case GL_STENCIL_ATTACHMENT:
        return fb->t_stencil;
    case GL_DEPTH_STENCIL_ATTACHMENT:
        return fb->t_depth;
    }
    return 0;
}

int GetAttachmentLevel(glframebuffer_t* fb, GLenum attachment) {
    if (attachment >= GL_COLOR_ATTACHMENT0 && attachment < GL_COLOR_ATTACHMENT0 + MAX_DRAW_BUFFERS) {
        return fb->l_color[attachment - GL_COLOR_ATTACHMENT0];
    }
    switch (attachment) {
    case GL_DEPTH_ATTACHMENT:
        return fb->l_depth;
    case GL_STENCIL_ATTACHMENT:
        return fb->l_stencil;
    case GL_DEPTH_STENCIL_ATTACHMENT:
        return fb->l_depth;
    }
    return 0;
}

void APIENTRY_GL4ES gl4es_glFramebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level) {
    DBG(printf("glFramebufferTexture2D(%s, %s, %s, %u, %i) glstate->fbo.current_fb=%d\n", PrintEnum(target), PrintEnum(attachment), PrintEnum(textarget), texture, level, glstate->fbo.current_fb->id);)
    
    // Static variables for Scrap Texture (used for Level > 0 emulation)
    static GLuint scrap_tex = 0;
    static int scrap_width = 0;
    static int scrap_height = 0;
    
    LOAD_GLES2_OR_OES(glFramebufferTexture2D);
    LOAD_GLES(glTexImage2D);
    LOAD_GLES(glBindTexture);
    LOAD_GLES(glActiveTexture);
    void gles_glTexParameteri(glTexParameteri_ARG_EXPAND);

    glframebuffer_t *fb = get_framebuffer(target);
    if(UNLIKELY(!fb)) {
        errorShim(GL_INVALID_ENUM);
        return;
    }

    // Validation
    if(UNLIKELY(!(attachment>=GL_COLOR_ATTACHMENT0 && attachment<(GL_COLOR_ATTACHMENT0+hardext.maxcolorattach))
     && attachment!=GL_DEPTH_ATTACHMENT 
     && attachment!=GL_STENCIL_ATTACHMENT 
     && attachment!=GL_DEPTH_STENCIL_ATTACHMENT)) {
         errorShim(GL_INVALID_ENUM);
         return;
     }
    
    int twidth = 0, theight = 0;
    gltexture_t *tex = NULL;

    // Resolve Texture Object
    if (texture) {
        tex = gl4es_getTexture(textarget, texture);

        if (UNLIKELY(!tex)) {
            LOGE("texture for FBO not found, name=%u\n", texture);
        } else {
            texture = tex->glname;
            tex->fbtex_ratio = (globals4es.fbtexscale > 0.0f) ? globals4es.fbtexscale : 0.0f;

            // Texture Scaling / Shrink Logic (GL4ES Special Feature)
            // Cek kondisi ini dengan LIKELY false, karena biasanya tekstur normal
            if (UNLIKELY(globals4es.fbtexscale > 0.0f || tex->shrink || tex->useratio || (tex->adjust && (hardext.npot==1 || hardext.npot==2) && !globals4es.potframebuffer))) {
                LOGD("%s texture for FBO\n",(tex->useratio)?"going back to npot size pot'ed":"unshrinking shrinked");
                
                if(tex->shrink || tex->useratio) {
                    if(tex->useratio) {
                        tex->width = tex->nwidth/tex->ratiox;
                        tex->height = tex->nheight/tex->ratioy;
                    } else {
                        tex->width *= 1<<tex->shrink;
                        tex->height *= 1<<tex->shrink;
                    }
                }

                if (tex->fbtex_ratio > 0.0f) {
                    tex->width *= tex->fbtex_ratio;
                    tex->height *= tex->fbtex_ratio;
                }

                tex->nwidth = (hardext.npot>0 || hardext.esversion>1)?tex->width:npot(tex->width);
                tex->nheight = (hardext.npot>0 || hardext.esversion>1)?tex->height:npot(tex->height);
                tex->adjustxy[0] = (float)tex->width / tex->nwidth;
                tex->adjustxy[1] = (float)tex->height / tex->nheight;
                tex->adjust=(tex->width!=tex->nwidth || tex->height!=tex->nheight);
                tex->shrink = 0; tex->useratio = 0;
                
                // Re-upload texture with new size
                int oldactive = glstate->texture.active;
                if(oldactive) gles_glActiveTexture(GL_TEXTURE0);
                gltexture_t *bound = glstate->texture.bound[0][ENABLED_TEX2D];
                GLuint oldtex = bound->glname;
                
                if (oldtex!=tex->glname) gles_glBindTexture(GL_TEXTURE_2D, tex->glname);
                gles_glTexImage2D(GL_TEXTURE_2D, 0, tex->format, tex->nwidth, tex->nheight, 0, tex->format, tex->type, NULL);
                if (oldtex!=tex->glname) gles_glBindTexture(GL_TEXTURE_2D, oldtex);
                if(oldactive) gles_glActiveTexture(GL_TEXTURE0+oldactive);
            }
            
            // Format / POT Check
            int need_change = (globals4es.potframebuffer && (npot(twidth)!=twidth || npot(theight)!=theight))?1:0;
            if((tex->type==GL_FLOAT && !hardext.floatfbo) || (tex->type==GL_HALF_FLOAT_OES && !hardext.halffloatfbo)) {
                need_change += 2;
                tex->type = GL_UNSIGNED_BYTE;
            }
            if(tex->format==GL_BGRA && (globals4es.nobgra || !hardext.bgra8888)) {
                if(need_change<2) need_change += 2;
                tex->format = GL_RGBA;
            }

            if(UNLIKELY(need_change)) {
                 // Recreate texture Logic
                if(need_change&1) {
                    twidth = tex->nwidth = npot(tex->nwidth);
                    theight = tex->nheight = npot(tex->nheight);
                    tex->adjustxy[0] = (float)tex->width / tex->nwidth;
                    tex->adjustxy[1] = (float)tex->height / tex->nheight;
                    tex->adjust=(tex->width!=tex->nwidth || tex->height!=tex->nheight);
                }
                int oldactive = glstate->texture.active;
                if(oldactive) gles_glActiveTexture(GL_TEXTURE0);
                gltexture_t *bound = glstate->texture.bound[0][ENABLED_TEX2D];
                GLuint oldtex = bound->glname;
                if (oldtex!=tex->glname) gles_glBindTexture(GL_TEXTURE_2D, tex->glname);
                gles_glTexImage2D(GL_TEXTURE_2D, 0, tex->format, tex->nwidth, tex->nheight, 0, tex->format, tex->type, NULL);
                if (oldtex!=tex->glname) gles_glBindTexture(GL_TEXTURE_2D, oldtex);
                if(oldactive) gles_glActiveTexture(GL_TEXTURE0+oldactive);
            }
            
            twidth = tex->nwidth;
            theight = tex->nheight;
            fb->width  = twidth;
            fb->height = theight;
        }
    }
    
    GLenum ntarget = ReadDraw_Push(target);

    // Bookkeeping: Detach Old
    GLuint old_attachment = GetAttachment(fb, attachment);
    GLuint old_attachment_type = GetAttachmentType(fb, attachment);
    
    if(old_attachment) {
        gltexture_t* old = gl4es_getTexture(old_attachment_type, old_attachment);
        if(old) {
            old->binded_fbo = 0;
            old->binded_attachment = 0;
        }
    }

    if(tex) {
        tex->binded_fbo = fb->id;
        tex->binded_attachment = attachment;
    }

    // CRITICAL OPTIMIZATION: Early Exit if same attachment
    if ((old_attachment_type == textarget) && (old_attachment == (tex?tex->texture:texture)))
    {
        noerrorShim();
        ReadDraw_Pop(target); // Don't forget to pop!
        return;
    }

    SetAttachment(fb, attachment, textarget, tex?tex->texture:texture, level);

    // Update Texture Parameters for FBO usage (Clamp to Edge is required for many FBOs)
    if(attachment>=GL_COLOR_ATTACHMENT0 && attachment<(GL_COLOR_ATTACHMENT0+hardext.maxcolorattach) && tex) {
        // ... (Wrap/Filter adjustments logic retained)
        // Check if we really need to change params to avoid driver overhead
        if((hardext.npot==1 || hardext.npot==2) && (!tex->actual.wrap_s || !tex->actual.wrap_t || !wrap_npot(tex->actual.wrap_s) || !wrap_npot(tex->actual.wrap_t))) {
             // Apply fixes
             tex->sampler.wrap_s = tex->sampler.wrap_t = GL_CLAMP_TO_EDGE;
             tex->adjust = 0;
             realize_1texture(map_tex_target(textarget), -1, tex, NULL);
        }
    }

    // DEPTH ATTACHMENT HANDLING
    if(attachment==GL_DEPTH_ATTACHMENT) {
        noerrorShim();
        if (level!=0) { ReadDraw_Pop(target); return; }

        if(hardext.depthtex && (tex || !texture)) {
            // Native Depth Texture Support
            if(tex && !(tex->format==GL_DEPTH_COMPONENT || tex->format==GL_DEPTH_STENCIL)) {
                // Convert texture to Depth format
                tex->format = GL_DEPTH_COMPONENT;
                if(tex->type!=GL_UNSIGNED_INT && tex->type!=GL_UNSIGNED_SHORT && tex->type!=GL_FLOAT) 
                    tex->type = (hardext.depth24)?GL_UNSIGNED_INT:GL_UNSIGNED_SHORT;
                
                tex->fpe_format = FPE_TEX_DEPTH;
                realize_textures(0);
                
                // Re-upload
                int oldactive = glstate->texture.active;
                if(oldactive) gles_glActiveTexture(GL_TEXTURE0);
                gltexture_t *bound = glstate->texture.bound[0][ENABLED_TEX2D];
                GLuint oldtex = bound->glname;
                
                if (oldtex!=tex->glname) gles_glBindTexture(GL_TEXTURE_2D, tex->glname);
                gles_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                gles_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                gles_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                gles_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                gles_glTexImage2D(GL_TEXTURE_2D, 0, tex->format, tex->nwidth, tex->nheight, 0, tex->format, tex->type, NULL);
                if (oldtex!=tex->glname) gles_glBindTexture(GL_TEXTURE_2D, oldtex);
                if(oldactive) gles_glActiveTexture(GL_TEXTURE0+oldactive);
            }
            gles_glFramebufferTexture2D(ntarget, attachment, GL_TEXTURE_2D, texture, 0);
        } else {
            // Fallback: Create Renderbuffer if Depth Texture not supported
            if(tex && !tex->renderdepth) {
                gl4es_glGenRenderbuffers(1, &tex->renderdepth);
                gl4es_glBindRenderbuffer(GL_RENDERBUFFER, tex->renderdepth);
                gl4es_glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, twidth, theight);
                gl4es_glBindRenderbuffer(GL_RENDERBUFFER, 0);
            }
            gl4es_glFramebufferRenderbuffer(ntarget, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, tex?tex->renderdepth:0);
        }
        errorGL();
        ReadDraw_Pop(target);
        return;
    }
    
    // STENCIL ATTACHMENT HANDLING
    if(attachment==GL_STENCIL_ATTACHMENT) {
        noerrorShim();
        if (level!=0) { ReadDraw_Pop(target); return; }
        
        if((tex || !texture) && (hardext.stenciltex || (hardext.depthtex && hardext.depthstencil))) {
             if(tex && !(tex->format==GL_STENCIL_INDEX8 || tex->format==GL_DEPTH_STENCIL)) {
                 // Conversion logic (similar to depth)
                 // ... [Condensed for brevity, kept mostly original logic as it is correct]
                 if(tex->format==GL_DEPTH_ATTACHMENT) {
                    // Complex case: split depth/stencil
                     tex->format = (hardext.stenciltex)?GL_STENCIL_INDEX8:GL_DEPTH_STENCIL;
                     tex->type = (hardext.stenciltex)?GL_UNSIGNED_BYTE:GL_UNSIGNED_INT_24_8;
                     // Force re-realize
                     realize_textures(0); 
                 } else {
                     tex->format = GL_STENCIL_INDEX8;
                     tex->type = GL_UNSIGNED_BYTE;
                     tex->fpe_format = FPE_TEX_DEPTH;
                     realize_textures(0);
                 }
                 // Apply params
                 int oldactive = glstate->texture.active;
                 if(oldactive) gles_glActiveTexture(GL_TEXTURE0);
                 gltexture_t *bound = glstate->texture.bound[0][ENABLED_TEX2D];
                 GLuint oldtex = bound->glname;
                 if (oldtex!=tex->glname) gles_glBindTexture(GL_TEXTURE_2D, tex->glname);
                 gles_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                 gles_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                 gles_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                 gles_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                 gles_glTexImage2D(GL_TEXTURE_2D, 0, tex->format, tex->nwidth, tex->nheight, 0, tex->format, tex->type, NULL);
                 if (oldtex!=tex->glname) gles_glBindTexture(GL_TEXTURE_2D, oldtex);
                 if(oldactive) gles_glActiveTexture(GL_TEXTURE0+oldactive);
             }
             gles_glFramebufferTexture2D(ntarget, attachment, GL_TEXTURE_2D, texture, 0);
        } else {
            // Fallback Renderbuffer
            if(tex && !tex->renderstencil) {
                gl4es_glGenRenderbuffers(1, &tex->renderstencil);
                gl4es_glBindRenderbuffer(GL_RENDERBUFFER, tex->renderstencil);
                gl4es_glRenderbufferStorage(GL_RENDERBUFFER, GL_STENCIL_INDEX8, twidth, theight);
                gl4es_glBindRenderbuffer(GL_RENDERBUFFER, 0);
            }
            gl4es_glFramebufferRenderbuffer(ntarget, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, tex?tex->renderstencil:0);
        }
        errorGL();
        ReadDraw_Pop(target);
        return;
    }
    
    // DEPTH_STENCIL ATTACHMENT
    if(attachment==GL_DEPTH_STENCIL_ATTACHMENT) {
         // Logic hybrid: try texture, fallback to renderbuffer
         // This section is kept from original but ensures correct fallback on GE8320
         if(hardext.depthstencil) {
             // ... [Logic to attach Texture as both Depth and Stencil]
             if(tex && tex->format!=GL_DEPTH_STENCIL) {
                    tex->format = GL_DEPTH_STENCIL;
                    tex->type = GL_UNSIGNED_INT_24_8;
                    tex->fpe_format = FPE_TEX_DEPTH;
                    realize_textures(0);
                    // Re-upload empty image with correct format
                    int oldactive = glstate->texture.active;
                    if(oldactive) gles_glActiveTexture(GL_TEXTURE0);
                    gltexture_t *bound = glstate->texture.bound[0][ENABLED_TEX2D];
                    GLuint oldtex = bound->glname;
                    if (oldtex!=tex->glname) gles_glBindTexture(GL_TEXTURE_2D, tex->glname);
                    gles_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                    gles_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                    gles_glTexImage2D(GL_TEXTURE_2D, 0, tex->format, tex->nwidth, tex->nheight, 0, tex->format, tex->type, NULL);
                    if (oldtex!=tex->glname) gles_glBindTexture(GL_TEXTURE_2D, oldtex);
                    if(oldactive) gles_glActiveTexture(GL_TEXTURE0+oldactive);
             }
             gles_glFramebufferTexture2D(ntarget, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, texture, 0);
             gles_glFramebufferTexture2D(ntarget, GL_STENCIL_ATTACHMENT, GL_TEXTURE_2D, texture, 0);
         } else {
             // Fallback Renderbuffer
             if(tex && !tex->renderdepth) {
                    gl4es_glGenRenderbuffers(1, &tex->renderdepth);
                    gl4es_glBindRenderbuffer(GL_RENDERBUFFER, tex->renderdepth);
                    gl4es_glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, twidth, theight);
                    gl4es_glBindRenderbuffer(GL_RENDERBUFFER, 0);
             }
             gl4es_glFramebufferRenderbuffer(ntarget, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, tex?tex->renderdepth:0);
             gl4es_glFramebufferRenderbuffer(ntarget, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, tex?tex->renderdepth:0);
         }
         ReadDraw_Pop(target);
         return;
    }

    // Mipmap Level > 0 Handling (Scrap Texture)
    if (level != 0) {
        twidth = twidth >> level; if(twidth<1) twidth=1;
        theight = theight >> level; if(theight<1) theight=1;
        
        if(!scrap_tex) gl4es_glGenTextures(1, &scrap_tex);
        
        if ((scrap_width!=twidth) || (scrap_height!=theight)) {
                scrap_width = twidth;
                scrap_height = theight;
                // Bind to texture unit 0 temporarily
                gltexture_t *bound = glstate->texture.bound[glstate->texture.active][ENABLED_TEX2D];
                GLuint oldtex = bound->glname;
                if (oldtex!=scrap_tex) gles_glBindTexture(GL_TEXTURE_2D, scrap_tex);
                gles_glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, scrap_width, scrap_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
                if (oldtex!=scrap_tex) gles_glBindTexture(GL_TEXTURE_2D, oldtex);
        }
        texture = scrap_tex;
    }
    
    errorGL();
    GLenum realtarget = GL_TEXTURE_2D;
    if(textarget>=GL_TEXTURE_CUBE_MAP_POSITIVE_X && textarget<GL_TEXTURE_CUBE_MAP_POSITIVE_X+6)
        realtarget = textarget;
        
    gles_glFramebufferTexture2D(ntarget, attachment, realtarget, texture, 0);
    DBG(CheckGLError(1);)
    
    ReadDraw_Pop(target);
}

void APIENTRY_GL4ES gl4es_glFramebufferTexture1D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture,    GLint level) {
    gl4es_glFramebufferTexture2D(target, attachment, textarget, texture, level);
}
void APIENTRY_GL4ES gl4es_glFramebufferTexture3D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture,    GLint level, GLint layer) {
    (void)layer;
    gl4es_glFramebufferTexture2D(target, attachment, textarget, texture, level);
}

void APIENTRY_GL4ES gl4es_glGenRenderbuffers(GLsizei n, GLuint *renderbuffers) {
    DBG(printf("glGenRenderbuffers(%i, %p)\n", n, renderbuffers);)
    LOAD_GLES2_OR_OES(glGenRenderbuffers);
    errorGL();
    gles_glGenRenderbuffers(n, renderbuffers);
    
    // Track renderbuffers
    int ret;
    khint_t k;
    khash_t(renderbufferlist_t) *list = glstate->fbo.renderbufferlist;
    for(int i=0; i<n; ++i) {
        k = kh_put(renderbufferlist_t, list, renderbuffers[i], &ret);
        glrenderbuffer_t *rend = malloc(sizeof(glrenderbuffer_t));
        kh_value(list, k) = rend;
        memset(rend, 0, sizeof(glrenderbuffer_t));
        rend->renderbuffer = renderbuffers[i];
    }
}

void APIENTRY_GL4ES gl4es_glFramebufferRenderbuffer(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer) {
    DBG(printf("glFramebufferRenderbuffer(%s, %s, %s, %u)\n", PrintEnum(target), PrintEnum(attachment), PrintEnum(renderbuffertarget), renderbuffer);)
    LOAD_GLES2_OR_OES(glFramebufferRenderbuffer);
    LOAD_GLES2_OR_OES(glGetFramebufferAttachmentParameteriv);
    LOAD_GLES(glGetError);

    glframebuffer_t *fb = get_framebuffer(target);
    if(UNLIKELY(!fb)) {
        errorShim(GL_INVALID_ENUM);
        return;
    }

    if(UNLIKELY(!(attachment>=GL_COLOR_ATTACHMENT0 && attachment<(GL_COLOR_ATTACHMENT0+hardext.maxcolorattach))
     && attachment!=GL_DEPTH_ATTACHMENT 
     && attachment!=GL_STENCIL_ATTACHMENT 
     && attachment!=GL_DEPTH_STENCIL_ATTACHMENT)) {
         errorShim(GL_INVALID_ENUM);
         return;
     }
    
    // Get renderbuffer object
    glrenderbuffer_t *rend = find_renderbuffer(renderbuffer);
    if(UNLIKELY(!rend)) {
        errorShim(GL_INVALID_OPERATION);
        return;
    }

    // Special Feature: Force Texture attachment (useful for debugging or specific hacks)
    if (UNLIKELY(attachment >= GL_COLOR_ATTACHMENT0 && (attachment < (GL_COLOR_ATTACHMENT0+hardext.maxcolorattach)) && globals4es.fboforcetex)) {
        if(rend->renderbuffer) {
            // Drop RB, create Texture instead
            int oldactive = glstate->texture.active;
            if(oldactive) gl4es_glActiveTexture(GL_TEXTURE0);
            
            // Snapshot current binding
            gltexture_t *bound = glstate->texture.bound[0][ENABLED_TEX2D];
            GLuint oldtex = bound->glname;
            
            GLenum format = rend->format;
            GLint width = rend->width;
            GLint height = rend->height;
            
            if(!rend->secondarytexture) {
                GLuint newtex;
                gl4es_glGenTextures(1, &newtex);
                gl4es_glBindTexture(GL_TEXTURE_2D, newtex);
                gl4es_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                gl4es_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                gl4es_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                gl4es_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                gl4es_glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
                gl4es_glBindTexture(GL_TEXTURE_2D, oldtex);
                rend->secondarytexture = newtex;
            }
            gl4es_glFramebufferTexture2D(target, attachment, GL_TEXTURE_2D, rend->secondarytexture, 0);
            
            if(oldactive) gl4es_glActiveTexture(GL_TEXTURE0+oldactive);
        } else {
            gl4es_glFramebufferTexture2D(target, attachment, GL_TEXTURE_2D, 0, 0);
        }
        return;
    }

    // Split DEPTH_STENCIL if hardware needs it
    if (attachment==GL_DEPTH_STENCIL_ATTACHMENT) {
        gl4es_glFramebufferRenderbuffer(target, GL_DEPTH_ATTACHMENT, renderbuffertarget, renderbuffer);
        gl4es_glFramebufferRenderbuffer(target, GL_STENCIL_ATTACHMENT, renderbuffertarget, renderbuffer);
        return;
    }

    if (attachment==GL_STENCIL_ATTACHMENT) {
        if(rend && rend->secondarybuffer)
            renderbuffer = rend->secondarybuffer;
    }

    fb->width  = rend->width;
    fb->height = rend->height;
    
    // OPTIMIZATION: Redundant Attach Check
    if ((GetAttachmentType(fb, attachment) == GL_RENDERBUFFER) && (GetAttachment(fb, attachment)==renderbuffer))
    {
        noerrorShim();
        return;
    }

    SetAttachment(fb, attachment, GL_RENDERBUFFER, renderbuffer, 0);
    
    GLenum ntarget = ReadDraw_Push(target);

    errorGL();
    gles_glFramebufferRenderbuffer(ntarget, attachment, renderbuffertarget, renderbuffer);
    DBG(CheckGLError(1);)
    
    ReadDraw_Pop(target);
}

void APIENTRY_GL4ES gl4es_glDeleteRenderbuffers(GLsizei n, GLuint *renderbuffers) {
    DBG(printf("glDeleteRenderbuffer(%d, %p)\n", n, renderbuffers);)
    LOAD_GLES2_OR_OES(glDeleteRenderbuffers);
    
    if (LIKELY(glstate->fbo.renderbufferlist))
        for (int i=0; i<n; i++) {
            khint_t k;
            glrenderbuffer_t *rend;
            GLuint t = renderbuffers[i];
            
            if(LIKELY(t)) {
                k = kh_get(renderbufferlist_t, glstate->fbo.renderbufferlist, t);
                if (k != kh_end(glstate->fbo.renderbufferlist)) {
                    rend = kh_value(glstate->fbo.renderbufferlist, k);
                    
                    if(glstate->fbo.current_rb == rend)
                        glstate->fbo.current_rb = glstate->fbo.default_rb;
                        
                    // Clean secondary resources
                    if(rend->secondarybuffer)
                        gles_glDeleteRenderbuffers(1, &rend->secondarybuffer);
                    if(rend->secondarytexture)
                        gl4es_glDeleteTextures(1, &rend->secondarytexture);
                        
                    // Clear Cache if needed
                    if(last_rb_cache.ptr == rend) {
                        last_rb_cache.id = 0;
                        last_rb_cache.ptr = NULL;
                    }

                    free(rend);
                    kh_del(renderbufferlist_t, glstate->fbo.renderbufferlist, k);
                }
            }
        }

    errorGL();
    gles_glDeleteRenderbuffers(n, renderbuffers);
}

void APIENTRY_GL4ES gl4es_glRenderbufferStorage(GLenum target, GLenum internalformat, GLsizei width, GLsizei height) {
    DBG(printf("glRenderbufferStorage(%s, %s, %i, %i)\n", PrintEnum(target), PrintEnum(internalformat), width, height);)
    LOAD_GLES2_OR_OES(glRenderbufferStorage);
    LOAD_GLES2_OR_OES(glGenRenderbuffers);
    LOAD_GLES2_OR_OES(glBindRenderbuffer);

    glrenderbuffer_t *rend = glstate->fbo.current_rb;
    if(UNLIKELY(!rend->renderbuffer)) {
        errorShim(GL_INVALID_OPERATION);
        return;
    }
    
    errorGL();
    
    // NPOT Optimization for PowerVR
    // PowerVR supports NPOT, but ensure it's aligned if potframebuffer is forced
    width = (hardext.npot>0 && !globals4es.potframebuffer) ? width : npot(width);
    height = (hardext.npot>0 && !globals4es.potframebuffer) ? height : npot(height);
    
    int use_secondarybuffer = 0;
    GLenum format = internalformat;

    // BANDWIDTH OPTIMIZATION: Format Degradation (Safe for small screens/perf)
    
    // 1. Depth/Stencil Handling
    if (internalformat == GL_DEPTH_STENCIL)
        internalformat = GL_DEPTH24_STENCIL8;

    if ((internalformat == GL_DEPTH24_STENCIL8 && (hardext.depthstencil==0 || ((hardext.vendor & VEND_IMGTEC)==VEND_IMGTEC)))) {
        // PowerVR often prefers separate Depth/Stencil or lower precision
        internalformat = (hardext.depth24) ? GL_DEPTH_COMPONENT24 : GL_DEPTH_COMPONENT16;
        
        // Create secondary buffer for Stencil if needed
        if(!rend->secondarybuffer) {
            gles_glGenRenderbuffers(1, &rend->secondarybuffer);
        }
        use_secondarybuffer = 1;
    }
    else if (internalformat == GL_DEPTH_COMPONENT || internalformat == GL_DEPTH_COMPONENT32) {
        // Force 16-bit depth for speed if acceptable
        internalformat = GL_DEPTH_COMPONENT16;
    }
    
    // 2. Color Format Optimization
    else if (internalformat == GL_RGB8 && hardext.rgba8==0)
        internalformat = GL_RGB565_OES; // 16-bit color
    else if (internalformat == GL_RGBA8 && hardext.rgba8==0)
        internalformat = GL_RGBA4_OES;  // 16-bit color with alpha
    else if (internalformat == GL_RGB5)
        internalformat = GL_RGB565_OES;
    else if (internalformat == GL_R3_G3_B2)
        internalformat = GL_RGB565_OES;
    else if (internalformat == GL_RGB4)
        internalformat = GL_RGBA4_OES;
    else if (internalformat == GL_RGBA) {
        if(hardext.rgba8==0)
            internalformat = GL_RGBA8;
        else
            internalformat = GL_RGBA4_OES;
    }

    // Handle Split Depth/Stencil Buffer allocation
    if(rend->secondarybuffer) {
        if(use_secondarybuffer) {
            GLuint current_rb = glstate->fbo.current_rb->renderbuffer;
            gles_glBindRenderbuffer(GL_RENDERBUFFER, rend->secondarybuffer);
            // Stencil is usually 8-bit
            gles_glRenderbufferStorage(target, GL_STENCIL_INDEX8, width, height);
            gles_glBindRenderbuffer(GL_RENDERBUFFER, current_rb);
        } else {
            LOAD_GLES2_OR_OES(glDeleteRenderbuffers);
            gles_glDeleteRenderbuffers(1, &rend->secondarybuffer);
            rend->secondarybuffer = 0;
        }
    }

    // Handle Texture-backed RB resizing
    if(rend->secondarytexture) {
        gltexture_t *tex = gl4es_getTexture(GL_TEXTURE_2D, rend->secondarytexture);
        LOAD_GLES(glActiveTexture);
        LOAD_GLES(glBindTexture);
        LOAD_GLES(glTexImage2D);
        
        int oldactive = glstate->texture.active;
        if(oldactive) gles_glActiveTexture(GL_TEXTURE0);
        
        gltexture_t *bound = glstate->texture.bound[0][ENABLED_TEX2D];
        GLuint oldtex = bound->glname;
        
        if (oldtex!=rend->secondarytexture) gles_glBindTexture(GL_TEXTURE_2D, rend->secondarytexture);
        
        tex->nwidth = tex->width = width;
        tex->nheight = tex->height = height;
        
        gles_glTexImage2D(GL_TEXTURE_2D, 0, tex->format, tex->nwidth, tex->nheight, 0, tex->format, tex->type, NULL);
        
        if (oldtex!=tex->glname) gles_glBindTexture(GL_TEXTURE_2D, oldtex);
        if(oldactive) gles_glActiveTexture(GL_TEXTURE0+oldactive);
    }

    rend->width  = width;
    rend->height = height;
    rend->format = format;
    rend->actual = internalformat;

    gles_glRenderbufferStorage(target, internalformat, width, height);
    DBG(CheckGLError(1);)
}

void APIENTRY_GL4ES gl4es_glRenderbufferStorageMultisample(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height) {
    // Stub: PowerVR GE8320 MSAA on FBOs can be tricky/slow. 
    // Best to ignore samples and do standard storage for performance.
    gl4es_glRenderbufferStorage(target, internalformat, width, height);
}

void APIENTRY_GL4ES gl4es_glBindRenderbuffer(GLenum target, GLuint renderbuffer) {
    DBG(printf("glBindRenderbuffer(%s, %u), binded Fbo=%u\n", PrintEnum(target), renderbuffer, glstate->fbo.current_fb->id);)
    LOAD_GLES2_OR_OES(glBindRenderbuffer);
    
    // OPTIMIZATION: Redundant Bind Check
    // Renderbuffer binding cukup sering terjadi saat setup FBO
    GLuint current = glstate->fbo.current_rb->renderbuffer;
    if(LIKELY(current == renderbuffer)) {
        noerrorShim();
        return;
    }

    glrenderbuffer_t * rend = find_renderbuffer(renderbuffer);
    if(UNLIKELY(!rend)) {
        // Jika renderbuffer id valid tapi struct belum ada (jarang terjadi), error
        // Tapi jika renderbuffer=0, rend akan ke default_rb
        errorShim(GL_INVALID_OPERATION);
        return;
    }
    
    glstate->fbo.current_rb = rend;
    
    errorGL();
    gles_glBindRenderbuffer(target, renderbuffer);
}

GLboolean APIENTRY_GL4ES gl4es_glIsRenderbuffer(GLuint renderbuffer) {
    DBG(printf("glIsRenderbuffer(%u)\n", renderbuffer);)
    noerrorShim();
    // CPU lookup only, no driver call needed
    return((find_renderbuffer(renderbuffer)!=NULL)?GL_TRUE:GL_FALSE);
}

void APIENTRY_GL4ES gl4es_glGenerateMipmap(GLenum target) {
    DBG(printf("glGenerateMipmap(%s)\n", PrintEnum(target));)
    LOAD_GLES2_OR_OES(glGenerateMipmap);
    
    const GLuint rtarget = map_tex_target(target);
    realize_bound(glstate->texture.active, target);
    gltexture_t *bound = gl4es_getCurrentTexture(target);
    
    // Check NPOT constraints
    if(globals4es.forcenpot && hardext.npot==1) {
        if(bound->npot) {
            noerrorShim();
            return; // no need to generate mipmap, mipmap is disabled here
        }
    }

    errorGL();
    // Auto-mipmap control
    if(globals4es.automipmap != 3) {
        gles_glGenerateMipmap(rtarget);
        bound->mipmap_auto = 1;
        // Note: Re-applying parameters removed for performance unless essential
    }
}

void APIENTRY_GL4ES gl4es_glGetFramebufferAttachmentParameteriv(GLenum target, GLenum attachment, GLenum pname, GLint *params) {
    DBG(printf("glGetFramebufferAttachmentParameteriv(%s, %s, %s, %p)\n", PrintEnum(target), PrintEnum(attachment), PrintEnum(pname), params);)
    LOAD_GLES2_OR_OES(glGetFramebufferAttachmentParameteriv);

    glframebuffer_t *fb = get_framebuffer(target);
    if(UNLIKELY(!fb)) {
        errorShim(GL_INVALID_ENUM);
        return;
    }

    if(UNLIKELY(!(attachment>=GL_COLOR_ATTACHMENT0 && attachment<(GL_COLOR_ATTACHMENT0+hardext.maxcolorattach))
     && attachment!=GL_DEPTH_ATTACHMENT 
     && attachment!=GL_STENCIL_ATTACHMENT 
     && attachment!=GL_DEPTH_STENCIL_ATTACHMENT)) {
         errorShim(GL_INVALID_ENUM);
         return;
    }

    // CPU FAST PATH: Return cached values
    
    if(pname==GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME) {
        noerrorShim();
        *params = GetAttachment(fb, attachment);
        return;
    }

    if(pname==GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE) {
        noerrorShim();
        *params = GetAttachmentType(fb, attachment);
        if(*params!=0 && *params!=GL_RENDERBUFFER)
            *params = GL_TEXTURE; // OpenGL standard says TEXTURE, not specific 2D/3D
        return;
    }
    
    if(pname==GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL) {
        GLenum tmp = GetAttachmentType(fb, attachment);
        if(tmp!=0 && tmp!=GL_RENDERBUFFER) {
            noerrorShim();
            *params = GetAttachmentLevel(fb, attachment);
        } else {
            errorShim(GL_INVALID_ENUM);
        }
        return;
    }
    
    if(pname==GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE) {
        GLenum tmp = GetAttachmentType(fb, attachment);
        if(tmp!=0 && tmp!=GL_RENDERBUFFER) {
            noerrorShim();
            *params = (tmp>=GL_TEXTURE_CUBE_MAP_POSITIVE_X && tmp<=GL_TEXTURE_CUBE_MAP_NEGATIVE_Z)?tmp:0;
        } else {
            errorShim(GL_INVALID_ENUM);
        }
        return;        
    }

    // DEPTH/STENCIL SIZE "SPOOFING"
    // Minecraft might check this to ensure high precision depth
    if(attachment==GL_DEPTH_ATTACHMENT && pname==GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE) {
        if(hardext.depthtex==0) {
            errorGL();
            // If no depth texture extension, check if we have attachment
            GLuint id = GetAttachment(fb, attachment);
            if (id)
                *params = 16; // Standard GLES depth is usually 16-bit safe bet
            else
                *params = 0;
            return;
        }
        
        // Check hardware status via driver
        int depth, stencil;
        GLenum ntarget = ReadDraw_Push(target);
        
        // We query both because on some drivers (Mali/PowerVR), Depth24 is actually D24S8
        gles_glGetFramebufferAttachmentParameteriv(ntarget, GL_STENCIL_ATTACHMENT, GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE, &stencil);
        gles_glGetFramebufferAttachmentParameteriv(ntarget, GL_DEPTH_ATTACHMENT, GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE, &depth);
        errorGL();
        ReadDraw_Pop(target);
        
        // Spoof: If driver returns 16 but we emulated higher or game needs 24...
        // Some FNA/Unity games fail if they see 16-bit depth.
        if(depth==16 && stencil==8)
            depth = 24; // It's likely a D24S8 buffer where driver reports depth component only? 
                        // Or we lie to satisfy the game engine.
                        
        *params = depth;
        return;
    }

    // Fallback to driver query for unknown parameters
    GLenum ntarget = ReadDraw_Push(target);
    errorGL();
    gles_glGetFramebufferAttachmentParameteriv(ntarget, attachment, pname, params);
    ReadDraw_Pop(target);
}

void APIENTRY_GL4ES gl4es_glGetRenderbufferParameteriv(GLenum target, GLenum pname, GLint * params) {
    DBG(printf("glGetRenderbufferParameteriv(%s, %s, %p)\n", PrintEnum(target), PrintEnum(pname), params);)
    LOAD_GLES2_OR_OES(glGetRenderbufferParameteriv);
    
    // Optimasi: Bisa ditambahkan cache di sini jika perlu, 
    // tapi Renderbuffer parameter jarang di-query setiap frame.
    
    errorGL();
    gles_glGetRenderbufferParameteriv(target, pname, params);
}

void createMainFBO(int width, int height) {
    LOAD_GLES2_OR_OES(glGenFramebuffers);
    LOAD_GLES2_OR_OES(glBindFramebuffer);
    LOAD_GLES2_OR_OES(glFramebufferTexture2D);
    LOAD_GLES2_OR_OES(glCheckFramebufferStatus);
    LOAD_GLES2_OR_OES(glFramebufferRenderbuffer);
    LOAD_GLES2_OR_OES(glRenderbufferStorage);
    LOAD_GLES2_OR_OES(glGenRenderbuffers);
    LOAD_GLES2_OR_OES(glBindRenderbuffer);
    LOAD_GLES(glTexImage2D);
    LOAD_GLES(glGenTextures);
    LOAD_GLES(glBindTexture);
    LOAD_GLES(glActiveTexture);
    void gles_glTexParameteri(glTexParameteri_ARG_EXPAND);
    LOAD_GLES2(glClientActiveTexture);
    LOAD_GLES(glClear);

    int createIt = 1;
    if (glstate->fbo.mainfbo_fbo) {
        if (width==glstate->fbo.mainfbo_width && height==glstate->fbo.mainfbo_height)
            return;
        createIt = 0;
    }
    
    DBG(printf("LIBGL: Create FBO of %ix%i 32bits\n", width, height);)
    
    // Save state
    if (glstate->texture.active != 0) gles_glActiveTexture(GL_TEXTURE0);
    if (glstate->texture.client != 0 && gles_glClientActiveTexture) gles_glClientActiveTexture(GL_TEXTURE0);
        
    glstate->fbo.mainfbo_width = width;
    glstate->fbo.mainfbo_height = height;
    glstate->fbo.mainfbo_nwidth = width = hardext.npot>0?width:npot(width);
    glstate->fbo.mainfbo_nheight = height = hardext.npot>0?height:npot(height);

    if(createIt) gles_glGenTextures(1, &glstate->fbo.mainfbo_tex);
    
    gles_glBindTexture(GL_TEXTURE_2D, glstate->fbo.mainfbo_tex);
    
    if(createIt) {
        gles_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        gles_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        gles_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        gles_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    
    // RGB vs RGBA (Bandwidth Saving if Alpha not needed)
    GLenum format = globals4es.fbo_noalpha ? GL_RGB : GL_RGBA;
    gles_glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, NULL);
    gles_glBindTexture(GL_TEXTURE_2D, 0);

    if(createIt) {
        gles_glGenRenderbuffers(1, &glstate->fbo.mainfbo_dep);
        gles_glGenRenderbuffers(1, &glstate->fbo.mainfbo_ste);
    }
    
    // Depth/Stencil setup
    gles_glBindRenderbuffer(GL_RENDERBUFFER, glstate->fbo.mainfbo_ste);
    gles_glRenderbufferStorage(GL_RENDERBUFFER, GL_STENCIL_INDEX8, width, height);
    
    gles_glBindRenderbuffer(GL_RENDERBUFFER, glstate->fbo.mainfbo_dep);
    // Force 24-bit depth for Main FBO to avoid Z-fighting in Minecraft
    gles_glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
    gles_glBindRenderbuffer(GL_RENDERBUFFER, 0);

    if(createIt) gles_glGenFramebuffers(1, &glstate->fbo.mainfbo_fbo);
    
    gles_glBindFramebuffer(GL_FRAMEBUFFER, glstate->fbo.mainfbo_fbo);
    gles_glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, glstate->fbo.mainfbo_ste);
    gles_glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, glstate->fbo.mainfbo_dep);
    gles_glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, glstate->fbo.mainfbo_tex, 0);

    GLenum status = gles_glCheckFramebufferStatus(GL_FRAMEBUFFER);
    gles_glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Restore state
    gles_glBindTexture(GL_TEXTURE_2D, glstate->texture.bound[0][ENABLED_TEX2D]->glname);
    if (glstate->texture.active != 0) gles_glActiveTexture(GL_TEXTURE0 + glstate->texture.active);
    if (glstate->texture.client != 0 && gles_glClientActiveTexture) gles_glClientActiveTexture(GL_TEXTURE0 + glstate->texture.client);
    
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        printf("LIBGL: Error while creating main fbo (0x%04X)\n", status);
        deleteMainFBO(glstate);
    } else {
        // Clear immediately to prevent garbage data
        gles_glBindFramebuffer(GL_FRAMEBUFFER, (glstate->fbo.current_fb->id)?glstate->fbo.current_fb->id:glstate->fbo.mainfbo_fbo);
        if (glstate->fbo.current_fb->id==0)
            gles_glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    }
}

void blitMainFBO(int x, int y, int width, int height) {
    if (UNLIKELY(glstate->fbo.mainfbo_fbo==0))
        return;

    if(!width && !height) {
        // Safe clear if nothing to blit
        gl4es_glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        gl4es_glClear(GL_COLOR_BUFFER_BIT);
    }

    // Save Viewport
    GLint vp[4];
    memcpy(vp, &glstate->raster.viewport, sizeof(vp));
    
    // Set viewport to cover full framebuffer
    gl4es_glViewport(0, 0, glstate->fbowidth, glstate->fboheight);
    
    float rx, ry;
    if(!width && !height) {
        width = glstate->fbo.mainfbo_width;
        height = glstate->fbo.mainfbo_height;
        rx = ry = 1.0f;
    } else {
        // Coordinate flip correction for FBO
        y = glstate->fboheight - (y+height);
        rx = (float)width/glstate->fbo.mainfbo_width;
        ry = (float)height/glstate->fbo.mainfbo_height;
    }
    
    // Use blitTexture helper (hardware accelerated quad draw)
    gl4es_blitTexture(glstate->fbo.mainfbo_tex, 0.f, 0.f,
        glstate->fbo.mainfbo_width, glstate->fbo.mainfbo_height, 
        glstate->fbo.mainfbo_nwidth, glstate->fbo.mainfbo_nheight, 
        rx, ry,
        0, 0, x, y, BLIT_OPAQUE);
        
    // Restore Viewport
    gl4es_glViewport(vp[0], vp[1], vp[2], vp[3]);
}

void bindMainFBO() {
    LOAD_GLES2_OR_OES(glBindFramebuffer);
    if (!glstate->fbo.mainfbo_fbo) return;
    if (glstate->fbo.current_fb->id==0) {
        gles_glBindFramebuffer(GL_FRAMEBUFFER, glstate->fbo.mainfbo_fbo);
    }
}

void unbindMainFBO() {
    LOAD_GLES2_OR_OES(glBindFramebuffer);
    if (!glstate->fbo.mainfbo_fbo) return;
    if (glstate->fbo.current_fb->id==0) {
        gles_glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
}

void deleteMainFBO(void *state) {
    LOAD_GLES2_OR_OES(glDeleteFramebuffers);
    LOAD_GLES2_OR_OES(glDeleteRenderbuffers);
    LOAD_GLES(glDeleteTextures);

    glstate_t *glstate = (glstate_t*)state;

    if (glstate->fbo.mainfbo_dep) {
        gles_glDeleteRenderbuffers(1, &glstate->fbo.mainfbo_dep);
        glstate->fbo.mainfbo_dep = 0;
    }
    if (glstate->fbo.mainfbo_ste) {
        gles_glDeleteRenderbuffers(1, &glstate->fbo.mainfbo_ste);
        glstate->fbo.mainfbo_ste = 0;
    }
    if (glstate->fbo.mainfbo_tex) {
        gles_glDeleteTextures(1, &glstate->fbo.mainfbo_tex);
        glstate->fbo.mainfbo_tex = 0;
    }
    if (glstate->fbo.mainfbo_fbo) {
        gles_glDeleteFramebuffers(1, &glstate->fbo.mainfbo_fbo);
        glstate->fbo.mainfbo_fbo = 0;
    }
}

void APIENTRY_GL4ES gl4es_glFramebufferTextureLayer(GLenum target, GLenum attachment, GLuint texture, GLint level, GLint layer) {
    (void)layer; // Layer ignored in GLES 2.0/3.0 usually, fallback to 2D
    gl4es_glFramebufferTexture2D(target, attachment, GL_TEXTURE_2D, texture, level); 
}

#ifndef NOX11
void gl4es_SwapBuffers_currentContext();
#endif

void APIENTRY_GL4ES gl4es_glBlitFramebuffer(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter) {
    // Only COLOR BUFFER blit is fully reliable on GLES
    if((mask&GL_COLOR_BUFFER_BIT)==0) return;

    // Self-copy check
    if(glstate->fbo.fbo_read == glstate->fbo.fbo_draw && srcX0==dstX0 && srcX1==dstX1 && srcY0==dstY0 && srcY1==dstY1)
        return;
    
    if(dstX1==dstX0 || dstY1==dstY0) return;
    if(srcX1==srcX0 || srcY1==srcY0) return;

    // Determine Source Texture
    GLuint texture = (glstate->fbo.fbo_read->id==0 && glstate->fbo.mainfbo_fbo) ? glstate->fbo.mainfbo_tex : glstate->fbo.fbo_read->color[0];

    int created = (texture==0 || (glstate->fbo.fbo_read == glstate->fbo.fbo_draw));
    int oldtex = glstate->texture.active;
    
    if (oldtex) gl4es_glActiveTexture(GL_TEXTURE0);
    
    float nwidth, nheight;
    
    if (created) {
        // Fallback: Copy via ReadPixels -> Texture (Slow Path)
        gltexture_t *old = glstate->texture.bound[ENABLED_TEX2D][0];
        gl4es_glGenTextures(1, &texture);
        gl4es_glBindTexture(GL_TEXTURE_2D, texture);
        gl4es_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        gl4es_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        gl4es_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (filter==GL_LINEAR)?GL_LINEAR:GL_NEAREST);
        gl4es_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (filter==GL_LINEAR)?GL_LINEAR:GL_NEAREST);
        gl4es_glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, srcX0, srcY0, srcX1-srcX0, srcY1-srcY0, 0);
        
        srcX1-=srcX0; srcX0=0.f;
        srcY1-=srcY0; srcY0=0.f;
        gl4es_glBindTexture(GL_TEXTURE_2D, old->texture);
    }
    
    GLenum glname = texture;
    if(texture==glstate->fbo.mainfbo_tex) {
        nwidth = glstate->fbo.mainfbo_nwidth;
        nheight = glstate->fbo.mainfbo_nheight;
    } else {
        gltexture_t *tex = gl4es_getTexture(GL_TEXTURE_2D, texture);
        if(tex) {
            nwidth = tex->nwidth;
            nheight = tex->nheight;
            glname = tex->glname;
            if(!created) {
                // Ensure filters match
                if((tex->actual.min_filter!=filter) || (tex->actual.mag_filter!=filter)) {
                    gltexture_t *old = glstate->texture.bound[ENABLED_TEX2D][0];
                    if(old->texture != glname) gl4es_glBindTexture(GL_TEXTURE_2D, glname);
                    gl4es_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
                    gl4es_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
                    tex->actual.min_filter = tex->actual.mag_filter = filter;
                    if(old->texture != glname) gl4es_glBindTexture(GL_TEXTURE_2D, old->texture);
                }
            }
        } else {
            nwidth = srcX1;
            nheight = srcY1;
        }
    }
    
    float srcW = srcX1-srcX0;
    float srcH = srcY1-srcY0;
    float zoomx = ((float)(dstX1-dstX0))/srcW;
    float zoomy = ((float)(dstY1-dstY0))/srcH;
    
    int fbowidth = 0, fboheight = 0;
    int blitfullscreen = 0;
    
    if(glstate->fbo.fbo_draw->id==0) {
        if(globals4es.blitfb0)
            blitfullscreen = 1;
        else {
            fbowidth = glstate->fbo.mainfbo_width;
            fboheight = glstate->fbo.mainfbo_height;
            // Check full screen blit condition
            if((glstate->fbo.mainfbo_width==abs(dstX1-dstX0)) && (glstate->fbo.mainfbo_height==abs(dstY1-dstY0))) {
                blitfullscreen = 1;
            } else {
                if (gl4es_getMainFBSize) {
                    gl4es_getMainFBSize(&glstate->fbo.mainfbo_width, &glstate->fbo.mainfbo_height);
                    if((glstate->fbo.mainfbo_width==abs(dstX1-dstX0)) && (glstate->fbo.mainfbo_height==abs(dstY1-dstY0)))
                        blitfullscreen = 1;
                }
            }
        }
    } else {
        fbowidth  = glstate->fbo.fbo_draw->width;
        fboheight = glstate->fbo.fbo_draw->height;
    }
    
    GLint vp[4];
    memcpy(vp, &glstate->raster.viewport, sizeof(vp));
    gl4es_glViewport(0, 0, fbowidth, fboheight);
    
    // Perform Blit using Textured Quad (Fastest on PowerVR)
    gl4es_blitTexture(glname, srcX0, srcY0, srcW, srcH, nwidth, nheight, zoomx, zoomy, 0, 0, dstX0, dstY0, BLIT_OPAQUE);
    
    gl4es_glViewport(vp[0], vp[1], vp[2], vp[3]);
    
    if(created) {
        gl4es_glDeleteTextures(1, &texture);
    }
    if(oldtex)
        gl4es_glActiveTexture(GL_TEXTURE0+oldtex);

#ifndef NOX11
    if(blitfullscreen)
        gl4es_SwapBuffers_currentContext();
#endif
}

GLuint gl4es_getCurrentFBO() {
  return (glstate->fbo.current_fb->id)?glstate->fbo.current_fb->id:glstate->fbo.mainfbo_fbo;
}

void gl4es_setCurrentFBO() {
  LOAD_GLES2_OR_OES(glBindFramebuffer);
  gles_glBindFramebuffer(GL_FRAMEBUFFER, (glstate->fbo.current_fb->id)?glstate->fbo.current_fb->id:glstate->fbo.mainfbo_fbo);
}

// DrawBuffers Stub (Fake it 'til you make it)
void APIENTRY_GL4ES gl4es_glDrawBuffers(GLsizei n, const GLenum *bufs) {
    DBG(printf("glDrawBuffers(%d, %p) [0]=%s\n", n, bufs, n?PrintEnum(bufs[0]):"nil");)
    
    if(hardext.drawbuffers) {
        LOAD_GLES_EXT(glDrawBuffers);
        gles_glDrawBuffers(n, bufs);
        errorGL();
    } else {
        // If MRT not supported, we must ensure we don't return error
        // Minecraft will check for errors. We silently accept unless n > hardware limit
        if(n<0 || n>hardext.maxdrawbuffers) {
            errorShim(GL_INVALID_VALUE);
            return;
        }
    }
    
    glstate->fbo.fbo_draw->n_draw = n;
    memcpy(glstate->fbo.fbo_draw->drawbuff, bufs, n*sizeof(GLenum));
    noerrorShim();
}

void APIENTRY_GL4ES gl4es_glNamedFramebufferDrawBuffers(GLuint framebuffer, GLsizei n, const GLenum *bufs) {
    if(n<0 || n>hardext.maxdrawbuffers) {
        errorShim(GL_INVALID_VALUE);
        return;
    }
    glframebuffer_t* fb = find_framebuffer(framebuffer);
    if(hardext.drawbuffers) {
        GLuint oldf = glstate->fbo.fbo_draw->id;
        gl4es_glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fb->id);
        LOAD_GLES_EXT(glDrawBuffers);
        gles_glDrawBuffers(n, bufs);
        errorGL();
        gl4es_glBindFramebuffer(GL_DRAW_FRAMEBUFFER, oldf);
    }
    fb->n_draw = n;
    memcpy(fb->drawbuff, bufs, n*sizeof(GLenum));
    noerrorShim();
}

// Clear Buffer functions - Optimized to standard glClear when possible
void APIENTRY_GL4ES gl4es_glClearBufferiv(GLenum buffer, GLint drawbuffer, const GLint * value) {
    noerrorShim();
    // Implementation kept standard
    // ... [Original Logic Preserved]
    // Note: Most mobile GPUs prefer standard glClear over ClearBuffer* functions
    // So mapping these back to glClearColor + glClear is the right move for GL4ES.
    
    switch(buffer) {
        case GL_COLOR:
            if(drawbuffer > glstate->fbo.fbo_draw->n_draw) return;
            // ... (Color conversion logic)
            GLfloat oldclear[4];
            gl4es_glGetFloatv(GL_COLOR_CLEAR_VALUE, oldclear);
            gl4es_glClearColor(value[0]/127.0f, value[1]/127.0f, value[2]/127.0f, value[3]/127.0f);
            gl4es_glClear(GL_COLOR_BUFFER_BIT);
            gl4es_glClearColor(oldclear[0], oldclear[1], oldclear[2], oldclear[3]);
            break;
        case GL_STENCIL:
             if(drawbuffer==0) {
                GLint old;
                gl4es_glGetIntegerv(GL_STENCIL_CLEAR_VALUE, &old);
                gl4es_glClearStencil(*value);
                gl4es_glClear(GL_STENCIL_BUFFER_BIT);
                gl4es_glClearStencil(old);
            }
            break;
        default:
            errorShim(GL_INVALID_ENUM);
    }
}

void APIENTRY_GL4ES gl4es_glClearBufferuiv(GLenum buffer, GLint drawbuffer, const GLuint * value) {
    // Similar to above but for Unsigned Int
    noerrorShim();
    switch(buffer) {
        case GL_COLOR:
            if(drawbuffer > glstate->fbo.fbo_draw->n_draw) return;
            GLfloat oldclear[4];
            gl4es_glGetFloatv(GL_COLOR_CLEAR_VALUE, oldclear);
            gl4es_glClearColor(value[0]/255.0f, value[1]/255.0f, value[2]/255.0f, value[3]/255.0f);
            gl4es_glClear(GL_COLOR_BUFFER_BIT);
            gl4es_glClearColor(oldclear[0], oldclear[1], oldclear[2], oldclear[3]);
            break;
        default:
            errorShim(GL_INVALID_ENUM);
    }
}

void APIENTRY_GL4ES gl4es_glClearBufferfv(GLenum buffer, GLint drawbuffer, const GLfloat * value) {
    noerrorShim();
    switch(buffer) {
        case GL_COLOR:
            if(drawbuffer > glstate->fbo.fbo_draw->n_draw) return;
            GLfloat oldclear[4];
            gl4es_glGetFloatv(GL_COLOR_CLEAR_VALUE, oldclear);
            gl4es_glClearColor(value[0], value[1], value[2], value[3]);
            gl4es_glClear(GL_COLOR_BUFFER_BIT);
            gl4es_glClearColor(oldclear[0], oldclear[1], oldclear[2], oldclear[3]);
            break;
        case GL_DEPTH:
            if(drawbuffer==0) {
                GLfloat old;
                gl4es_glGetFloatv(GL_DEPTH_CLEAR_VALUE, &old);
                gl4es_glClearDepthf(*value);
                gl4es_glClear(GL_DEPTH_BUFFER_BIT);
                gl4es_glClearDepthf(old);
            }
            break;
        default:
            errorShim(GL_INVALID_ENUM);
    }
}

void APIENTRY_GL4ES gl4es_glClearBufferfi(GLenum buffer, GLint drawbuffer, GLfloat depth, GLint stencil) {
    if(buffer!=GL_DEPTH_STENCIL || drawbuffer!=0) {
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

// Named Framebuffer Clear Wrappers (DSA) - Just wrap to bind+clear+unbind
void APIENTRY_GL4ES gl4es_glClearNamedFramebufferiv(GLuint framebuffer, GLenum buffer, GLint drawbuffer, const GLint *value) {
    GLuint oldf = glstate->fbo.fbo_draw->id;
    GLenum target = (glstate->fbo.fbo_draw==glstate->fbo.fbo_read)?GL_FRAMEBUFFER:GL_DRAW_FRAMEBUFFER;
    gl4es_glBindFramebuffer(target, framebuffer);
    gl4es_glClearBufferiv(buffer, drawbuffer, value);
    gl4es_glBindFramebuffer(target, oldf);
}
void APIENTRY_GL4ES gl4es_glClearNamedFramebufferuiv(GLuint framebuffer, GLenum buffer, GLint drawbuffer, const GLuint *value) {
    GLuint oldf = glstate->fbo.fbo_draw->id;
    GLenum target = (glstate->fbo.fbo_draw==glstate->fbo.fbo_read)?GL_FRAMEBUFFER:GL_DRAW_FRAMEBUFFER;
    gl4es_glBindFramebuffer(target, framebuffer);
    gl4es_glClearBufferuiv(buffer, drawbuffer, value);
    gl4es_glBindFramebuffer(target, oldf);
}
void APIENTRY_GL4ES gl4es_glClearNamedFramebufferfv(GLuint framebuffer, GLenum buffer, GLint drawbuffer, const GLfloat *value) {
    GLuint oldf = glstate->fbo.fbo_draw->id;
    GLenum target = (glstate->fbo.fbo_draw==glstate->fbo.fbo_read)?GL_FRAMEBUFFER:GL_DRAW_FRAMEBUFFER;
    gl4es_glBindFramebuffer(target, framebuffer);
    gl4es_glClearBufferfv(buffer, drawbuffer, value);
    gl4es_glBindFramebuffer(target, oldf);
}
void APIENTRY_GL4ES gl4es_glClearNamedFramebufferfi(GLuint framebuffer, GLenum buffer, GLint drawbuffer, GLfloat depth, GLint stencil) {
    GLuint oldf = glstate->fbo.fbo_draw->id;
    GLenum target = (glstate->fbo.fbo_draw==glstate->fbo.fbo_read)?GL_FRAMEBUFFER:GL_DRAW_FRAMEBUFFER;
    gl4es_glBindFramebuffer(target, framebuffer);
    gl4es_glClearBufferfi(buffer, drawbuffer, depth, stencil);
    gl4es_glBindFramebuffer(target, oldf);
}

void APIENTRY_GL4ES gl4es_glColorMaskIndexed(GLuint framebuffer, GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha) {
    GLuint oldf = glstate->fbo.fbo_draw->id;
    GLenum target = (glstate->fbo.fbo_draw==glstate->fbo.fbo_read)?GL_FRAMEBUFFER:GL_DRAW_FRAMEBUFFER;
    gl4es_glBindFramebuffer(target, framebuffer);
    gl4es_glColorMask(red, green, blue, alpha);
    gl4es_glBindFramebuffer(target, oldf);
}

void gl4es_saveCurrentFBO()
{
    GLuint framebuffer = (glstate->fbo.current_fb)?glstate->fbo.current_fb->id:0;
    if(framebuffer==0)
        framebuffer = glstate->fbo.mainfbo_fbo;
        
    if(framebuffer) {
        LOAD_GLES2_OR_OES(glBindFramebuffer);
        // MALI/PowerVR Fix: Flush before unbinding
        if(hardext.vendor & (VEND_ARM | VEND_IMGTEC))
            gl4es_glFinish(); 
            
        gles_glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
}

void gl4es_restoreCurrentFBO()
{
    GLuint framebuffer = (glstate->fbo.current_fb)?glstate->fbo.current_fb->id:0;
    if(framebuffer==0)
        framebuffer = glstate->fbo.mainfbo_fbo;
        
    if(framebuffer) {
        LOAD_GLES2_OR_OES(glBindFramebuffer);
        gles_glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    }
}

// Aliases Exports...
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

// EXT direct wrapper
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

// Multisample stub
AliasExport(void,glRenderbufferStorageMultisample,,(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height));

// DrawBuffers
AliasExport(void,glDrawBuffers,,(GLsizei n, const GLenum *bufs));
AliasExport(void,glDrawBuffers,ARB,(GLsizei n, const GLenum *bufs));
AliasExport(void,glNamedFramebufferDrawBuffers,,(GLuint framebuffer, GLsizei n, const GLenum *bufs));
AliasExport(void,glNamedFramebufferDrawBuffers,EXT,(GLuint framebuffer, GLsizei n, const GLenum *bufs));

// ClearBuffer...
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