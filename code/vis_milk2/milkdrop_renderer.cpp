#include "milkdrop_renderer.h"
#include <cstdio>
#include <algorithm>

MilkdropRenderer::MilkdropRenderer()
    : m_ctx(nullptr)
    , m_width(1024)
    , m_height(768)
    , m_current_vs(0)
    , m_warp_shader(0)
    , m_comp_shader(0)
    , m_blur1_shader(0)
    , m_blur2_shader(0)
    , m_initialized(false)
    , m_first_frame(true)
{
    m_vs_tex[0] = m_vs_tex[1] = 0;
    m_vs_fbo[0] = m_vs_fbo[1] = 0;
    for (int i = 0; i < NUM_BLUR_TEX; i++)
    {
        m_blur_tex[i] = 0;
        m_blur_fbo[i] = 0;
        m_blur_w[i] = m_blur_h[i] = 0;
    }
}

MilkdropRenderer::~MilkdropRenderer()
{
    DestroyRenderTargets();
    if (m_warp_shader) m_ctx->DestroyShader(m_warp_shader);
    if (m_comp_shader) m_ctx->DestroyShader(m_comp_shader);
    if (m_blur1_shader) m_ctx->DestroyShader(m_blur1_shader);
    if (m_blur2_shader) m_ctx->DestroyShader(m_blur2_shader);
}

bool MilkdropRenderer::Init(GLContext* ctx, const char* data_path)
{
    m_ctx = ctx;
    m_width = ctx->Width();
    m_height = ctx->Height();

    // Load shader templates from disk
    if (!m_shaders.LoadTemplates(data_path))
    {
        fprintf(stderr, "Failed to load shader templates from %s\n", data_path);
        return false;
    }

    // Build and compile the warp pixel shader
    {
        std::vector<ShaderParamInfo> params;
        m_warp_shader = m_shaders.CompilePixelShader(
            m_shaders.m_warp_ps_default.c_str(),
            "#define rad _rad_ang.x\n"
            "#define ang _rad_ang.y\n"
            "#define uv _uv.xy\n"
            "#define uv_orig _uv.zw\n",
            params);
        if (!m_warp_shader)
        {
            fprintf(stderr, "Failed to compile warp shader\n");
            return false;
        }
        // Cache the texsize uniform location
        m_warp_u_texsize = glGetUniformLocation(m_warp_shader, "_c7");
    }

    // Build and compile the composite pixel shader
    {
        std::vector<ShaderParamInfo> params;
        m_comp_shader = m_shaders.CompilePixelShader(
            m_shaders.m_comp_ps_default.c_str(),
            "#define rad _rad_ang.x\n"
            "#define ang _rad_ang.y\n"
            "#define uv _uv.xy\n"
            "#define uv_orig _uv.xy\n"
            "#define hue_shader _vDiffuse.xyz\n",
            params);
        if (!m_comp_shader)
        {
            fprintf(stderr, "Failed to compile composite shader\n");
            return false;
        }
        m_comp_u_texsize = glGetUniformLocation(m_comp_shader, "_c7");
    }

    // Build blur shaders using vs_src from templates
    {
        // Blur1 (horizontal)
        std::string vs_src = m_shaders.m_blur_vs_text;
        std::string fs_src = m_shaders.m_blur1_ps_text;
        std::string full_vs = "#version 330 core\n" + vs_src;
        std::string full_fs = "#version 330 core\n" + fs_src;

        GLuint vs = m_shaders.CompileShaderStage(full_vs.c_str(), GL_VERTEX_SHADER);
        GLuint fs = m_shaders.CompileShaderStage(full_fs.c_str(), GL_FRAGMENT_SHADER);
        if (vs && fs)
        {
            m_blur1_shader = m_shaders.LinkProgram(vs, fs);
            glDeleteShader(vs);
            glDeleteShader(fs);
        }
        if (!m_blur1_shader)
        {
            fprintf(stderr, "Failed to compile blur1 shader\n");
            return false;
        }
        m_blur1_u_c0 = glGetUniformLocation(m_blur1_shader, "_c0");
        m_blur1_u_c1 = glGetUniformLocation(m_blur1_shader, "_c1");
        m_blur1_u_c2 = glGetUniformLocation(m_blur1_shader, "_c2");
        m_blur1_u_c3 = glGetUniformLocation(m_blur1_shader, "_c3");
    }

    {
        // Blur2 (vertical)
        std::string vs_src = m_shaders.m_blur_vs_text;
        std::string fs_src = m_shaders.m_blur2_ps_text;
        std::string full_vs = "#version 330 core\n" + vs_src;
        std::string full_fs = "#version 330 core\n" + fs_src;

        GLuint vs = m_shaders.CompileShaderStage(full_vs.c_str(), GL_VERTEX_SHADER);
        GLuint fs = m_shaders.CompileShaderStage(full_fs.c_str(), GL_FRAGMENT_SHADER);
        if (vs && fs)
        {
            m_blur2_shader = m_shaders.LinkProgram(vs, fs);
            glDeleteShader(vs);
            glDeleteShader(fs);
        }
        if (!m_blur2_shader)
        {
            fprintf(stderr, "Failed to compile blur2 shader\n");
            return false;
        }
        m_blur2_u_c0 = glGetUniformLocation(m_blur2_shader, "_c0");
        m_blur2_u_c5 = glGetUniformLocation(m_blur2_shader, "_c5");
        m_blur2_u_c6 = glGetUniformLocation(m_blur2_shader, "_c6");
    }

    if (!CreateRenderTargets())
    {
        fprintf(stderr, "Failed to create render targets\n");
        return false;
    }

    m_initialized = true;
    m_first_frame = true;
    return true;
}

