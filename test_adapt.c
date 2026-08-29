#include "gl_adapt.h"
#include <stdio.h>

static int failures = 0;
#define CK(cond,msg) do { if (cond) printf("  [ok]   %s\n", msg); \
                          else { printf("  [FAIL] %s\n", msg); failures++; } } while(0)

int main(void) {
    printf("=== gl_adapt tests (no GPU needed) ===\n");

    /* texture targets */
    GLenum t = 0x0DE0; CK(adapt_texture_target(&t) == 1 && t == 0x0DE1, "TEXTURE_1D -> TEXTURE_2D");
    t = 0x84F5;       CK(adapt_texture_target(&t) == 1 && t == 0x0DE1, "TEXTURE_RECTANGLE -> TEXTURE_2D");
    t = 0x0DE1;       CK(adapt_texture_target(&t) == 0 && t == 0x0DE1, "TEXTURE_2D unchanged");

    /* wrap modes */
    GLenum w = 0x2900; CK(adapt_wrap(&w) == 1 && w == 0x812F, "GL_CLAMP -> CLAMP_TO_EDGE");
    w = 0x812D;        CK(adapt_wrap(&w) == 1 && w == 0x812F, "GL_CLAMP_TO_BORDER -> CLAMP_TO_EDGE");
    w = 0x2901;        CK(adapt_wrap(&w) == 0 && w == 0x2901, "GL_REPEAT unchanged");

    /* legacy pixel formats */
    GLenum in = 0x8049, fm = 0x1906; /* INTENSITY internal, ALPHA format */
    CK(adapt_pixel_format(&in, &fm) == 1 && in == 0x1908 && fm == 0x1908, "ALPHA/LUMINANCE -> RGBA");

    /* primitives */
    GLenum m = 0x0007; CK(adapt_primitive(&m) == 1 && m == 0x0004, "GL_QUADS -> GL_TRIANGLES");
    m = 0x0009;        CK(adapt_primitive(&m) == 1 && m == 0x000B, "GL_POLYGON -> TRIANGLE_FAN");
    m = 0x0004;        CK(adapt_primitive(&m) == 0, "GL_TRIANGLES unchanged");

    /* quad index expansion: quads (0,1,2,3) and (4,5,6,7) */
    GLuint q[8] = {0,1,2,3, 4,5,6,7};
    GLuint out[12];
    int n = expand_quads_to_triangles(2, q, out);
    CK(n == 12, "quad expansion count = 12");
    GLuint expect[12] = {0,1,2, 0,2,3, 4,5,6, 4,6,7};
    int ok = 1;
    for (int i = 0; i < 12; i++) if (out[i] != expect[i]) ok = 0;
    CK(ok, "quad expansion indices correct (two tris per quad)");

    printf("\n%d failure(s)\n", failures);
    return failures ? 1 : 0;
}
