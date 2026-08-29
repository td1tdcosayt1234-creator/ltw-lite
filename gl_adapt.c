#include "gl_adapt.h"

/* GLES 3.0 legal texture targets (subset we pass through unchanged) */
static int is_valid_target(GLenum t) {
    switch (t) {
        case 0x0DE1: /* GL_TEXTURE_2D */
        case 0x8513: /* GL_TEXTURE_CUBE_MAP */
        case 0x806F: /* GL_TEXTURE_2D_ARRAY? (ES3) */
        case 0x8C2A: /* GL_TEXTURE_3D (ES3) */
            return 1;
        default:
            return 0;
    }
}

int adapt_texture_target(GLenum *target) {
    /* GL_TEXTURE_1D = 0x0DE0 ; GL_TEXTURE_RECTANGLE = 0x84F5 */
    if (*target == 0x0DE0 || *target == 0x84F5) {
        *target = 0x0DE1; /* GL_TEXTURE_2D */
        return 1;
    }
    /* leave valid targets untouched; anything else we cannot fix here */
    if (!is_valid_target(*target)) { *target = 0x0DE1; return 1; }
    return 0;
}

int adapt_wrap(GLenum *wrap) {
    /* GL_CLAMP = 0x2900 ; GL_CLAMP_TO_EDGE = 0x812F ;
       GL_CLAMP_TO_BORDER = 0x812D */
    if (*wrap == 0x2900 || *wrap == 0x812D) {
        *wrap = 0x812F; /* GL_CLAMP_TO_EDGE */
        return 1;
    }
    return 0;
}

int adapt_pixel_format(GLenum *internalformat, GLenum *format) {
    int changed = 0;
    /* GL_ALPHA=0x1906, GL_LUMINANCE=0x1909, GL_LUMINANCE_ALPHA=0x190A,
       GL_INTENSITY=0x8049 -> RGBA.  GL_R=0x1903/GL_RED ok in ES3;
       keep simple: collapse legacy single-channel to RGBA. */
    switch (*format) {
        case 0x1906: /* ALPHA */
        case 0x1909: /* LUMINANCE */
        case 0x190A: /* LUMINANCE_ALPHA */
        case 0x8049: /* INTENSITY */
            *format = 0x1908;        /* GL_RGBA */
            *internalformat = 0x1908;/* GL_RGBA */
            changed = 1;
            break;
        default:
            break;
    }
    /* GL_INTENSITY as internalformat alone */
    if (*internalformat == 0x8049) {
        *internalformat = 0x1908;
        changed = 1;
    }
    return changed;
}

int adapt_primitive(GLenum *mode) {
    /* GL_QUADS = 0x0007 ; GL_POLYGON = 0x0009 */
    if (*mode == 0x0007) { *mode = 0x0004; return 1; } /* TRIANGLES */
    if (*mode == 0x0009) { *mode = 0x000B; return 1; } /* TRIANGLE_FAN */
    return 0;
}

int expand_quads_to_triangles(int quad_count, const GLuint *q, GLuint *out) {
    if (quad_count <= 0 || !q || !out) return 0;
    for (int i = 0; i < quad_count; i++) {
        GLuint a = q[4 * i + 0];
        GLuint b = q[4 * i + 1];
        GLuint c = q[4 * i + 2];
        GLuint d = q[4 * i + 3];
        /* two triangles: a,b,c and a,c,d */
        out[6 * i + 0] = a;
        out[6 * i + 1] = b;
        out[6 * i + 2] = c;
        out[6 * i + 3] = a;
        out[6 * i + 4] = c;
        out[6 * i + 5] = d;
    }
    return 6 * quad_count;
}
