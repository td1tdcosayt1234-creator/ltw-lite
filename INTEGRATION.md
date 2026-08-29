# Integrating ltw-lite into the real LTW renderer

This repo is a focused, tested foundation for the fixes that stop **Create** (and
similar mods) from crashing on PojavLauncher's LTW renderer. It is *not* a fork of
LTW — it is a drop-in set of translation/adaptation functions. This file shows the
exact seams in the real `PojavLauncherTeam/LTW` source where to call them.

The LTW source tree (relevant files):
- `ltw/src/main/tinywrapper/shader_wrapper.c` — intercepts `glShaderSource`/`glCreateShader`
- `ltw/src/main/tinywrapper/framebuffer.c` — `glCheckFramebufferStatus` / FBO attachments
- `ltw/src/main/tinywrapper/es3_functions.h` — the `es3_functions.*` GLES dispatch table

## 1. Shader translation (fixes: shader compile errors, clip-cull-distance, lod ext)

`shader_wrapper.c`, function `glShaderSource` (around line 155), currently does:

```c
GLchar* new_source = optimize_shader(target_string, shader_info->shader_type, 460, current_context->shader_version);
```

Add `glsl_translate.c` + `ltw_glue.c` to the LTW build, then change that line to:

```c
/* ltw-lite: rewrite desktop GLSL -> GLSL ES 3.00 BEFORE optimization.
 * This is what makes Create's shaders compile and what emulates
 * GL_EXT_clip_cull_distance (issue #4310). */
char *translated = ltw_translate_shader(target_string, shader_info->shader_type);
GLchar* new_source = optimize_shader(translated, shader_info->shader_type, 460, current_context->shader_version);
free(translated);
```

`ltw_translate_shader()` maps `GL_VERTEX_SHADER`/`GL_FRAGMENT_SHADER` to the right
stage and calls `glsl_translate()`. That single call handles:

- `attribute`/`varying`/`texture2D`/`gl_FragColor`/`gl_FragData` rewriting
- stripping `#extension` lines for emulated/core features (`GL_EXT_clip_cull_distance`,
  `GL_ARB_shader_texture_lod`, `GL_EXT_draw_buffers`, …)
- `gl_ClipDistance` -> `_clipDist` varying + fragment `discard` emulation
- `texture2DProj`/`shadow2D`/`texture2DGrad` -> `textureProj`/`texture`/`textureGrad`

(Verified headless in `test_glsl` and `test_ltw`.)

## 2. Stencil FBO merge (fixes: `GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT` at enableStencil())

The #1 Create crash. In `framebuffer.c`, intercept `glFramebufferRenderbuffer`,
`glFramebufferTexture2D`, `glRenderbufferStorage`, and `glCheckFramebufferStatus`
to merge a separate `STENCIL_INDEX8` attachment into a combined
`DEPTH_STENCIL_ATTACHMENT(DEPTH24_STENCIL8)` using `fbo_builder_*` from `gl_adapt.c`
(exactly as `gl_wrapper.c` demonstrates). Then `glCheckFramebufferStatus` returns
`GL_FRAMEBUFFER_COMPLETE` and `RenderTarget.enableStencil()` no longer throws.

## 3. Primitive / texture adaptation (fixes: QUADS/POLYGON, 1D/RECT, wrap, formats)

In `es3_functions.h` (or a wrapper), route `glDrawArrays`/`glDrawElements` through
`adapt_primitive` + `expand_quads_to_triangles`, `glBindTexture`/`glTexImage2D`/
`glTexParameteri` through `adapt_texture_target`/`adapt_pixel_format`/`adapt_wrap`
(all in `gl_adapt.c`).

## Build

Add to LTW's `Android.mk` / CMake:
```
glsl_translate.c gl_adapt.c ltw_glue.c
```
No extra libraries needed — it is pure C that forwards to the existing GLES dispatch.

## Validation

After building LTW with these changes, run Minecraft + Create (Forge/Fabric) on a
device. Expected: no `GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT` crash, Create shaders
compile, and contraptions/clip planes render (clip planes are emulated via fragment
discard, so they are visually correct, not just non-crashing).