void MilkdropRenderer::Resize(int w, int h)
{
    m_width = w;
    m_height = h;
    DestroyRenderTargets();
    CreateRenderTargets();
}

bool MilkdropRenderer::CreateRenderTargets()
{
    // Create VS render targets (full resolution)
    for (int i = 0; i < 2; i++)
    {
        m_vs_tex[i] = m_ctx->CreateTexture(m_width, m_height,
            GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE);
        if (!m_vs_tex[i]) return false;
        m_vs_fbo[i] = m_ctx->CreateFBO(m_vs_tex[i]);
        if (!m_vs_fbo[i]) return false;
    }

    // Create blur textures at progressive sizes
    int bw = m_width / 2;
    int bh = m_height / 2;
    for (int i = 0; i < NUM_BLUR_TEX; i++)
    {
        m_blur_w[i] = std::max(1, bw);
        m_blur_h[i] = std::max(1, bh);
        m_blur_tex[i] = m_ctx->CreateTexture(m_blur_w[i], m_blur_h[i],
            GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE);
        if (!m_blur_tex[i]) return false;
        m_blur_fbo[i] = m_ctx->CreateFBO(m_blur_tex[i]);
        if (!m_blur_fbo[i]) return false;

        if (i % 2 == 1)
        {
            bw /= 2;
            bh /= 2;
        }
    }

    return true;
}

void MilkdropRenderer::DestroyRenderTargets()
{
    for (int i = 0; i < 2; i++)
    {
        m_ctx->DestroyFBO(m_vs_fbo[i]);
        m_ctx->DestroyTexture(m_vs_tex[i]);
        m_vs_fbo[i] = 0;
        m_vs_tex[i] = 0;
    }
    for (int i = 0; i < NUM_BLUR_TEX; i++)
    {
        m_ctx->DestroyFBO(m_blur_fbo[i]);
        m_ctx->DestroyTexture(m_blur_tex[i]);
        m_blur_fbo[i] = 0;
        m_blur_tex[i] = 0;
    }
}

void MilkdropRenderer::RenderFrame(float time, float dt)
{
    if (!m_initialized) return;

    // Clear the VS1 target (write target)
    m_ctx->BindFBO(m_vs_fbo[1]);
    m_ctx->SetViewport(0, 0, m_width, m_height);
    m_ctx->Clear(0.0f, 0.0f, 0.0f, 0.0f);

    if (m_first_frame)
    {
        // On first frame, just clear VS0 too
        m_ctx->BindFBO(m_vs_fbo[0]);
        m_ctx->Clear(0.0f, 0.0f, 0.0f, 0.0f);
        m_ctx->BindFBO(m_vs_fbo[1]);
        m_first_frame = false;
    }

    // Step 1: Render warp pass (sample from VS0, write to VS1)
    RenderWarpPass();

    // Step 2: Blur passes (sample from VS1, write to blur chain)
    RenderBlurPasses();

    // Step 3: Composite to screen
    m_ctx->BindDefaultFBO();
    m_ctx->SetViewport(0, 0, m_ctx->Width(), m_ctx->Height());
    RenderCompositePass();

    // Step 4: Swap VS buffers
    std::swap(m_vs_tex[0], m_vs_tex[1]);
    std::swap(m_vs_fbo[0], m_vs_fbo[1]);
}

