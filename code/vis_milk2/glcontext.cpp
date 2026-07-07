#include "glcontext.h"
#include <cstdio>
#include <cstdlib>
#include <glm/gtc/type_ptr.hpp>

GLContext::GLContext()
    : m_window(nullptr)
    , m_gl_context(nullptr)
    , m_width(1024)
    , m_height(768)
    , m_aspect_x(1.0f)
    , m_aspect_y(1.0f)
    , m_quit(false)
    , m_fullscreen_quad_vao(0)
    , m_fullscreen_quad_vbo(0)
{
}

GLContext::~GLContext()
{
    DestroyWindow();
}

bool GLContext::CreateWindow(int w, int h, bool fullscreen)
{
    m_width = w;
    m_height = h;
    m_aspect_x = (float)h / (float)w;
    m_aspect_y = 1.0f;

    Uint32 flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE;
    if (fullscreen)
        flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;

    m_window = SDL_CreateWindow(
        "MilkDrop3",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        w, h, flags);

    if (!m_window)
    {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        return false;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    m_gl_context = SDL_GL_CreateContext(m_window);
    if (!m_gl_context)
    {
        fprintf(stderr, "SDL_GL_CreateContext failed: %s\n", SDL_GetError());
        return false;
    }

    SDL_GL_MakeCurrent(m_window, m_gl_context);

    glewExperimental = GL_TRUE;
    GLenum glew_err = glewInit();
    if (glew_err != GLEW_OK)
    {
        fprintf(stderr, "glewInit failed: %s\n", glewGetErrorString(glew_err));
        return false;
    }

    // Clear initial error from glewExperimental
    glGetError();

    const char* sdl_video = SDL_GetCurrentVideoDriver();
    fprintf(stdout, "  Video driver: %s\n", sdl_video ? sdl_video : "(unknown)");
    fprintf(stdout, "  OpenGL: %s | GLSL: %s\n  Vendor: %s\n  Renderer: %s\n",
            glGetString(GL_VERSION), glGetString(GL_SHADING_LANGUAGE_VERSION),
            glGetString(GL_VENDOR), glGetString(GL_RENDERER));

    SDL_GL_SetSwapInterval(1); // vsync on

    m_fullscreen_quad_vao = CreateFullscreenQuad();

    return true;
}

void GLContext::DestroyWindow()
{
    if (m_fullscreen_quad_vbo)
    {
        glDeleteBuffers(1, &m_fullscreen_quad_vbo);
        m_fullscreen_quad_vbo = 0;
    }
    if (m_fullscreen_quad_vao)
    {
        glDeleteVertexArrays(1, &m_fullscreen_quad_vao);
        m_fullscreen_quad_vao = 0;
    }

    if (m_gl_context)
    {
        SDL_GL_DeleteContext(m_gl_context);
        m_gl_context = nullptr;
    }
    if (m_window)
    {
        SDL_DestroyWindow(m_window);
        m_window = nullptr;
    }
}

void GLContext::SwapBuffers()
{
    SDL_GL_SwapWindow(m_window);
}

bool GLContext::ProcessEvents()
{
    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        switch (event.type)
        {
        case SDL_QUIT:
            m_quit = true;
            break;
        case SDL_KEYDOWN:
            if (event.key.keysym.sym == SDLK_ESCAPE)
                m_quit = true;
            break;
        case SDL_WINDOWEVENT:
            if (event.window.event == SDL_WINDOWEVENT_RESIZED)
            {
                m_width = event.window.data1;
                m_height = event.window.data2;
                m_aspect_x = (float)m_height / (float)m_width;
                SetViewport(0, 0, m_width, m_height);
            }
            break;
        }
    }
    return !m_quit;
}

void GLContext::SetViewport(int x, int y, int w, int h)
{
    glViewport(x, y, w, h);
}

void GLContext::Clear(float r, float g, float b, float a)
{
    glClearColor(r, g, b, a);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

GLuint GLContext::CreateTexture(int w, int h, GLenum internal, GLenum format,
                                 GLenum type, const void* data,
                                 GLenum min_filter, GLenum mag_filter)
{
    GLuint id;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glTexImage2D(GL_TEXTURE_2D, 0, internal, w, h, 0, format, type, data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min_filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag_filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    return id;
}

void GLContext::DestroyTexture(GLuint id)
{
    if (id)
        glDeleteTextures(1, &id);
}

void GLContext::BindTexture(int unit, GLuint id)
{
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, id);
}

GLuint GLContext::CreateFBO(GLuint color_tex, GLuint depth_tex)
{
    GLuint fbo;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    if (color_tex)
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color_tex, 0);
    if (depth_tex)
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth_tex, 0);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE)
    {
        fprintf(stderr, "FBO incomplete: 0x%x\n", status);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDeleteFramebuffers(1, &fbo);
        return 0;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return fbo;
}

