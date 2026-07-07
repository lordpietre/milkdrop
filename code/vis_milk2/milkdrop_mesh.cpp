#include "milkdrop_mesh.h"
#include <cmath>
#include <cstring>


static const float pi = 3.14159265358979323846f;

MilkdropMesh::MilkdropMesh()
    : m_grid_x(64), m_grid_y(48)
    , m_aspect_x(1.0f), m_aspect_y(1.0f)
    , m_vao(0), m_vbo(0), m_ebo(0)
    , m_buffers_dirty(true)
{}

MilkdropMesh::~MilkdropMesh()
{ Destroy(); }

void MilkdropMesh::Destroy()
{
    if (m_vao) glDeleteVertexArrays(1, &m_vao);
    if (m_vbo) glDeleteBuffers(1, &m_vbo);
    if (m_ebo) glDeleteBuffers(1, &m_ebo);
    m_vao = m_vbo = m_ebo = 0;
    m_verts.clear();
    m_indices.clear();
    m_vert_info.clear();
}

bool MilkdropMesh::Init(int grid_x, int grid_y, float aspect_x, float aspect_y)
{
    m_grid_x = grid_x;
    m_grid_y = grid_y;
    m_aspect_x = aspect_x;
    m_aspect_y = aspect_y;

    int nVerts = (m_grid_x + 1) * (m_grid_y + 1);
    m_verts.resize(nVerts);
    m_vert_info.resize(nVerts);

    for (int y = 0; y <= m_grid_y; y++)
    {
        float fy = (float)y / (float)m_grid_y;
        for (int x = 0; x <= m_grid_x; x++)
        {
            float fx = (float)x / (float)m_grid_x;
            int idx = y * (m_grid_x + 1) + x;

            GridVertex& v = m_verts[idx];
            v.x = (fx - 0.5f) * 2.0f;
            v.y = (fy - 0.5f) * 2.0f;
            v.z = 0.0f;

            v.diffuse = 0xFFFFFFFF;

            v.tu = fx;
            v.tv = fy;
            v.tu_orig = fx;
            v.tv_orig = fy;

            v.rad = sqrtf((fx - 0.5f) * 2.0f * (fx - 0.5f) * 2.0f +
                          (fy - 0.5f) * 2.0f * (fy - 0.5f) * 2.0f) /
                    sqrtf(2.0f);
            v.ang = atan2f((fy - 0.5f) * m_aspect_y, (fx - 0.5f) * m_aspect_x);

            VertInfo& vi = m_vert_info[idx];
            vi.rad = v.rad;
            vi.ang = v.ang;
            vi.a = 1.0f;
            vi.c = 0.0f;
        }
    }

    // Build index buffer (triangle strip)
    int nIndices = (m_grid_y) * (m_grid_x + 1) * 2 + (m_grid_y) * 2;
    m_indices.reserve(nIndices);

    for (int y = 0; y < m_grid_y; y++)
    {
        for (int x = 0; x <= m_grid_x; x++)
        {
            int idx0 = y * (m_grid_x + 1) + x;
            int idx1 = (y + 1) * (m_grid_x + 1) + x;
            m_indices.push_back(idx0);
            m_indices.push_back(idx1);
        }
        // Degenerate strip link
        if (y < m_grid_y - 1)
        {
            int last = (y + 1) * (m_grid_x + 1) + m_grid_x;
            int first = (y + 1) * (m_grid_x + 1);
            m_indices.push_back(last);
            m_indices.push_back(first);
        }
    }

    m_buffers_dirty = true;
    return true;
}

