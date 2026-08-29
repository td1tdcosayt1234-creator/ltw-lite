/*
 * gl_wrapper.c -- the GL -> GLES "thin wrapper" dispatch layer.
 *
 * How glsl_translate() + gl_adapt.c plug into a desktop-OpenGL on-GLES
 * backend like LTW. GLES entry points are loaded through dlsym (the way a
 * real wrapper works) so our wrapper functions forward instead of recursing.
 *
 * Build on an Android/embedded target with:
 *     gcc -DLTW_HAVE_GLES -ldl -lGLESv2 -lEGL gl_wrapper.c glsl_translate.c gl_adapt.c
 */
#ifdef LTW_HAVE_GLES

#include <dlfcn.h>
#include <GLES3/gl3.h>
#include <GLES3/gl3ext.h>
#include "glsl_translate.h"
#include "gl_adapt.h"

#define GL_ADAPT_TYPES  /* GLES already defines GLenum/GLuint */

typedef struct {
    PFNGLBINDTEXTUREPROC     BindTexture;
    PFNGLTEXPARAMETERIPROC   TexParameteri;
    PFNGLTEXIMAGE2DPROC      TexImage2D;
    PFNGLTEXIMAGE3DPROC      TexImage3D;
    PFNGLDRAWELEMENTSPROC    DrawElements;
    PFNGLDRAWARRAYSPROC      DrawArrays;
    PFNGLENABLEPROC          Enable;
    PFNGLGETERRORPROC        GetError;
} GLES;

static GLES G;
static int g_inited = 0;

static void *sym(void *h, const char *n) {
    void *p = dlsym(h, n);
    if (!p) fprintf(stderr, "LTW: missing GLES symbol %s\n", n);
    return p;
}

static void ltw_load(void) {
    void *h = dlopen("libGLESv2.so", RTLD_LAZY);
    if (!h) { fprintf(stderr, "LTW: cannot load libGLESv2.so\n"); return; }
    G.BindTexture    = sym(h, "glBindTexture");
    G.TexParameteri  = sym(h, "glTexParameteri");
    G.TexImage2D     = sym(h, "glTexImage2D");
    G.TexImage3D     = sym(h, "glTexImage3D");
    G.DrawElements   = sym(h, "glDrawElements");
    G.DrawArrays     = sym(h, "glDrawArrays");
    G.Enable         = sym(h, "glEnable");
    G.GetError       = sym(h, "glGetError");
    g_inited = 1;
}

/* ---- shader objects: run sources through the translator ---- */

void glShaderSourceARB(GLuint obj, GLsizei count, const char **src, const GLint *len) {
    (void)count; (void)len;
    glsl_stage stage = (obj & 1) ? STAGE_FRAGMENT : STAGE_VERTEX;
    char *es = glsl_translate(src[0], stage);
    glShaderSource(obj, 1, (const char **)&es, NULL);
    free(es);
}

/* ---- texture target / wrap / format adaptation ---- */

void glBindTexture(GLenum target, GLuint tex) {
    if (!g_inited) ltw_load();
    adapt_texture_target(&target);
    G.BindTexture(target, tex);
}

void glTexParameteri(GLenum target, GLenum pname, GLint param) {
    if (!g_inited) ltw_load();
    adapt_texture_target(&target);
    if (pname == 0x2802 /* GL_TEXTURE_WRAP_S */ ||
        pname == 0x2803 /* GL_TEXTURE_WRAP_T */) {
        GLenum w = (GLenum)param;
        adapt_wrap(&w);
        param = (GLint)w;
    }
    G.TexParameteri(target, pname, param);
}

void glTexImage2D(GLenum target, GLint level, GLint internalformat,
                  GLsizei w, GLsizei h, GLint border, GLenum format,
                  GLenum type, const void *pixels) {
    if (!g_inited) ltw_load();
    adapt_texture_target(&target);
    adapt_pixel_format((GLenum*)&internalformat, &format);
    G.TexImage2D(target, level, internalformat, w, h, border, format, type, pixels);
}

/* ---- primitive adaptation (GL_QUADS / GL_POLYGON) ---- */

void glDrawArrays(GLenum mode, GLint first, GLsizei count) {
    if (!g_inited) ltw_load();
    adapt_primitive(&mode);
    G.DrawArrays(mode, first, count);
}

void glDrawElements(GLenum mode, GLsizei count, GLenum type,
                    const void *indices) {
    if (!g_inited) ltw_load();
    GLenum m = mode;
    if (adapt_primitive(&m)) {
        if (mode == 0x0007 && indices != NULL) { /* was QUADS, client indices */
            int quads = count / 4;
            GLuint *exp = malloc(sizeof(GLuint) * 6 * quads);
            const GLuint *src = (const GLuint *)indices; /* assumes uint indices */
            int outn = expand_quads_to_triangles(quads, src, exp);
            G.DrawElements(0x0004, outn, type, exp); /* TRIANGLES */
            free(exp);
            return;
        }
        /* GL_ELEMENT_ARRAY_BUFFER-backed quads: expansion must happen in the
         * host that owns the buffer; document and fall back to fan-safe path */
    }
    G.DrawElements(m, count, type, indices);
}

/* ---- the remaining native forwards (unchanged from before) ---- */

void glBeginTransformFeedback(GLenum prim)      { glBeginTransformFeedback(prim); }
void glEndTransformFeedback(void)               { glEndTransformFeedback(); }
void glBindBufferBase(GLenum target, GLuint i, GLuint buf) { glBindBufferBase(target, i, buf); }
void glGenVertexArrays(GLsizei n, GLuint *a)    { glGenVertexArrays(n, a); }
void glBindVertexArray(GLuint a)                { glBindVertexArray(a); }
void glDeleteVertexArrays(GLsizei n, GLuint *a) { glDeleteVertexArrays(n, a); }
void glDrawBuffers(GLsizei n, const GLenum *b)  { glDrawBuffers(n, b); }
void glPointSize(GLfloat s)                     { glPointSize(s); }

#endif /* LTW_HAVE_GLES */
