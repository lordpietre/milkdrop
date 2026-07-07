# Plan de Integración Linux para MilkDrop3 — Puerto Nativo

## Diagnóstico

MilkDrop3 es 100% dependiente de Windows:
- **Gráficos**: DirectX 9 + HLSL + D3DX9
- **Audio**: WASAPI loopback (COM)
- **UI/Sistema**: Win32 API (HWND, WinMain, CreateWindow, etc.)
- **Build**: Visual Studio `.vcxproj` / `.sln`
- **Shaders**: HLSL `.fx` (warp, composite, blur)

No existe `#ifdef __linux__` en ningún lado. El soporte actual es solo Wine.

---

## Progreso Actual

### ✅ Fase 1: CMake Build System — COMPLETADA
- [x] `code/CMakeLists.txt` raíz con detección de plataforma
- [x] `code/ns-eel2/CMakeLists.txt` — compila como librería estática en Linux
- [x] `code/vis_milk2/CMakeLists.txt` — target ejecutable `milkdrop3`
- [x] Dependencias detectadas: SDL2, GLEW, GLFW3, OpenGL, PipeWire, glm
- [x] ns-eel2 compila 100% en Linux x86_64
- [x] Stub `wdltypes.h` creado para `INT_PTR` en Linux
- [x] Includes fijados en `asm-nseel-x86-gcc.c`

### ✅ Fase 2: Abstracción de Plataforma con SDL2 — COMPLETADA
- [x] `shell_defines.h` envuelto con `#ifdef _WIN32`
- [x] `support.h` envuelto con `#ifdef _WIN32`
- [x] `dxcontext.h` envuelto con `#ifdef _WIN32`
- [x] `texmgr.h` envuelto con `#ifdef _WIN32`  
- [x] `textmgr.h` envuelto con `#ifdef _WIN32`
- [x] `utility.h` envuelto con `#ifdef _WIN32`
- [x] `wasabi.h` envuelto con `#ifdef _WIN32`
- [x] `state.h` envuelto con `#ifdef _WIN32` (d3dx9math.h)
- [x] Nuevo `main_linux.cpp` con SDL2 (ventana, eventos, input)
- [x] CMakeLists.txt compila archivo correcto según plataforma
- [x] Ventana SDL2 + OpenGL 3.3 Core Profile funciona (testeado con Xvfb)
- [x] FPS timing y event loop funcionando

### ✅ Fase 3: OpenGL + GLSL — EN PROGRESO
#### 3.1 Shaders: HLSL → GLSL — COMPLETADO
- [x] `include.fx` → `include.glsl` (todos los uniforms, samplers, #defines)
- [x] `warp_vs.fx` → `warp_vs.glsl`
- [x] `warp_ps.fx` → `warp_ps.glsl`
- [x] `comp_vs.fx` → `comp_vs.glsl`
- [x] `comp_ps.fx` → `comp_ps.glsl`
- [x] `blur_vs.fx` → `blur_vs.glsl`
- [x] `blur1_ps.fx` → `blur1_ps.glsl`
- [x] `blur2_ps.fx` → `blur2_ps.glsl`

#### 3.2 OpenGL Context Wrapper — COMPLETADO
- [x] `glcontext.h/.cpp` creado (reemplaza dxcontext)
- [x] SDL2 window + OpenGL 3.3 Core context
- [x] FBO management (render targets)
- [x] Texture creation/destruction
- [x] Shader compilation (glCompileShader + glLinkProgram)
- [x] Uniform setting helpers (float, vec2-4, mat4, mat4x3)
- [x] Fullscreen quad VAO/VBO
- [x] Event processing (quit, resize, keyboard)

#### 3.3 GLShaderManager — COMPLETADO
- [x] Cargar GLSL desde archivos
- [x] Compilación de shaders con cuerpo de preset (warp/comp)
- [x] Cache de uniform locations
- [x] Helpers para SetVec4, SetFloat, SetMatrix4x3

#### 3.4 MilkDrop Render Pipeline — COMPLETADO
- [x] `milkdrop_renderer.h/.cpp` implementa pipeline completo
  - [x] 2 VS render targets (double buffer) + 6 blur targets
  - [x] Warp pass: VS0 → VS1 con warp pixel shader + grid mesh UVs
  - [x] Blur passes: gaussiano horizontal+vertical en cadena
  - [x] Composite pass: VS1 → pantalla con comp shader
  - [x] VS0/VS1 swap cada frame
- [x] Grid mesh 65×65 con distorsión per-vertex (ComputeGridUVs)
- [x] Test pattern procedural al iniciar
- [x] Pipeline funcional sin errores

#### 3.5 Por hacer
- [ ] Preset loader: parsear .milk files → variables de preset
- [ ] Evaluación de ecuaciones por-frame con ns-eel2
- [ ] Compilación runtime de shaders de preset (warp_ps/comp_ps body reemplazable)
- [ ] Portar `state.cpp` a glm
- [ ] OpenGL texture manager
- [ ] Canvas/render-pass opcional (sprites, motion vectors)

### ⏳ Fase 4: Audio — Pendiente
- [ ] PipeWire/ALSA loopback capture
- [ ] FFT analysis → bass/mid/treb bands
- [ ] Integrar con audiobuf.cpp

### ⏳ Fase 5: Empaquetado y CI — Pendiente
- [ ] Flatpak / AppImage
- [ ] GitHub Actions

### ⏳ Fase 6: Plugin VLC — Pendiente
- [ ] Interfaz plugin VLC en C
- [ ] Compartir librería de render con VLC

---

## Dependencias Linux Instaladas

| Propósito | Librería | Estado |
|-----------|----------|--------|
| Build system | CMake 3.28.3 | ✅ |
| Compilador | GCC 13.3.0 | ✅ |
| Ventana/eventos | SDL2 2.30.0 | ✅ |
| OpenGL | libGL / GLEW 2.2.0 | ✅ |
| Ventana OpenGL | GLFW3 3.3.10 | ✅ |
| Matemáticas | glm 0.9.9.8 | ✅ |
| Audio | PipeWire 1.0.5 | ✅ |

---

## Archivos Clave

| Archivo | Propósito |
|---------|-----------|
| `code/CMakeLists.txt` | Build raíz multiplataforma |
| `code/wdltypes.h` | Stub `INT_PTR` para Linux |
| `code/vis_milk2/main_linux.cpp` | Entry point SDL2 |
| `code/vis_milk2/glcontext.h/.cpp` | Contexto OpenGL (FBOs, VAOs, shaders) |
| `code/vis_milk2/glshader.h/.cpp` | Shader manager + CompilePixelShader |
| `code/vis_milk2/milkdrop_renderer.h/.cpp` | Pipeline warp→blur→composite |
| `code/vis_milk2/milkdrop_mesh.h/.cpp` | Grid mesh 65×65 con compute de UVs |
| `code/vis_milk2/fft.cpp` | FFT analysis stub |
| `code/resources/Milkdrop2/data/*.glsl` | 8 shaders GLSL |
| `build.sh` | Script build + instalación dependencias |
