#ifndef GL_ADAPT_H
#define GL_ADAPT_H

#include "glsl_translate.h"

#ifndef GL_ADAPT_TYPES
#define GL_ADAPT_TYPES
typedef unsigned int GLenum;
typedef unsigned int GLuint;
#endif

/*
 * Adaptation layer for desktop-OpenGL constructs that simply do not exist
 * in OpenGL ES 3.0 but that mods (Create and friends) still emit. Every
 * function rewrites an enum/parameter in place and returns 1 if it changed
 * it, 0 if it was already valid for GLES. The caller (gl_wrapper) forwards
 * the rewritten value to the real GLES call.
 *
 * These are pure translations -> fully unit-testable without a GPU.
 */

/* GL_TEXTURE_1D / GL_TEXTURE_RECTANGLE -> GL_TEXTURE_2D (GLES has neither) */
int adapt_texture_target(GLenum *target);

/* GL_CLAMP -> GL_CLAMP_TO_EDGE ; GL_CLAMP_TO_BORDER -> GL_CLAMP_TO_EDGE
 * (CLAMP_TO_BORDER is ES 3.2 only; edge is the safe fallback) */
int adapt_wrap(GLenum *wrap);

/* Legacy single/dual-channel formats -> GL_RGBA / GL_RG so texImage* works */
int adapt_pixel_format(GLenum *internalformat, GLenum *format);

/* GL_QUADS -> GL_TRIANGLES ; GL_POLYGON -> GL_TRIANGLE_FAN
 * (both removed in GLES). Rewrites *mode in place. */
int adapt_primitive(GLenum *mode);

/* Expand a QUADS index list into a TRIANGLES index list.
 * quad_indices: 4*quadCount indices; out: 6*quadCount indices (caller allocs).
 * Returns number of output indices, or 0 if not a quad list. */
int expand_quads_to_triangles(int quad_count, const GLuint *quad_indices, GLuint *out);

#endif /* GL_ADAPT_H */
