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
  - [x] Warp pass: VS0 → VS1 con warp pixel shader
  - [x] Blur passes: gaussiano horizontal+vertical en cadena
  - [x] Composite pass: VS1 → pantalla con comp shader
  - [x] VS0/VS1 swap cada frame
- [x] Pipeline funcional (testeado con Xvfb — sin errores)

#### 3.5 Por hacer (próximos pasos)
- [ ] OpenGL texture manager (reemplazar texmgr)
- [ ] Portar `state.cpp` con glm en vez de D3DXVECTOR3/4
- [ ] Grid mesh warp (ComputeGridAlphaValues + WarpedBlit_Shaders)
- [ ] Portar `milkdropfs.cpp` (render loop principal) a OpenGL
- [ ] Portar `plugin.cpp` (shader compilation de presets con runtime)

### ⏳ Fase 4: Audio (2 semanas) — Pendiente
- [ ] PipeWire loopback capture
- [ ] ALSA fallback
- [ ] Integrar con audiobuf.cpp

### ⏳ Fase 5: Empaquetado y CI (1 semana) — Pendiente
- [ ] Flatpak / AppImage
- [ ] GitHub Actions

### ⏳ Fase 6: Pruebas y Polishing (2-3 semanas) — Pendiente
- [ ] Probar presets .milk y .milk2
- [ ] Verificar FPS
- [ ] Probar multi-monitor

---

## Roadmap por Fases

### Fase 1: CMake Build System (1-2 semanas)
- Migrar de `.vcxproj` a **CMake** 3.20+
- Detectar plataforma con `if(WIN32)` / `if(UNIX)`
- En Windows: seguir linkeando DirectX 9
- En Linux: linkear SDL2, OpenGL, PipeWire/ALSA
- Organizar targets: `milkdrop3` (ejecutable), `ns-eel2` (librería)
- Copiar recursos (shaders, docs) al directorio de build

### Fase 2: Abstracción de Plataforma con SDL2 (2-3 semanas)
- Reemplazar `WinMain` + `HWND` + `MSG` loop con SDL2
- Crear `main_linux.cpp` con SDL2
- Envolver headers criticales con `#ifdef _WIN32`
- Reemplazar `dxcontext` con `glcontext` (OpenGL)

**Archivos creados/modificados:**
- `code/CMakeLists.txt` ✓
- `code/ns-eel2/CMakeLists.txt` ✓
- `code/vis_milk2/CMakeLists.txt` ✓
- `code/wdltypes.h` ✓ (stub)
- `code/vis_milk2/shell_defines.h` ✓ (platform ifdef)
- `code/vis_milk2/support.h` ✓ (platform ifdef)
- `code/vis_milk2/dxcontext.h` ✓ (platform ifdef)
- `code/main_linux.cpp` — pendiente

### Fase 3: OpenGL + GLSL (4-6 semanas) — La fase crítica
Reemplazar todo el pipeline DirectX 9 por OpenGL 3.3+.

#### 3.1 Shaders: HLSL → GLSL
Traducir los 8 archivos `.fx` a GLSL. El HLSL de MilkDrop3 es SM2.0/3.0, traducción directa.

#### 3.2 Render Pipeline
Reemplazar todas las interfaces DirectX 9 por OpenGL.

#### 3.3 Referencia: projectM
[projectM](https://github.com/projectM-visualizer/projectm) ya portó MilkDrop clásico a OpenGL. Usar como referencia para traducción de ecuaciones y pipeline.

### Fase 4: Audio (2 semanas)
Reemplazar WASAPI loopback con PipeWire (recomendado) o ALSA.

### Fase 5: Empaquetado y CI (1 semana)
Flatpak o AppImage. GitHub Actions para Linux.

### Fase 6: Pruebas y Polishing (2-3 semanas)
Probar presets, rendimiento, multi-monitor, diferentes distros.

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

## Estructura de Directorios Propuesta

```
code/
├── CMakeLists.txt              # Raíz del build
├── wdltypes.h                  # Stub para INT_PTR en Linux
├── vis_milk2/
│   ├── CMakeLists.txt
│   ├── Milkdrop2PcmVisualizer.cpp  # Windows: MAIN entry point
│   ├── main_linux.cpp           # Linux: MAIN entry point (SDL2)
│   ├── glcontext.cpp/.h         # Contexto OpenGL (reemplaza dxcontext)
│   ├── shaders/                 # GLSL (traducidos de HLSL .fx)
│   │   ├── include.glsl
│   │   ├── warp.vert / warp.frag
│   │   ├── comp.vert / comp.frag
│   │   └── blur.vert / blur.frag
│   └── ... (resto sin cambios)
├── audio/
│   ├── audiobuf.cpp/.h          # Reutilizable
│   ├── loopback-capture.cpp     # Solo Windows
│   └── pipewire-capture.cpp     # Solo Linux
├── ns-eel2/                     # Portable (C), ya compila en Linux
└── resources/                   # Datos (shaders originales, docs)
```

---

## Estrategia de Implementación

Para no romper el build de Windows, usar `#ifdef _WIN32` / `#ifdef __linux__` durante la transición:

1. **Envolver** el código Windows en `#ifdef _WIN32`
2. **Agregar** implementación Linux con `#else` / `#ifdef __linux__`
3. **Refactorizar** gradualmente extrayendo interfaces comunes
4. Los archivos nuevos para Linux van en archivos separados (`*_linux.cpp`)
