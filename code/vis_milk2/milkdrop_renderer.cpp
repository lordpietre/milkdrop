#include "milkdrop_renderer.h"
#include "preset_engine.h"
#include <cstdio>
#include <algorithm>

MilkdropRenderer::MilkdropRenderer()
    : m_ctx(nullptr)
    , m_width(1024)
    , m_height(768)
    , m_current_vs(0)
    , m_warp_shader(0)
    , m_warp_shader_custom(0)
    , m_comp_shader(0)
    , m_comp_shader_custom(0)
    , m_blur1_shader(0)
    , m_blur2_shader(0)
    , m_preset_engine(nullptr)
    , m_initialized(false)
    , m_first_frame(true)
    , m_decay(0.004f)
    , m_zoom(1.0f)
    , m_rot(0.0f)
    , m_cx(0.0f), m_cy(0.0f)
    , m_dx(0.0f), m_dy(0.0f)
    , m_warp(0.1f)
    , m_sx(0.0f), m_sy(0.0f)
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
    if (m_warp_shader_custom) m_ctx->DestroyShader(m_warp_shader_custom);
    if (m_comp_shader) m_ctx->DestroyShader(m_comp_shader);
    if (m_comp_shader_custom) m_ctx->DestroyShader(m_comp_shader_custom);
    if (m_blur1_shader) m_ctx->DestroyShader(m_blur1_shader);
    if (m_blur2_shader) m_ctx->DestroyShader(m_blur2_shader);
}

bool MilkdropRenderer::Init(GLContext* ctx, const char* data_path)
{
    m_ctx = ctx;
    m_width = ctx->Width();
    m_height = ctx->Height();

    // Init mesh
    float aspect_x = 1.0f;
    float aspect_y = (float)m_height / (float)m_width;
    if (!m_mesh.Init(64, 48, aspect_x, aspect_y))
    {
        fprintf(stderr, "Failed to init mesh\n");
        return false;
    }

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
        std::string full_vs = "#version 330 core\n" + m_shaders.m_blur_vs_text;
        std::string fs_src = "#version 330 core\n" + m_shaders.m_blur1_ps_text;

        GLuint vs = m_shaders.CompileShaderStage(full_vs.c_str(), GL_VERTEX_SHADER);
        GLuint fs = m_shaders.CompileShaderStage(fs_src.c_str(), GL_FRAGMENT_SHADER);
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
        std::string full_vs = "#version 330 core\n" + m_shaders.m_blur_vs_text;
        std::string fs_src = "#version 330 core\n" + m_shaders.m_blur2_ps_text;

        GLuint vs = m_shaders.CompileShaderStage(full_vs.c_str(), GL_VERTEX_SHADER);
        GLuint fs = m_shaders.CompileShaderStage(fs_src.c_str(), GL_FRAGMENT_SHADER);
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

    float aspect_x = 1.0f;
    float aspect_y = (float)m_height / (float)m_width;
    m_mesh.Init(64, 48, aspect_x, aspect_y);
}

bool MilkdropRenderer::CompileWarpShader(const char* body)
{
    if (m_warp_shader_custom)
    {
        m_ctx->DestroyShader(m_warp_shader_custom);
        m_warp_shader_custom = 0;
    }

    std::vector<ShaderParamInfo> params;
    m_warp_shader_custom = m_shaders.CompilePixelShader(
        body,
        "#define rad _rad_ang.x\n"
        "#define ang _rad_ang.y\n"
        "#define uv _uv.xy\n"
        "#define uv_orig _uv.zw\n",
        params);

    if (!m_warp_shader_custom)
    {
        fprintf(stderr, "Failed to compile custom warp shader\n");
        return false;
    }
    return true;
}

bool MilkdropRenderer::CompileCompShader(const char* body)
{
    if (m_comp_shader_custom)
    {
        m_ctx->DestroyShader(m_comp_shader_custom);
        m_comp_shader_custom = 0;
    }

    std::vector<ShaderParamInfo> params;
    m_comp_shader_custom = m_shaders.CompilePixelShader(
        body,
        "#define rad _rad_ang.x\n"
        "#define ang _rad_ang.y\n"
        "#define uv _uv.xy\n"
        "#define uv_orig _uv.xy\n"
        "#define hue_shader _vDiffuse.xyz\n",
        params);

    if (!m_comp_shader_custom)
    {
        fprintf(stderr, "Failed to compile custom composite shader\n");
        return false;
    }
    return true;
}

