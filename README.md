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
- `test_glsl.c` — headless unit tests (Create-like vertex/fragment/MRT shaders).
- `gl_wrapper.c` — structural skeleton showing how the translator plugs into a
  GL→GLES dispatch (transform feedback / VAO forwards). Build only on a GLES target.

## Build & test (no GPU needed)

```sh
make            # builds and runs the translator tests
```

## Build on a real GLES/EGL target (Android / Mesa host)

```sh
make gles       # requires GLESv2/EGL dev libraries
```

## Honest limitations

This is **not** a drop-in LTW replacement. It is a focused, correct foundation that solves
the **shader-translation** half of the Create crash. The remaining hard parts LTW itself
still struggles with (per the project roadmap: *"resolve issues with Create"*) are:

- clipping/cull distances (`GL_EXT_clip_cull_distance`) — not available in GLES 3.0,
- full transform-feedback / vertex-pulling correctness,
- buffer/uniform edge cases in large mods.

Solving those requires extending `gl_wrapper.c` against a real GLES driver and testing with
actual Minecraft + Create, which can't be done in this environment.

## License

MIT — free to use, extend, and upstream into LTW-style projects.