void MilkdropMesh::ComputeGridUVs(float time, float decay, float zoom,
                                   float rot, float cx, float cy,
                                   float dx, float dy,
                                   float warp, float sx, float sy)
{
    // Port of ComputeGridAlphaValues simplified:
    // Compute per-vertex UV distortion using the warp variables

    // Wrap zoom to prevent extreme values each frame
    zoom = fmodf(zoom, 8.0f);
    if (zoom < -4.0f) zoom += 8.0f;
    if (zoom >  4.0f) zoom -= 8.0f;

    // Clamp decay
    decay = fmaxf(0.0f, fminf(1.0f, decay));

    int nVerts = (m_grid_x + 1) * (m_grid_y + 1);
    for (int i = 0; i < nVerts; i++)
    {
        GridVertex& v = m_verts[i];
        VertInfo& vi = m_vert_info[i];

        // Start from original UV
        float u = v.tu_orig;
        float vv = v.tv_orig;

        // Apply warp with polar-coordinate-based distortion
        float rad = vi.rad;
        float ang = vi.ang;

        // Basic warp: displace UV based on radial distance
        // This mimics what the preset 'warp' equation would do
        float w = warp * 0.1f;

        // Radial zoom
        u = 0.5f + (u - 0.5f) * (1.0f / zoom);
        vv = 0.5f + (vv - 0.5f) * (1.0f / zoom);

        // Rotational warping
        float cos_r = cosf(rot);
        float sin_r = sinf(rot);
        float ru = (u - 0.5f) * cos_r - (vv - 0.5f) * sin_r + 0.5f;
        float rv = (u - 0.5f) * sin_r + (vv - 0.5f) * cos_r + 0.5f;
        u = ru;
        vv = rv;

        // Center offset
        u += cx;
        vv += cy;

        // Radial wave warp
        u += sinf(rad * 6.28318f * 4.0f + time) * w;
        vv += cosf(rad * 6.28318f * 4.0f + time * 1.3f) * w;

        // Angular wave
        u += sinf(ang * 2.0f + time * 2.0f) * w * 0.5f;
        vv += cosf(ang * 2.0f + time * 1.7f) * w * 0.5f;

        // Apply stretch
        u = (u - 0.5f) * sx + 0.5f;
        vv = (vv - 0.5f) * sy + 0.5f;

        // Wrap mode
        if (u > 1.0f) u -= (int)u;
        if (u < 0.0f) u += 1.0f - (int)u;
        if (vv > 1.0f) vv -= (int)vv;
        if (vv < 0.0f) vv += 1.0f - (int)vv;

        v.tu = u;
        v.tv = vv;
    }

    m_buffers_dirty = true;
}

void MilkdropMesh::Render(GLuint shader, GLuint texture)
{
    // Upload geometry if dirty
    if (m_buffers_dirty && !m_verts.empty())
    {
        if (!m_vao)
            glGenVertexArrays(1, &m_vao);
        if (!m_vbo)
            glGenBuffers(1, &m_vbo);
        if (!m_ebo)
            glGenBuffers(1, &m_ebo);

        glBindVertexArray(m_vao);

        glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
        glBufferData(GL_ARRAY_BUFFER,
                     m_verts.size() * sizeof(GridVertex),
                     m_verts.data(), GL_DYNAMIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                     m_indices.size() * sizeof(unsigned int),
                     m_indices.data(), GL_STATIC_DRAW);

        // Position
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
                              sizeof(GridVertex), (void*)offsetof(GridVertex, x));
        glEnableVertexAttribArray(0);

        // Diffuse
        glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE,
                              sizeof(GridVertex), (void*)offsetof(GridVertex, diffuse));
        glEnableVertexAttribArray(1);

        // Texcoord
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE,
                              sizeof(GridVertex), (void*)offsetof(GridVertex, tu));
        glEnableVertexAttribArray(2);

        glBindVertexArray(0);
        m_buffers_dirty = false;
    }

    GLint prev_tex;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &prev_tex);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);

    glUseProgram(shader);
    glUniform1i(glGetUniformLocation(shader, "sampler_main"), 0);

    glBindVertexArray(m_vao);
    glDrawElements(GL_TRIANGLE_STRIP, (GLsizei)m_indices.size(),
                   GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);

    glUseProgram(0);

    glBindTexture(GL_TEXTURE_2D, prev_tex);
}

void MilkdropMesh::RenderFullscreenQuad(GLuint shader, GLuint texture)
{
    glUseProgram(shader);
    glUniform1i(glGetUniformLocation(shader, "sampler_main"), 0);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);

    RenderQuad();

    glUseProgram(0);
}

void MilkdropMesh::RenderQuad()
{
    static GLuint s_vao = 0;
    static GLuint s_vbo = 0;

    if (!s_vao)
    {
        float verts[] = {
            -1, -1, 0, 0, 0,
             1, -1, 0, 1, 0,
             1,  1, 0, 1, 1,
            -1, -1, 0, 0, 0,
             1,  1, 0, 1, 1,
            -1,  1, 0, 0, 1,
        };

        glGenVertexArrays(1, &s_vao);
        glGenBuffers(1, &s_vbo);

        glBindVertexArray(s_vao);
        glBindBuffer(GL_ARRAY_BUFFER, s_vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(2);

        glBindVertexArray(0);
    }

    glBindVertexArray(s_vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}
