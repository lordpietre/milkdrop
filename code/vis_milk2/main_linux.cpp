// MilkDrop3 - Linux Entry Point (SDL2 + OpenGL)
// Phase 3: Using GLContext + MilkdropRenderer

#include "glcontext.h"
#include "glshader.h"
#include "milkdrop_renderer.h"
#include <cstdio>
#include <cstdlib>
#include <cmath>

static GLContext g_gl;
static MilkdropRenderer g_renderer;

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
        return 1;
    }

    // Find shader data path relative to executable
    std::string data_path;
    {
        // Try several locations for the shader .glsl files
        const char* candidates[] = {
            "resources/Milkdrop2/data",
            "../resources/Milkdrop2/data",
            "../../resources/Milkdrop2/data",
        };
        for (auto* cp : candidates)
        {
            FILE* f = fopen((std::string(cp) + "/include.glsl").c_str(), "r");
            if (f)
            {
                fclose(f);
                data_path = cp;
                break;
            }
        }
        if (data_path.empty())
        {
            fprintf(stderr, "Cannot find shader data directory\n");
            return 1;
        }
    }

    if (!g_renderer.Init(&g_gl, data_path.c_str()))
    {
        fprintf(stderr, "Failed to initialize MilkDrop renderer\n");
        return 1;
    }

    printf("MilkDrop renderer initialized.\n");
    printf("  Render targets: %dx%d\n", width, height);
    printf("  Controls: ESC=quit\n\n");

    // Draw initial test pattern into VS0 so we have content to warp
    {
        g_renderer.FillTestPattern();
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

        // Cap dt to avoid physics explosion on pause/resume
        if (dt > 0.1f) dt = 0.1f;

        // Simulate preset variables (will come from preset engine later)
        g_renderer.m_zoom  = 1.0f + sinf(time * 0.5f) * 0.3f;
        g_renderer.m_rot   = sinf(time * 0.3f) * 0.05f;
        g_renderer.m_warp  = 0.05f + fabsf(sinf(time * 0.2f)) * 0.15f;
        g_renderer.m_cx    = sinf(time * 0.1f) * 0.02f;
        g_renderer.m_cy    = cosf(time * 0.15f) * 0.02f;

        g_renderer.RenderFrame(time, dt);

        g_gl.SwapBuffers();

        // Frame rate limiting
        Uint32 frameTime = SDL_GetTicks() - lastFrameTime;
        if (frameTime < FRAME_DELAY_MS)
            SDL_Delay(FRAME_DELAY_MS - frameTime);
        lastFrameTime = SDL_GetTicks();

        // FPS counter
        frameCount++;
        if (now - fpsTimer >= 1000)
        {
            // printf("FPS: %d\n", frameCount);
            frameCount = 0;
            fpsTimer = now;
        }
    }

    g_gl.DestroyWindow();
    SDL_Quit();

    printf("\nMilkDrop3 shutdown complete.\n");
    return 0;
}
