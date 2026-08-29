#include "glsl_translate.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures = 0;

static void check(const char *label, const char *hay, const char *needle) {
    if (strstr(hay, needle)) {
        printf("  [ok]   %s contains: %s\n", label, needle);
    } else {
        printf("  [FAIL] %s missing: %s\n", label, needle);
        failures++;
    }
}

static void check_not(const char *label, const char *hay, const char *needle) {
    if (strstr(hay, needle)) {
        printf("  [FAIL] %s should NOT contain: %s\n", label, needle);
        failures++;
    } else {
        printf("  [ok]   %s correctly omits: %s\n", label, needle);
    }
}

int main(void) {
    /* A Create-mod-like VERTEX shader (desktop GLSL 120/150 style) */
    const char *vsrc =
        "#version 150\n"
        "attribute vec3 position;\n"
        "attribute vec2 uv;\n"
        "varying vec2 vUv;\n"
        "varying vec4 vColor;\n"
        "uniform mat4 modelView;\n"
        "void main() {\n"
        "  vec4 p = gl_Vertex + vec4(position, 1.0);\n"
        "  vec3 n = gl_Normal;\n"
        "  vec4 tc = gl_MultiTexCoord0;\n"
        "  vUv = uv;\n"
        "  vColor = gl_Color;\n"
        "  gl_Position = modelView * p;\n"
        "}\n";

    char *v = glsl_translate(vsrc, STAGE_VERTEX);
    printf("=== Translated VERTEX shader ===\n%s\n", v);

    check("vertex", v, "#version 300 es");
    check("vertex", v, "precision highp float;");
    check("vertex", v, "in vec3 position;");          /* attribute -> in */
    check("vertex", v, "out vec2 vUv;");              /* varying -> out */
    check("vertex", v, "out vec4 vColor;");
    check("vertex", v, "in vec4 gl_Vertex;");         /* builtin attr injected */
    check("vertex", v, "in vec3 gl_Normal;");
    check("vertex", v, "in vec4 gl_MultiTexCoord0;");
    check_not("vertex", v, "#version 150");           /* old version gone */

    free(v);

    /* A Create-mod-like FRAGMENT shader using gl_FragColor + texture2D */
    const char *fsrc =
        "#version 120\n"
        "varying vec2 vUv;\n"
        "uniform sampler2D tex;\n"
        "void main() {\n"
        "  vec4 c = texture2D(tex, vUv);\n"
        "  gl_FragColor = c;\n"
        "}\n";

    char *f = glsl_translate(fsrc, STAGE_FRAGMENT);
    printf("=== Translated FRAGMENT shader ===\n%s\n", f);

    check("fragment", f, "#version 300 es");
    check("fragment", f, "precision highp float;");
    check("fragment", f, "in vec2 vUv;");             /* varying -> in */
    check("fragment", f, "texture(tex, vUv)");        /* texture2D -> texture */
    check("fragment", f, "out vec4 _fragColor;");     /* gl_FragColor decl */
    check("fragment", f, "_fragColor = c;");          /* usage rewritten */
    check_not("fragment", f, "gl_FragColor");         /* no longer raw */
    check_not("fragment", f, "#version 120");

    free(f);

    /* Fragment using gl_FragData (multiple render targets, used by some mods) */
    const char *fsrc2 =
        "#version 150\n"
        "out vec4 foo;\n"
        "void main() {\n"
        "  gl_FragData[0] = vec4(1.0);\n"
        "  gl_FragData[1] = vec4(0.0);\n"
        "}\n";
    char *f2 = glsl_translate(fsrc2, STAGE_FRAGMENT);
    printf("=== Translated FRAGMENT (MRT) ===\n%s\n", f2);
    check("fragment-mrt", f2, "out vec4 gl_FragData[2];");
    check("fragment-mrt", f2, "gl_FragData[0] = vec4(1.0);");
    free(f2);

    printf("\n%d failure(s)\n", failures);
    return failures ? 1 : 0;
}
