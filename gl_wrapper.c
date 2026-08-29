/*
 * gl_wrapper.c -- the GL -> GLES "thin wrapper" dispatch layer.
 *
 * This is the structural skeleton of how the GLSL translator above
 * plugs into a desktop-OpenGL-on-GLES backend like LTW.
 *
 * It is NOT compiled in the default test build (no GLES/EGL dev libs
 * on this machine). Build it on an Android/embedded target with:
 *
 *     gcc -DLTW_HAVE_GLES -lGLESv2 -lEGL gl_wrapper.c glsl_translate.c
 *
 * The key idea: every desktop gl* call is forwarded to its GLES
 * equivalent. Shader sources are transparently run through
 * glsl_translate() so mods written for desktop GL (e.g. Create) get
 * valid GLSL ES 3.00 without the game knowing.
 */
#ifdef LTW_HAVE_GLES

#include <GLES3/gl32.h>
#include <GLES3/gl3ext.h>
#include "glsl_translate.h"

/* ---- shader objects: run sources through the translator ---- */

static GLuint gles_compile(GLenum gl_type, const char *src, glsl_stage stage) {
    char *es_src = glsl_translate(src, stage);
    GLuint sh = glCreateShader(gl_type == GL_VERTEX_SHADER ? GL_VERTEX_SHADER : GL_FRAGMENT_SHADER);
    const char *p = es_src;
    glShaderSource(sh, 1, &p, NULL);
    glCompileShader(sh);
    free(es_src);

    GLint ok = 0;
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        /* without translation this is exactly where Create crashes with
         * "vertex error" -- emitting the log helps diagnose mod shaders. */
        char log[4096];
        glGetShaderInfoLog(sh, sizeof(log), NULL, log);
        fprintf(stderr, "LTW: shader compile failed:\n%s\n", log);
    }
    return sh;
}

GLuint glCreateShaderObjectARB(GLenum type) { return glCreateShader(type); }

/* thin wrappers for the entry points LWJGL/Minecraft actually call */
void glShaderSourceARB(GLuint obj, GLsizei count, const char **src, const GLint *len) {
    (void)count; (void)len;
    /* upstream caller passes desktop GLSL; translate on the fly.
     * stage is tracked per-object in real LTW via a small table. */
    glsl_stage stage = (obj & 1) ? STAGE_FRAGMENT : STAGE_VERTEX; /* simplified */
    char *es = glsl_translate(src[0], stage);
    glShaderSource(obj, 1, (const char **)&es, NULL);
    free(es);
}

/* ---- transform feedback / vertex pulling ----
 * Create relies on vertex pulling + transform feedback in places.
 * GLES 3.0 has real transform feedback, so we forward it directly. */
void glBeginTransformFeedback(GLenum prim)        { glBeginTransformFeedback(prim); }
void glEndTransformFeedback(void)                 { glEndTransformFeedback(); }
void glBindBufferBase(GLenum target, GLuint idx, GLuint buf) { glBindBufferBase(target, idx, buf); }

/* ---- vertex array objects ----
 * GLES 3.0 has native VAOs, so these are direct forwards. */
void glGenVertexArrays(GLsizei n, GLuint *a)      { glGenVertexArrays(n, a); }
void glBindVertexArray(GLuint a)                  { glBindVertexArray(a); }
void glDeleteVertexArrays(GLsizei n, GLuint *a)   { glDeleteVertexArrays(n, a); }

#endif /* LTW_HAVE_GLES */
