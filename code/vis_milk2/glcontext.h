#ifndef __GL_CONTEXT_H__
#define __GL_CONTEXT_H__ 1

#include <GL/glew.h>
#include <SDL2/SDL.h>
#include <glm/glm.hpp>
#include <string>
#include <vector>

#define MAX_TEX 32

struct GLTexture
{
    GLuint id;
    int w, h;
    GLenum target;
};

struct GLShader
{
    GLuint program;
    GLuint vs;
    GLuint fs;
    std::string name;
};

class GLContext
{
public:
    GLContext();
    ~GLContext();

    // Window management
    bool CreateWindow(int w, int h, bool fullscreen);
    void DestroyWindow();
    void SwapBuffers();
    bool ProcessEvents();
    SDL_Window* GetWindow() { return m_window; }

    // OpenGL state
    void SetViewport(int x, int y, int w, int h);
    void Clear(float r, float g, float b, float a);

    // Texture management
    GLuint CreateTexture(int w, int h, GLenum internal, GLenum format, GLenum type,
                         const void* data = nullptr, GLenum min_filter = GL_LINEAR,
                         GLenum mag_filter = GL_LINEAR);
    void DestroyTexture(GLuint id);
    void BindTexture(int unit, GLuint id);

    // FBO management
    GLuint CreateFBO(GLuint color_tex, GLuint depth_tex = 0);
    void DestroyFBO(GLuint fbo);
    void BindFBO(GLuint fbo);
    void BindDefaultFBO();

    // Shader management
    GLuint CompileShader(const char* src, GLenum type);
    GLuint LinkProgram(GLuint vs, GLuint fs);
    GLuint CreateShader(const char* vs_src, const char* fs_src);
    void DestroyShader(GLuint program);
    void UseShader(GLuint program);
    void SetUniform(GLuint program, const char* name, float x);
    void SetUniform(GLuint program, const char* name, float x, float y);
    void SetUniform(GLuint program, const char* name, float x, float y, float z);
    void SetUniform(GLuint program, const char* name, float x, float y, float z, float w);
    void SetUniform(GLuint program, const char* name, const glm::vec2& v);
    void SetUniform(GLuint program, const char* name, const glm::vec3& v);
    void SetUniform(GLuint program, const char* name, const glm::vec4& v);
    void SetUniform(GLuint program, const char* name, const glm::mat4& m);
    void SetUniform(GLuint program, const char* name, const glm::mat4x3& m);

    // Fullscreen quad rendering
    GLuint CreateFullscreenQuad();
    void DrawFullscreenQuad();

    // Error checking
    bool CheckError(const char* context);

    // Window state
    int Width() const { return m_width; }
    int Height() const { return m_height; }
    float AspectX() const { return m_aspect_x; }
    float AspectY() const { return m_aspect_y; }

private:
    SDL_Window* m_window;
    SDL_GLContext m_gl_context;
    int m_width;
    int m_height;
    float m_aspect_x;
    float m_aspect_y;
    bool m_quit;

    GLuint m_fullscreen_quad_vao;
    GLuint m_fullscreen_quad_vbo;
};

#endif // __GL_CONTEXT_H__