void GLContext::DestroyFBO(GLuint fbo)
{
    if (fbo)
        glDeleteFramebuffers(1, &fbo);
}

void GLContext::BindFBO(GLuint fbo)
{
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
}

void GLContext::BindDefaultFBO()
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

GLuint GLContext::CompileShader(const char* src, GLenum type)
{
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    GLint compiled = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled)
    {
        GLint info_len = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &info_len);
        std::vector<char> info(info_len + 1);
        glGetShaderInfoLog(shader, info_len, nullptr, info.data());
        fprintf(stderr, "Shader compile error (%s):\n%s\n",
                type == GL_VERTEX_SHADER ? "VS" : "FS", info.data());
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

GLuint GLContext::LinkProgram(GLuint vs, GLuint fs)
{
    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);

    GLint linked = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (!linked)
    {
        GLint info_len = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &info_len);
        std::vector<char> info(info_len + 1);
        glGetProgramInfoLog(program, info_len, nullptr, info.data());
        fprintf(stderr, "Program link error:\n%s\n", info.data());
        glDeleteProgram(program);
        return 0;
    }
    return program;
}

GLuint GLContext::CreateShader(const char* vs_src, const char* fs_src)
{
    GLuint vs = CompileShader(vs_src, GL_VERTEX_SHADER);
    if (!vs) return 0;
    GLuint fs = CompileShader(fs_src, GL_FRAGMENT_SHADER);
    if (!fs)
    {
        glDeleteShader(vs);
        return 0;
    }
    GLuint program = LinkProgram(vs, fs);
    glDeleteShader(vs);
    glDeleteShader(fs);
    return program;
}

void GLContext::DestroyShader(GLuint program)
{
    if (program)
        glDeleteProgram(program);
}

void GLContext::UseShader(GLuint program)
{
    glUseProgram(program);
}

void GLContext::SetUniform(GLuint program, const char* name, float x)
{
    GLint loc = glGetUniformLocation(program, name);
    if (loc >= 0) glUniform1f(loc, x);
}

void GLContext::SetUniform(GLuint program, const char* name, float x, float y)
{
    GLint loc = glGetUniformLocation(program, name);
    if (loc >= 0) glUniform2f(loc, x, y);
}

void GLContext::SetUniform(GLuint program, const char* name, float x, float y, float z)
{
    GLint loc = glGetUniformLocation(program, name);
    if (loc >= 0) glUniform3f(loc, x, y, z);
}

void GLContext::SetUniform(GLuint program, const char* name, float x, float y, float z, float w)
{
    GLint loc = glGetUniformLocation(program, name);
    if (loc >= 0) glUniform4f(loc, x, y, z, w);
}

void GLContext::SetUniform(GLuint program, const char* name, const glm::vec2& v)
{
    GLint loc = glGetUniformLocation(program, name);
    if (loc >= 0) glUniform2fv(loc, 1, glm::value_ptr(v));
}

void GLContext::SetUniform(GLuint program, const char* name, const glm::vec3& v)
{
    GLint loc = glGetUniformLocation(program, name);
    if (loc >= 0) glUniform3fv(loc, 1, glm::value_ptr(v));
}

void GLContext::SetUniform(GLuint program, const char* name, const glm::vec4& v)
{
    GLint loc = glGetUniformLocation(program, name);
    if (loc >= 0) glUniform4fv(loc, 1, glm::value_ptr(v));
}

void GLContext::SetUniform(GLuint program, const char* name, const glm::mat4& m)
{
    GLint loc = glGetUniformLocation(program, name);
    if (loc >= 0) glUniformMatrix4fv(loc, 1, GL_FALSE, glm::value_ptr(m));
}

void GLContext::SetUniform(GLuint program, const char* name, const glm::mat4x3& m)
{
    GLint loc = glGetUniformLocation(program, name);
    if (loc >= 0) glUniformMatrix4x3fv(loc, 1, GL_FALSE, glm::value_ptr(m));
}

GLuint GLContext::CreateFullscreenQuad()
{
    float vertices[] = {
        -1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
         1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
         1.0f,  1.0f, 0.0f, 1.0f, 1.0f,
        -1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
    };
    unsigned int indices[] = { 0, 1, 2, 0, 2, 3 };

    GLuint vao, vbo, ebo;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);

    glBindVertexArray(vao);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);

    m_fullscreen_quad_vbo = vbo;
    return vao;
}

void GLContext::DrawFullscreenQuad()
{
    glBindVertexArray(m_fullscreen_quad_vao);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

bool GLContext::CheckError(const char* context)
{
    GLenum err = glGetError();
    if (err != GL_NO_ERROR)
    {
        fprintf(stderr, "GL error 0x%x at %s\n", err, context);
        return false;
    }
    return true;
}
