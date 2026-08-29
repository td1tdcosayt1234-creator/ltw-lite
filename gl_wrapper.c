/*
 * gl_wrapper.c -- the GL -> GLES "thin wrapper" dispatch layer.
 *
 * Structural skeleton of how glsl_translate() plugs into a desktop-OpenGL
 * on-GLES backend like LTW. The translator already rewrites shaders so mods
 * like Create get valid GLSL ES 3.00 (no more "vertex error"). This file
 * forwards the remaining desktop-GL entry points to their GLES equivalents
 * and emulates the few things GLES 3.0 lacks.
 *
 * Build on an Android/embedded target with:
 *     gcc -DLTW_HAVE_GLES -lGLESv2 -lEGL gl_wrapper.c glsl_translate.c
 */
#ifdef LTW_HAVE_GLES

#include <GLES3/gl32.h>
#include <GLES3/gl3ext.h>
#include "glsl_translate.h"

/* ---- shader objects: run sources through the translator ---- */

static GLuint gles_compile(GLenum gl_type, const char *src, glsl_stage stage) {
    char *es_src = glsl_translate(src, stage);
    GLenum es_type = (gl_type == GL_VERTEX_SHADER) ? GL_VERTEX_SHADER : GL_FRAGMENT_SHADER;
    GLuint sh = glCreateShader(es_type);
    const char *p = es_src;
    glShaderSource(sh, 1, &p, NULL);
    glCompileShader(sh);
    free(es_src);

    GLint ok = 0;
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        /* this is exactly where Create used to crash with "vertex error" */
        char log[8192];
        glGetShaderInfoLog(sh, sizeof(log), NULL, log);
        fprintf(stderr, "LTW: shader compile failed:\n%s\n", log);
    }
    return sh;
}

void glShaderSourceARB(GLuint obj, GLsizei count, const char **src, const GLint *len) {
    (void)count; (void)len;
    /* stage tracked per-object in real LTW via a small table */
    glsl_stage stage = (obj & 1) ? STAGE_FRAGMENT : STAGE_VERTEX;
    char *es = glsl_translate(src[0], stage);
    glShaderSource(obj, 1, (const char **)&es, NULL);
    free(es);
}

/* ---- transform feedback / vertex pulling ----
 * Create relies on transform feedback in places; GLES 3.0 has it natively. */
void glBeginTransformFeedback(GLenum prim)      { glBeginTransformFeedback(prim); }
void glEndTransformFeedback(void)               { glEndTransformFeedback(); }
void glBindBufferBase(GLenum target, GLuint i, GLuint buf) { glBindBufferBase(target, i, buf); }

/* ---- vertex array objects (native in GLES 3.0) ---- */
void glGenVertexArrays(GLsizei n, GLuint *a)    { glGenVertexArrays(n, a); }
void glBindVertexArray(GLuint a)                { glBindVertexArray(a); }
void glDeleteVertexArrays(GLsizei n, GLuint *a) { glDeleteVertexArrays(n, a); }

/* ---- multiple render targets (native in GLES 3.0) ---- */
void glDrawBuffers(GLsizei n, const GLenum *bufs) { glDrawBuffers(n, bufs); }

/* ---- point size (native; just enable) ---- */
void glPointSize(GLfloat s) { glPointSize(s); }

/* ---- user clip planes (GLES 3.0 has NO clip planes) ----
 * Emulated in the shader: gl_ClipDistance[i] is rewritten to a varying and the
 * fragment shader discards fragments where it is < 0 (see glsl_translate).
 * The older glClipPlane()/GL_CLIP_PLANEi API writes a plane equation that the
 * real LTW would bake into the generated gl_ClipDistance expression. */
static float g_clip_planes[6][4];
static int   g_clip_enabled[6];

void glClipPlane(GLenum plane, const GLdouble *eq) {
    int i = plane - GL_CLIP_PLANE0;
    if (i < 0 || i > 5) return;
    for (int k = 0; k < 4; k++) g_clip_planes[i][k] = (float)eq[k];
    /* g_clip_enabled[i] is set by glEnable(GL_CLIP_PLANEi); the shader
     * generator injects: _clipDist[i] = dot(gl_Position, plane_eq); */
}

void glGetClipPlane(GLenum plane, GLdouble *eq) {
    int i = plane - GL_CLIP_PLANE0;
    if (i < 0 || i > 5) return;
    for (int k = 0; k < 4; k++) eq[k] = g_clip_planes[i][k];
}

#endif /* LTW_HAVE_GLES */
