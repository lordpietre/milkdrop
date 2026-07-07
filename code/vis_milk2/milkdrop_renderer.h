#ifndef __MILKDROP_RENDERER_H__
#define __MILKDROP_RENDERER_H__ 1

#include "glcontext.h"
#include "glshader.h"
#include <glm/glm.hpp>
#include <string>
#include <vector>

#define NUM_BLUR_TEX 6

class MilkdropRenderer
{
public:
    MilkdropRenderer();
    ~MilkdropRenderer();

    bool Init(GLContext* ctx, const char* data_path);
    void Resize(int w, int h);
    void RenderFrame(float time, float dt);

private:
    bool CreateRenderTargets();
    void DestroyRenderTargets();
    void RenderWarpPass();
    void RenderBlurPasses();
    void RenderCompositePass();

    GLContext* m_ctx;
    GLShaderManager m_shaders;

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
    GLuint m_comp_shader;
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
