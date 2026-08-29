# ltw-lite

A **GL → GLES "thin wrapper"** — a small, real, compilable starting point for the
kind of renderer backend that [PojavLauncher's LTW](https://github.com/PojavLauncherTeam/LTW)
provides: running desktop-OpenGL software (like Minecraft: Java Edition mods) on top of
OpenGL ES 3.0.

## Why this exists

The Create mod (and others) crash on LTW with a **"vertex error"**. The root cause is that
LTW emulates desktop OpenGL 3.2 on top of OpenGL ES 3.0, but several desktop-GL constructs
are **invalid in GLSL ES 3.00** and must be rewritten before a shader is compiled:

| Desktop GL (what mods emit) | GLSL ES 3.00 (what GLES needs) |
|---|---|
| `attribute` | `in` (vertex) |
| `varying` | `out` (vertex) / `in` (fragment) |
| `texture2D` / `texture3D` / `textureCube` | `texture` |
| `texture2DLod` / ... | `textureLod` |
| `gl_FragColor` | a declared `out vec4` |
| `gl_FragData[i]` | a declared `out vec4` array |
| built-in `gl_Vertex`, `gl_Normal`, `gl_MultiTexCoord0…` | declared `in` attributes |

If those aren't translated, the vertex shader fails to compile → "vertex error" → the game
crashes. `glsl_translate.c` does exactly that translation, so mods get valid ES shaders
without the game knowing.

## What's in here

- `glsl_translate.h/.c` — the real, tested GLSL translator (the part that prevents the
  Create-style crash). Pure C, no GPU required.
- `test_glsl.c` — headless unit tests (Create-like vertex/fragment/MRT/clip/texture shaders).
- `gl_wrapper.c` — structural skeleton showing how the translator plugs into a
  GL→GLES dispatch (transform feedback / VAO / MRT / clip-plane emulation forwards).
  Build only on a GLES target.

## What this fixes for Create-style mods

Each item below was confirmed against real PojavLauncher issues (#1250, #3445,
#4310, #5528, #6593) and the community `create-gl4es-stencil-fix` mod.

| # | Real crash / problem | Root cause on GLES | Status | How it's fixed here |
|---|---|---|---|---|
| 1 | `GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT` / `GL_FRAMEBUFFER_UNSUPPORTED` at `RenderTarget.enableStencil()` | GLES 3.0 can't attach a standalone `STENCIL_INDEX8` next to a separate `DEPTH_COMPONENT` — needs one combined `DEPTH_STENCIL_ATTACHMENT` (DEPTH24_STENCIL8) | **fixed** | `gl_adapt.c` FBO builder merges depth+stencil; `glCheckFramebufferStatus` returns COMPLETE |
| 2 | `Could not compile shader` (Create's `GlShader`) | desktop GLSL (`attribute`/`varying`/`texture2D`/`gl_FragColor`/`gl_FragData`) is invalid in ES 3.00 | **fixed** | `glsl_translate` rewrites to ES 3.00 |
| 3 | `Extension 'GL_EXT_clip_cull_distance' not supported` + `Illegal identifier 'gl_ClipDistance'` (issue #4310) | extension line passed through; driver rejects it | **fixed** | `#extension` lines for emulated/core features are stripped; `gl_ClipDistance` → `_clipDist` varying + fragment `discard` |
| 4 | `GL_ARB_shader_texture_lod` / `GL_EXT_shader_texture_lod` rejected (Iris/shaders, #6593) | same `#extension` problem | **fixed** | stripped by the same mechanism |
| 5 | clip/cull distance rendering wrong | no `GL_EXT_clip_cull_distance` in ES 3.0 | **emulated** | discards fragments where clip distance `< 0` |
| 6 | `GL_QUADS`/`GL_POLYGON` primitives (old mod/renderer code) | removed in GLES | **fixed** | rewritten to `TRIANGLES`/`TRIANGLE_FAN`; quad index lists expanded |
| 7 | `GL_TEXTURE_1D`/`GL_TEXTURE_RECTANGLE` | don't exist in GLES | **fixed** | mapped to `GL_TEXTURE_2D` |
| 8 | `GL_CLAMP`/`GL_CLAMP_TO_BORDER` | ES only has `CLAMP_TO_EDGE` | **fixed** | mapped to `CLAMP_TO_EDGE` |
| 9 | legacy formats `GL_ALPHA`/`LUMINANCE`/`INTENSITY` | removed in ES 3.0 | **fixed** | mapped to `GL_RGBA` |
| 10 | `texture2DProj`/`shadow2D`/`texture2DGrad` | ES 3.00 names | **fixed** | mapped to `textureProj`/`texture`/`textureGrad` |
| 11 | transform feedback / vertex pulling | — | **native forward** | GLES 3.0 has real transform feedback |
| 12 | multiple render targets | — | **native forward** | GLES 3.0 `glDrawBuffers` |
| 13 | VAOs | — | **native forward** | GLES 3.0 `glVertexArray*` |

`gl_adapt.c` holds the pure enum/primitive translations (unit-tested headless in
`test_adapt.c`); `glsl_translate.c` holds the shader rewrites; `gl_wrapper.c` routes
desktop calls through both into real GLES.

## Build & test (no GPU needed)

```sh
make            # builds and runs the translator tests
```

## Build on a real GLES/EGL target (Android / Mesa host)

```sh
make gles       # requires GLESv2/EGL dev libraries
```

## Honest limitations

This solves the **API/shader-translation** layer that causes Create to fail to *compile*
and to *clip incorrectly*. It is **not** a full, drop-in LTW renderer and has **not** been
run against real Minecraft + Create on a device (that needs an Android GLES driver and the
game itself, which this environment lacks). The remaining real-world risks are:

- The older `glClipPlane()` + `GL_CLIP_PLANEi` form is stored as state but the plane
  equation is only *baked into `gl_ClipDistance`* when the wrapper's shader generator is
  wired to read `g_clip_planes` — that wiring lives in the host renderer, not here.
- `GL_TEXTURE_RECTANGLE` is mapped to `GL_TEXTURE_2D`; mods that use 0..w texcoords
  (instead of 0..1) would need a shader coord rewrite too.
- ELEMENT_ARRAY_BUFFER-backed `GL_QUADS` index expansion needs the host's buffer contents;
  the client-index path is handled, the buffer path needs host support.
- Several GPU-only features (compute shaders, geometry shaders, `GL_TEXTURE_BUFFER`)
  that some advanced shader packs use are out of scope for a GLES 3.0 backend.
- Performance: a pure GLES 3.0 backend trades some features for speed; this is the same
  trade LTW makes.

To actually run Create on mobile, this code must be integrated into the LTW/Amethyst shader
pipeline and validated on-device.

## License

MIT — free to use, extend, and upstream into LTW-style projects.
