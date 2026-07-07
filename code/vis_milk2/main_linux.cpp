// MilkDrop3 - Linux Entry Point (SDL2 + OpenGL)
// Phase 3: Preset engine + MilkdropRenderer

#include "glcontext.h"
#include "glshader.h"
#include "milkdrop_renderer.h"
#include "preset_engine.h"
#include "audio_capture.h"
#include <cstdio>
#include <cstdlib>
#include <cmath>

static GLContext g_gl;
static MilkdropRenderer g_renderer;
static PresetEngine g_presets;
static AudioCapture g_audio;

static const int FPS_TARGET = 60;
static const int FRAME_DELAY_MS = 1000 / FPS_TARGET;

int main(int argc, char* argv[])
{
    printf("MilkDrop3 - Linux Port (Phase 3)\n");
    printf("=================================\n\n");

    int width = 1024;
    int height = 768;
    bool fullscreen = false;

    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-f") == 0 || strcmp(argv[i], "--fullscreen") == 0)
            fullscreen = true;
        if (strcmp(argv[i], "-w") == 0 && i + 2 < argc)
        {
            width = atoi(argv[++i]);
            height = atoi(argv[++i]);
        }
    }

    if (!g_gl.CreateWindow(width, height, fullscreen))
    {
        fprintf(stderr, "\nERROR: Failed to create OpenGL window/context.\n");
        fprintf(stderr, "This machine may not support OpenGL 3.3 Core Profile.\n");
        fprintf(stderr, "Press Enter to exit.\n");
        getchar();
        return 1;
    }

    // Find shader data path relative to executable
    std::string data_path;
    std::string preset_path;
    {
        // Also try relative to the executable location
        std::string exe_dir;
        if (argc > 0 && argv[0][0] != '\0')
        {
            exe_dir = argv[0];
            size_t slash = exe_dir.find_last_of("/\\");
            if (slash != std::string::npos)
                exe_dir = exe_dir.substr(0, slash);
            else
                exe_dir = ".";
        }
        else
        {
            exe_dir = ".";
        }

        std::string candidates[] = {
            exe_dir + "/resources/Milkdrop2/data",
            "resources/Milkdrop2/data",
            "../resources/Milkdrop2/data",
            "../../resources/Milkdrop2/data",
        };
        for (auto& cp : candidates)
        {
            FILE* f = fopen((cp + "/include.glsl").c_str(), "r");
            if (f)
            {
                fclose(f);
                data_path = cp;
                break;
            }
        }
        if (data_path.empty())
        {
            fprintf(stderr, "Cannot find shader data directory (searched %zu locations including %s)\n",
                    sizeof(candidates)/sizeof(candidates[0]), candidates[0].c_str());
            return 1;
        }

        // Find presets directory
        std::string preset_candidates[] = {
            exe_dir + "/presets",
            "presets",
            "../presets",
            "../../presets",
        };
        for (auto& cp : preset_candidates)
        {
            FILE* f = fopen((cp + "/101-per_frame.milk").c_str(), "r");
            if (f)
            {
                fclose(f);
                preset_path = cp;
                break;
            }
        }
        if (preset_path.empty())
        {
            fprintf(stderr, "Warning: cannot find presets directory\n");
        }
    }

    if (!g_renderer.Init(&g_gl, data_path.c_str()))
    {
        fprintf(stderr, "Failed to initialize MilkDrop renderer\n");
        return 1;
    }
    g_renderer.SetPresetEngine(&g_presets);

    // Initialize audio capture
    g_audio.Init(44100, 1024);

    // Load a preset
    if (!preset_path.empty())
    {
        std::string preset_file = preset_path + "/101-per_frame.milk";
        if (!g_presets.LoadPreset(preset_file.c_str()))
        {
            fprintf(stderr, "Failed to load preset, using defaults\n");
        }
        else
        {
            printf("Loaded preset: %s\n", g_presets.GetPresetName());
            g_presets.ApplyShaderOverrides(&g_renderer);
        }
    }

    // Draw initial test pattern into VS0 so we have content to warp
    {
        g_renderer.FillTestPattern();
    }

    // Clear default framebuffer to black
    g_gl.Clear(0, 0, 0, 1);
    g_gl.SwapBuffers();

    printf("MilkDrop renderer initialized.\n");
    printf("  Render targets: %dx%d\n", width, height);
    printf("  Shaders: %s\n", data_path.c_str());
    printf("  Presets: %s\n", preset_path.empty() ? "(none)" : preset_path.c_str());
    printf("  Controls: ESC=quit\n\n");

    // Diagnostic: render a red quad directly to screen
    {
        const char* vs = "#version 330 core\nlayout(location=0)in vec3 p;void main(){gl_Position=vec4(p,1);}\n";
        const char* fs = "#version 330 core\nout vec4 c;void main(){c=vec4(1,0,0,1);}\n";
        GLuint vs_o = g_gl.CompileShader(vs, GL_VERTEX_SHADER);
        GLuint fs_o = g_gl.CompileShader(fs, GL_FRAGMENT_SHADER);
        if (vs_o && fs_o)
        {
            GLuint prog = g_gl.LinkProgram(vs_o, fs_o);
            glDeleteShader(vs_o); glDeleteShader(fs_o);
            if (prog)
            {
                g_gl.UseShader(prog);
                float verts[] = { -1,-1,0, 1,-1,0, 1,1,0, -1,1,0 };
                GLuint vao, vbo;
                glGenVertexArrays(1, &vao);
                glGenBuffers(1, &vbo);
                glBindVertexArray(vao);
                glBindBuffer(GL_ARRAY_BUFFER, vbo);
                glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STREAM_DRAW);
                glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
                glEnableVertexAttribArray(0);
                glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
                glBindVertexArray(0);
                glDeleteBuffers(1, &vbo);
                glDeleteVertexArrays(1, &vao);
                g_gl.UseShader(0);
                g_gl.DestroyShader(prog);
                g_gl.SwapBuffers();
                printf("DIAG: drew red quad to screen. If you see RED, basic GL works.\n");
                printf("      If still GREEN, problem is SDL/GL context layer.\n");
                SDL_Delay(2000);  // pause 2s so user can see
            }
        }
    }

    Uint32 lastFrameTime = SDL_GetTicks();
    Uint32 startTime = lastFrameTime;
    int frameCount = 0;
    Uint32 fpsTimer = lastFrameTime;

    while (g_gl.ProcessEvents())
    {
        Uint32 now = SDL_GetTicks();
        float time = (now - startTime) / 1000.0f;
        float dt = (now - lastFrameTime) / 1000.0f;
        if (dt > 0.1f) dt = 0.1f;

        // Read audio
        float bass = 0, mid = 0, treb = 0;
        float bass_att = 0, mid_att = 0, treb_att = 0;
        g_audio.Read(bass, mid, treb, bass_att, mid_att, treb_att);

        // Get waveform for wave rendering
        float waveform[1024];
        int waveform_len = g_audio.GetWaveform(waveform, 1024);

        // Evaluate preset equations with live audio
        g_presets.EvaluateFrame(time, 60.0f,
                                bass, mid, treb,
                                bass_att, mid_att, treb_att,
                                frameCount,
                                &g_renderer);

        g_renderer.RenderFrame(time, dt, waveform, waveform_len);

        g_gl.SwapBuffers();

        Uint32 frameTime = SDL_GetTicks() - lastFrameTime;
        if (frameTime < FRAME_DELAY_MS)
            SDL_Delay(FRAME_DELAY_MS - frameTime);
        lastFrameTime = SDL_GetTicks();

        frameCount++;
        if (now - fpsTimer >= 1000)
        {
            printf("FPS: %d | bass=%.2f mid=%.2f treb=%.2f\n",
                   frameCount, bass, mid, treb);
            frameCount = 0;
            fpsTimer = now;
        }
    }

    g_audio.Shutdown();
    g_gl.DestroyWindow();
    SDL_Quit();

    printf("\nMilkDrop3 shutdown complete.\n");
    return 0;
}