void MilkdropRenderer::RenderWarpPass()
{
    m_ctx->BindFBO(m_vs_fbo[1]);

    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);

    m_ctx->UseShader(m_warp_shader);

    // Bind VS0 texture as sampler_main (texture unit 0)
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_vs_tex[0]);
    glUniform1i(glGetUniformLocation(m_warp_shader, "sampler_main"), 0);

    // Set texsize constant (_c7)
    m_shaders.SetVec4(m_warp_u_texsize,
        (float)m_width, (float)m_height,
        1.0f / (float)m_width, 1.0f / (float)m_height);

    m_ctx->DrawFullscreenQuad();
    m_ctx->UseShader(0);
}

void MilkdropRenderer::RenderBlurPasses()
{
    // Gaussian blur weights (default from MilkDrop)
    const float w[8] = { 4.0f, 3.8f, 3.5f, 2.9f, 1.9f, 1.2f, 0.7f, 0.3f };
    const float w1 = w[0] + w[1];
    const float w2 = w[2] + w[3];
    const float w3 = w[4] + w[5];
    const float w4 = w[6] + w[7];
    const float d1 = 0 + 2 * w[1] / w1;
    const float d2 = 2 + 2 * w[3] / w2;
    const float d3 = 4 + 2 * w[5] / w3;
    const float d4 = 6 + 2 * w[7] / w4;
    const float w_div = 0.5f / (w1 + w2 + w3 + w4);

    const float w_div_v = 1.0f / ((w1 + w2) * 2);

    float edge_c1 = 1.0f;
    float edge_c2 = 0.0f;
    float edge_c3 = 5.0f;

    for (int i = 0; i < NUM_BLUR_TEX; i++)
    {
        GLuint src_tex;
        int src_w, src_h;

        if (i == 0)
        {
            // First blur reads from VS1
            src_tex = m_vs_tex[1];
            src_w = m_width;
            src_h = m_height;
        }
        else
        {
            src_tex = m_blur_tex[i - 1];
            src_w = m_blur_w[i - 1];
            src_h = m_blur_h[i - 1];
        }

        m_ctx->BindFBO(m_blur_fbo[i]);
        m_ctx->SetViewport(0, 0, m_blur_w[i], m_blur_h[i]);
        m_ctx->Clear(0.0f, 0.0f, 0.0f, 0.0f);

        glDisable(GL_BLEND);
        glDisable(GL_DEPTH_TEST);

        if (i % 2 == 0)
        {
            // Horizontal pass (blur1 shader)
            m_ctx->UseShader(m_blur1_shader);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, src_tex);
            glUniform1i(glGetUniformLocation(m_blur1_shader, "sampler_main"), 0);

            m_shaders.SetVec4(m_blur1_u_c0,
                (float)src_w, (float)src_h,
                1.0f / (float)src_w, 1.0f / (float)src_h);
            m_shaders.SetVec4(m_blur1_u_c1, w1, w2, w3, w4);
            m_shaders.SetVec4(m_blur1_u_c2, d1, d2, d3, d4);
            m_shaders.SetVec4(m_blur1_u_c3, 1.0f, 0.0f, w_div, 0.0f);
        }
        else
        {
            // Vertical pass (blur2 shader)
            m_ctx->UseShader(m_blur2_shader);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, src_tex);
            glUniform1i(glGetUniformLocation(m_blur2_shader, "sampler_main"), 0);

            m_shaders.SetVec4(m_blur2_u_c0,
                (float)src_w, (float)src_h,
                1.0f / (float)src_w, 1.0f / (float)src_h);
            m_shaders.SetVec4(m_blur2_u_c5, w1, w2, d1, d2);
            m_shaders.SetVec4(m_blur2_u_c6, w_div_v, edge_c1, edge_c2, edge_c3);
        }

        m_ctx->DrawFullscreenQuad();
        m_ctx->UseShader(0);
    }
}

void MilkdropRenderer::RenderCompositePass()
{
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);

    m_ctx->UseShader(m_comp_shader);

    // Bind VS1 texture as sampler_main
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_vs_tex[1]);
    glUniform1i(glGetUniformLocation(m_comp_shader, "sampler_main"), 0);

    // Set texsize constant (_c7)
    m_shaders.SetVec4(m_comp_u_texsize,
        (float)m_width, (float)m_height,
        1.0f / (float)m_width, 1.0f / (float)m_height);

    m_ctx->DrawFullscreenQuad();
    m_ctx->UseShader(0);
}
