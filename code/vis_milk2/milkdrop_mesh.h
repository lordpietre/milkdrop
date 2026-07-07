#ifndef __MILKDROP_MESH_H__
#define __MILKDROP_MESH_H__ 1

#include <GL/glew.h>
#include <glm/glm.hpp>
#include <vector>

#define MAX_GRID_X 192
#define MAX_GRID_Y 144

struct GridVertex
{
    float x, y, z;
    unsigned int diffuse;
    float tu, tv;
    float tu_orig, tv_orig;
    float rad, ang;
};

struct VertInfo
{
    float rad, ang, a, c;
};

struct SpriteVertex
{
    float x, y, z;
    unsigned int diffuse;
    float tu, tv;
};

class MilkdropMesh
{
public:
    MilkdropMesh();
    ~MilkdropMesh();

    bool Init(int grid_x, int grid_y, float aspect_x, float aspect_y);
    void Destroy();

    // Compute per-vertex UVs with basic warp effect
    void ComputeGridUVs(float time, float decay, float zoom,
                        float rot, float cx, float cy, float dx, float dy,
                        float warp, float sx, float sy);

    // Render the warped grid
    void Render(GLuint shader, GLuint texture);

    // Render fullscreen quad
    void RenderFullscreenQuad(GLuint shader, GLuint texture);

    // Render a fullscreen textured quad for blur/composite passes
    static void RenderQuad();

    // Getters
    int GridX() const { return m_grid_x; }
    int GridY() const { return m_grid_y; }

private:
    int m_grid_x;
    int m_grid_y;
    float m_aspect_x;
    float m_aspect_y;

    std::vector<GridVertex> m_verts;
    std::vector<VertInfo> m_vert_info;
    std::vector<unsigned int> m_indices;

    // OpenGL buffers
    GLuint m_vao;
    GLuint m_vbo;
    GLuint m_ebo;
    bool m_buffers_dirty;
};

#endif
