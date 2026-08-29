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

| Problem | Status | How |
|---|---|---|
| `attribute`/`varying`/`texture2D`/`gl_FragColor`/`gl_FragData` | **fixed** | `glsl_translate` rewrites them to ES 3.00 |
| `gl_ClipDistance` / user clip planes (`gl_ClipPlane`, `GL_CLIP_PLANEi`) | **fixed (emulated)** | clip distance → varying; fragment discards where `< 0` |
| `texture2DProj`/`shadow2D`/`texture2DGrad`/… | **fixed** | mapped to `textureProj`/`texture`/`textureGrad` |
| Transform feedback / vertex pulling | **native forward** | GLES 3.0 has real transform feedback |
| Multiple render targets | **native forward** | GLES 3.0 `glDrawBuffers` |
| VAOs | **native forward** | GLES 3.0 `glVertexArray*` |

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
- Large mods hit buffer/uniform/extension edge cases that only show up at runtime.
- Performance: a pure GLES 3.0 backend trades some features for speed; this is the same
  trade LTW makes.

To actually run Create on mobile, this code must be integrated into the LTW/Amethyst shader
pipeline and validated on-device.

## License

MIT — free to use, extend, and upstream into LTW-style projects.