void MilkdropRenderer::ResetShaders()
{
    if (m_warp_shader_custom)
    {
        m_ctx->DestroyShader(m_warp_shader_custom);
        m_warp_shader_custom = 0;
    }
    if (m_comp_shader_custom)
    {
        m_ctx->DestroyShader(m_comp_shader_custom);
        m_comp_shader_custom = 0;
    }
}

bool MilkdropRenderer::CreateRenderTargets()
{
    for (int i = 0; i < 2; i++)
    {
        m_vs_tex[i] = m_ctx->CreateTexture(m_width, m_height,
            GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE);
        if (!m_vs_tex[i]) return false;
        m_vs_fbo[i] = m_ctx->CreateFBO(m_vs_tex[i]);
        if (!m_vs_fbo[i]) return false;
    }

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

void MilkdropRenderer::FillTestPattern()
{
    // Simple VS + FS that draws a radial + checkerboard test pattern
    const char* vs_src =
        "#version 330 core\n"
        "layout(location = 0) in vec3 pos;\n"
        "layout(location = 2) in vec2 uv;\n"
        "out vec2 _uv;\n"
        "void main() {\n"
        "    _uv = uv;\n"
        "    gl_Position = vec4(pos, 1.0);\n"
        "}\n";

    const char* fs_src =
        "#version 330 core\n"
        "in vec2 _uv;\n"
        "out vec4 fragColor;\n"
        "void main() {\n"
        "    vec2 uv = _uv;\n"
        "    float r = length(uv - 0.5);\n"
        "    float ang = atan(uv.y - 0.5, uv.x - 0.5);\n"
        "    float checker = mod(floor(uv.x * 8.0) + floor(uv.y * 8.0), 2.0);\n"
        "    vec3 col = 0.5 + 0.5 * cos(ang * 4.0 + r * 10.0 + vec3(0.0, 2.1, 4.2));\n"
        "    col *= 0.7 + 0.3 * (1.0 - r * 1.4);\n"
        "    col *= 0.5 + 0.5 * checker;\n"
        "    fragColor = vec4(col, 1.0);\n"
        "}\n";

    GLuint vs = m_ctx->CompileShader(vs_src, GL_VERTEX_SHADER);
    GLuint fs = m_ctx->CompileShader(fs_src, GL_FRAGMENT_SHADER);
    if (!vs || !fs)
    {
        if (vs) glDeleteShader(vs);
        if (fs) glDeleteShader(fs);
        return;
    }
    GLuint prog = m_ctx->LinkProgram(vs, fs);
    glDeleteShader(vs);
    glDeleteShader(fs);
    if (!prog) return;

    // Render test pattern to VS0
    m_ctx->BindFBO(m_vs_fbo[0]);
    m_ctx->SetViewport(0, 0, m_width, m_height);
    m_ctx->Clear(0.0f, 0.0f, 0.0f, 0.0f);

    glUseProgram(prog);
    MilkdropMesh::RenderQuad();
    glUseProgram(0);

    m_ctx->DestroyShader(prog);

    // Also render to VS1 so the first warp has something to see
    m_ctx->BindFBO(m_vs_fbo[1]);
    m_ctx->SetViewport(0, 0, m_width, m_height);
    m_ctx->Clear(0.0f, 0.0f, 0.0f, 0.0f);

    glUseProgram(prog);
    MilkdropMesh::RenderQuad();
    glUseProgram(0);

    m_ctx->BindDefaultFBO();
}

void MilkdropRenderer::RenderFrame(float time, float dt)
{
    if (!m_initialized) return;

    m_ctx->BindFBO(m_vs_fbo[1]);
    m_ctx->SetViewport(0, 0, m_width, m_height);
    m_ctx->Clear(0.0f, 0.0f, 0.0f, 0.0f);

    if (m_first_frame)
    {
        m_ctx->BindFBO(m_vs_fbo[0]);
        m_ctx->Clear(0.0f, 0.0f, 0.0f, 0.0f);
        m_ctx->BindFBO(m_vs_fbo[1]);
        m_first_frame = false;
    }

    RenderWarpPass(time);
    RenderBlurPasses();

    m_ctx->BindDefaultFBO();
    m_ctx->SetViewport(0, 0, m_ctx->Width(), m_ctx->Height());
    RenderCompositePass();

    std::swap(m_vs_tex[0], m_vs_tex[1]);
    std::swap(m_vs_fbo[0], m_vs_fbo[1]);
}

void MilkdropRenderer::RenderWarpPass(float time)
{
    float decay = 1.0f - m_decay;
    if (decay < 0.0f) decay = 0.0f;
    if (decay > 1.0f) decay = 1.0f;

    int nVerts = m_mesh.GridVertsCount();
    const float* rad = m_mesh.GetRadArray();
    const float* ang = m_mesh.GetAngArray();
    const float* px = m_mesh.GetPosXArray();
    const float* py = m_mesh.GetPosYArray();

    // Per-vertex modifier arrays
    std::vector<float> per_zoom, per_rot, per_warp;
    std::vector<float> per_cx, per_cy, per_dx, per_dy, per_sx, per_sy;

    if (m_preset_engine && nVerts > 0)
    {
        per_zoom.resize(nVerts, 0.0f);
        per_rot.resize(nVerts, 0.0f);
        per_warp.resize(nVerts, 0.0f);
        per_cx.resize(nVerts, 0.0f);
        per_cy.resize(nVerts, 0.0f);
        per_dx.resize(nVerts, 0.0f);
        per_dy.resize(nVerts, 0.0f);
        per_sx.resize(nVerts, 0.0f);
        per_sy.resize(nVerts, 0.0f);

        m_preset_engine->EvaluatePerPixel(
            nVerts, rad, ang, px, py,
            per_zoom.data(), per_rot.data(), per_warp.data(),
            per_cx.data(), per_cy.data(),
            per_dx.data(), per_dy.data(),
            per_sx.data(), per_sy.data());
    }

    m_mesh.ComputeGridUVs(time, decay, m_zoom, m_rot,
                          m_cx, m_cy, m_dx, m_dy,
                          m_warp, m_sx, m_sy,
                          per_zoom.empty() ? nullptr : per_zoom.data(),
                          per_rot.empty()  ? nullptr : per_rot.data(),
                          per_warp.empty() ? nullptr : per_warp.data(),
                          per_cx.empty()   ? nullptr : per_cx.data(),
                          per_cy.empty()   ? nullptr : per_cy.data(),
                          per_dx.empty()   ? nullptr : per_dx.data(),
                          per_dy.empty()   ? nullptr : per_dy.data(),
                          per_sx.empty()   ? nullptr : per_sx.data(),
                          per_sy.empty()   ? nullptr : per_sy.data());

    m_ctx->BindFBO(m_vs_fbo[1]);
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);

    GLuint shader = m_warp_shader_custom ? m_warp_shader_custom : m_warp_shader;
    m_mesh.Render(shader, m_vs_tex[0]);
}

void MilkdropRenderer::RenderBlurPasses()
{
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

        MilkdropMesh::RenderQuad();
        m_ctx->UseShader(0);
    }
}

void MilkdropRenderer::RenderCompositePass()
{
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);

    GLuint shader = m_comp_shader_custom ? m_comp_shader_custom : m_comp_shader;
    m_ctx->UseShader(shader);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_vs_tex[1]);
    glUniform1i(glGetUniformLocation(shader, "sampler_main"), 0);

    GLint texsize_loc = glGetUniformLocation(shader, "_c7");
    if (texsize_loc >= 0)
    {
        m_shaders.SetVec4(texsize_loc,
            (float)m_width, (float)m_height,
            1.0f / (float)m_width, 1.0f / (float)m_height);
    }

    MilkdropMesh::RenderQuad();
    m_ctx->UseShader(0);
}
