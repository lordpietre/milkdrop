# MilkDrop3 on Linux — Native Port

A native Linux port of MilkDrop3 that runs without Wine.
Uses SDL2 + OpenGL 3.3 Core + GLSL shaders + PipeWire audio capture.

## Current Status — Phase 4

| Component             | Status     |
|-----------------------|------------|
| SDL2 window + events  | ✅ Done    |
| OpenGL 3.3 Core FBOs  | ✅ Done    |
| GLSL shaders (8)      | ✅ Done    |
| Warp/blur/composite   | ✅ Done    |
| Grid mesh 65×65       | ✅ Done    |
| .milk preset loader   | ✅ Done    |
| Per-frame equations   | ✅ Done    |
| Per-pixel equations   | ✅ Done    |
| Custom warp/comp shaders | ✅ Done |
| PipeWire audio capture | ✅ Done |
| FFT → bass/mid/treb   | ✅ Done    |
| state.cpp port (glm)  | ❌ Pending |
| Texture manager       | ❌ Pending |
| Wave/shape rendering  | ❌ Pending |

## Build Dependencies

- GCC >= 10
- CMake >= 3.10
- SDL2
- GLEW
- GLM (OpenGL Mathematics)
- PipeWire (>= 0.3, optional — falls back gracefully)

### Install on Ubuntu/Debian

```bash
sudo apt install cmake g++ libsdl2-dev libglew-dev libglm-dev libpipewire-0.3-dev
```

### Install on Arch/Manjaro

```bash
sudo pacman -S cmake gcc sdl2 glew glm pipewire
```

## Build

```bash
git clone https://github.com/lordpietre/milkdrop.git
cd milkdrop
bash build.sh
```

Or manually:
```bash
cd build
cmake ../code
make -j$(nproc)
```

The binary is at `build/vis_milk2/milkdrop3`.

## Run

```bash
cd build/vis_milk2
./milkdrop3
```

### Options

| Flag | Description |
|------|-------------|
| `-f` or `--fullscreen` | Start fullscreen |
| `-w W H` | Set window size (e.g. `-w 1920 1080`) |

### Presets

The build script copies `presets/tests/*.milk` into the build directory.
To use your own presets, place `.milk` files in `build/vis_milk2/presets/`.

The default preset loaded is `101-per_frame.milk`.

### Controls

| Key | Action |
|-----|--------|
| `ESC` | Quit |

## Architecture

```
main_linux.cpp          -- Entry point, SDL event loop, audio + preset + render per frame
glcontext.h/.cpp        -- SDL2 window + OpenGL context + FBO/texture helpers
glshader.h/.cpp         -- GLSL shader compiler, uniform cache, CompilePixelShader
milkdrop_renderer.h/.cpp-- Pipeline: warp pass → 6 blur passes → composite pass
milkdrop_mesh.h/.cpp    -- Grid mesh (65×65) with per-vertex UV distortion
preset_engine.h/.cpp    -- .milk parser + TinyExpr equation evaluator
audio_capture.h/.cpp    -- PipeWire loopback → ring buffer → FFT → bass/mid/treb
fft.cpp                 -- FFT implementation (from original MilkDrop)
tinyexpr.c              -- TinyExpr math expression library (zlib license)
```

### Pipeline

```
VS0 ──warp──→ VS1 ──blur×6──→ composite ──→ screen
↑                              |
└──────── swap ────────────────┘
```

The warp pass uses a 65×65 grid mesh with per-vertex UV distortion
(zoom, rotation, center offset, radial/angular warp, stretch).
Per-pixel equations from the preset modify these values per vertex.

The blur chain applies cascaded gaussian blur (horizontal + vertical,
each stage at half resolution).

The composite pass blends the blurred result onto the screen.

## Audio

Audio is captured from the system's default audio sink via PipeWire.
The capture runs in a separate thread with a lock-free ring buffer.
FFT (1024 samples, 44100 Hz) decomposes the signal into three bands:
- **Bass**: 0–761 Hz
- **Mid**: 761–2897 Hz
- **Treble**: 2897–22050 Hz

Attack/release smoothing and peak-hold are applied.
If PipeWire is not running, the visualizer continues without audio.

## Known Issues

- `pw_context_new()` fails if no PipeWire server is running (gracefully handled)
- ns-eel2 JIT compiler does not work on Linux x86_64 — equations are evaluated with TinyExpr instead
- Wave, shape, and sprite rendering not yet ported
- Only one preset loaded at a time (no preset transitions or mashups yet)

## License

MilkDrop3 is licensed under the BSD 3-Clause License.
See the LICENSE file for details.

TinyExpr is licensed under the zlib License.
See `code/vis_milk2/tinyexpr.h` for details.

## Credits

- **MilkDrop3**: MilkDrop2077
- **Original MilkDrop**: Ryan Geiss
- **Linux port**: lordpietre
- **TinyExpr**: Lewis Van Winkle (https://codeplea.com/tinyexpr)
