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

    /* Clip distance / cull distance emulation (the LTW gap that breaks
     * Create + Immersive Portals). gl_ClipDistance must become a varying
     * and the fragment shader must discard clipped fragments. */
    const char *vclip =
        "#version 150\n"
        "void main() {\n"
        "  gl_ClipDistance[0] = dot(gl_Vertex, vec4(1.0,0.0,0.0,0.0));\n"
        "  gl_Position = vec4(0.0);\n"
        "}\n";
    char *vc = glsl_translate(vclip, STAGE_VERTEX);
    printf("=== VERTEX with clip distance ===\n%s\n", vc);
    check("clip-vert", vc, "out float _clipDist[1];");
    check("clip-vert", vc, "_clipDist[0] = dot(gl_Vertex");
    free(vc);

    const char *fclip =
        "#version 150\n"
        "void main() {\n"
        "  gl_FragColor = vec4(1.0);\n"
        "}\n";
    /* simulate that clip distance is used (linker would provide it) */
    char *fc = glsl_translate(fclip, STAGE_FRAGMENT);
    printf("=== FRAGMENT (clip discard injected only if clip used) ===\n%s\n", fc);
    /* no clip used here, so no discard injected */
    check_not("clip-frag", fc, "_clipDist");
    free(fc);

    /* texture variant mapping */
    const char *ftex =
        "#version 150\n"
        "uniform sampler2D t; uniform sampler2DShadow s;\n"
        "in vec2 uv; in vec4 uvp;\n"
        "void main() {\n"
        "  vec4 a = texture2DProj(t, uvp);\n"
        "  float b = shadow2D(s, vec3(uv, 0.5)).x;\n"
        "  float c = texture2DGrad(t, uv, vec2(1.0), vec2(1.0)).r;\n"
        "  gl_FragColor = a + vec4(b) + vec4(c);\n"
        "}\n";
    char *ft = glsl_translate(ftex, STAGE_FRAGMENT);
    printf("=== FRAGMENT texture variants ===\n%s\n", ft);
    check("texvar", ft, "textureProj(t, uvp)");
    check("texvar", ft, "texture(s, vec3(uv, 0.5))");
    check("texvar", ft, "textureGrad(t, uv, vec2(1.0), vec2(1.0))");
    free(ft);

    /* #extension stripping: GL_EXT_clip_cull_distance must be dropped (it is
     * the fatal "Extension not supported" error from Pojav issue #4310),
     * while gl_ClipDistance is emulated via discard. */
    const char *vext =
        "#version 320 es\n"
        "#extension GL_EXT_clip_cull_distance : require\n"
        "precision highp float;\n"
        "out float gl_ClipDistance[4];\n"
        "void main() {\n"
        "  gl_ClipDistance[0] = 1.0;\n"
        "  gl_Position = vec4(0.0);\n"
        "}\n";
    char *ex = glsl_translate(vext, STAGE_VERTEX);
    printf("=== VERTEX with #extension (clipped) ===\n%s\n", ex);
    check_not("ext", ex, "GL_EXT_clip_cull_distance");   /* dropped */
    check("ext", ex, "out float _clipDist[4];");         /* renamed */
    check("ext", ex, "_clipDist[0] = 1.0;");
    free(ex);

    const char *fext =
        "#version 150\n"
        "#extension GL_ARB_shader_texture_lod : enable\n"
        "void main() { gl_FragColor = vec4(1.0); }\n";
    char *fx = glsl_translate(fext, STAGE_FRAGMENT);
    printf("=== FRAGMENT with dropped #extension ===\n%s\n", fx);
    check_not("ext2", fx, "GL_ARB_shader_texture_lod");
    check("ext2", fx, "_fragColor = vec4(1.0);");
    free(fx);

    printf("\n%d failure(s)\n", failures);
    return failures ? 1 : 0;
}
