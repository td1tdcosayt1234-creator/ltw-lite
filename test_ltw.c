#include "ltw_glue.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures = 0;
#define CK(c,m) do { if (c) printf("  [ok]   %s\n", m); else { printf("  [FAIL] %s\n", m); failures++; } } while(0)

/* Simulates LTW's glShaderSource() flow: concatenate the (possibly multi-part)
 * source, then translate exactly as the integration patch would. */
static char *concat(const char *a, const char *b) {
    size_t n = strlen(a) + strlen(b) + 1;
    char *s = malloc(n);
    snprintf(s, n, "%s%s", a, b);
    return s;
}

int main(void) {
    printf("=== LTW integration flow (glShaderSource seam) ===\n");

    /* A Create-style vertex shader: desktop GLSL with clip-cull-distance ext. */
    const char *vsrc_a = "#version 320 es\n"
                         "#extension GL_EXT_clip_cull_distance : require\n";
    const char *vsrc_b = "out float gl_ClipDistance[4];\n"
                         "void main() { gl_ClipDistance[0]=1.0; gl_Position=vec4(0.0); }\n";
    char *vsrc = concat(vsrc_a, vsrc_b);
    char *v = ltw_translate_shader(vsrc, 0x8B31); /* GL_VERTEX_SHADER */
    printf("%s\n", v);
    CK(strstr(v, "#version 300 es") != NULL, "vertex -> #version 300 es");
    CK(strstr(v, "GL_EXT_clip_cull_distance") == NULL, "vertex drops unsupported #extension");
    CK(strstr(v, "_clipDist[4]") != NULL, "vertex gl_ClipDistance -> _clipDist[4]");
    free(v); free(vsrc);

    /* A Create-style fragment shader: gl_FragColor + texture2D + shadow lod. */
    const char *fsrc_a = "#version 150\n";
    const char *fsrc_b = "uniform sampler2D t; varying vec2 uv;\n"
                         "void main(){ vec4 c=texture2D(t,uv); gl_FragColor=c; }\n";
    char *fsrc = concat(fsrc_a, fsrc_b);
    char *f = ltw_translate_shader(fsrc, 0x8B30); /* GL_FRAGMENT_SHADER */
    printf("%s\n", f);
    CK(strstr(f, "out vec4 _fragColor;") != NULL, "fragment gl_FragColor -> out _fragColor");
    CK(strstr(f, "texture(t,uv)") != NULL, "fragment texture2D -> texture");
    CK(strstr(f, "gl_FragColor") == NULL, "fragment no raw gl_FragColor");
    free(f); free(fsrc);

    printf("\n%d failure(s)\n", failures);
    return failures ? 1 : 0;
}
