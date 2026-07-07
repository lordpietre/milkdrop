#ifndef __MILKDROP_RENDERER_H__
#define __MILKDROP_RENDERER_H__ 1

#include "glcontext.h"
#include "glshader.h"
#include "milkdrop_mesh.h"
#include <glm/glm.hpp>
#include <string>
#include <vector>

#define NUM_BLUR_TEX 6

class PresetEngine;
class MilkdropRenderer
{
public:
    MilkdropRenderer();
    ~MilkdropRenderer();

    bool Init(GLContext* ctx, const char* data_path);
    void Resize(int w, int h);
    void RenderFrame(float time, float dt);
    void FillTestPattern();
    void SetPresetEngine(PresetEngine* engine) { m_preset_engine = engine; }

    // Per-frame preset variables (to be set by preset engine later)
    float m_decay;
    float m_zoom;
    float m_rot;
    float m_cx, m_cy;
    float m_dx, m_dy;
    float m_warp;
    float m_sx, m_sy;

private:
    bool CreateRenderTargets();
    void DestroyRenderTargets();
    void RenderWarpPass(float time);
    void RenderBlurPasses();
    void RenderCompositePass();

public:
    // Compile a custom warp shader from preset body text
    bool CompileWarpShader(const char* body);
    // Compile a custom composite shader from preset body text
    bool CompileCompShader(const char* body);
    // Reset to default shaders
    void ResetShaders();

    GLContext* m_ctx;
    GLShaderManager m_shaders;
    MilkdropMesh m_mesh;
    PresetEngine* m_preset_engine;

    // Render target textures and FBOs
    GLuint m_vs_tex[2];
    GLuint m_vs_fbo[2];
    GLuint m_blur_tex[NUM_BLUR_TEX];
    GLuint m_blur_fbo[NUM_BLUR_TEX];
    int m_blur_w[NUM_BLUR_TEX];
    int m_blur_h[NUM_BLUR_TEX];

    int m_width;
    int m_height;
    int m_current_vs;

    // Shader programs
    GLuint m_warp_shader;
    GLuint m_warp_shader_custom; // set from preset, 0 if none
    GLuint m_comp_shader;
    GLuint m_comp_shader_custom; // set from preset, 0 if none
    GLuint m_blur1_shader;
    GLuint m_blur2_shader;

    // Cached uniform locations
    GLint m_warp_u_texsize;
    GLint m_comp_u_texsize;
    GLint m_blur1_u_c0;
    GLint m_blur1_u_c1;
    GLint m_blur1_u_c2;
    GLint m_blur1_u_c3;
    GLint m_blur2_u_c0;
    GLint m_blur2_u_c5;
    GLint m_blur2_u_c6;

    bool m_initialized;
    bool m_first_frame;
};

#endif // __MILKDROP_RENDERER_H__
