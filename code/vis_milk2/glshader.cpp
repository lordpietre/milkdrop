#include "glshader.h"
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <sstream>
#include <fstream>
#include <glm/gtc/type_ptr.hpp>

GLShaderManager::GLShaderManager()
{
}

GLShaderManager::~GLShaderManager()
{
}

std::string GLShaderManager::LoadFile(const char* path)
{
    std::ifstream file(path, std::ios::in | std::ios::binary);
    if (!file)
    {
        fprintf(stderr, "Failed to load shader file: %s\n", path);
        return "";
    }
    std::string content;
    file.seekg(0, std::ios::end);
    content.resize((size_t)file.tellg());
    file.seekg(0, std::ios::beg);
    file.read(&content[0], content.size());
    file.close();
    return content;
}

bool GLShaderManager::LoadTemplates(const char* data_path)
{
    std::string base = data_path;
    if (!base.empty() && base.back() != '/')
        base += '/';

    m_include_text    = LoadFile((base + "include.glsl").c_str());
    m_warp_vs_text    = LoadFile((base + "warp_vs.glsl").c_str());
    m_warp_ps_default = LoadFile((base + "warp_ps.glsl").c_str());
    m_comp_vs_text    = LoadFile((base + "comp_vs.glsl").c_str());
    m_comp_ps_default = LoadFile((base + "comp_ps.glsl").c_str());
    m_blur_vs_text    = LoadFile((base + "blur_vs.glsl").c_str());
    m_blur1_ps_text   = LoadFile((base + "blur1_ps.glsl").c_str());
    m_blur2_ps_text   = LoadFile((base + "blur2_ps.glsl").c_str());

    if (m_include_text.empty() || m_warp_vs_text.empty())
    {
        fprintf(stderr, "Failed to load required shader templates from %s\n", base.c_str());
        return false;
    }
    return true;
}

std::string GLShaderManager::BuildPShaderSource(const char* body, const char* defines)
{
    std::ostringstream ss;
    ss << "#version 330 core\n";
    ss << m_include_text << "\n";
    if (defines)
        ss << defines << "\n";
    ss << body << "\n";
    return ss.str();
}

std::string GLShaderManager::BuildVShaderSource(const char* body)
{
    std::ostringstream ss;
    ss << "#version 330 core\n";
    ss << body << "\n";
    return ss.str();
}

GLuint GLShaderManager::CompileShaderStage(const char* src, GLenum type)
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

GLuint GLShaderManager::LinkProgram(GLuint vs, GLuint fs)
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

GLuint GLShaderManager::CompilePixelShader(const char* body, const char* defines,
                                           std::vector<ShaderParamInfo>& params)
{
    std::string fs_src = BuildPShaderSource(body, defines);
    GLuint fs = CompileShaderStage(fs_src.c_str(), GL_FRAGMENT_SHADER);
    if (!fs) return 0;

    // Use a simple passthrough vertex shader
    const char* vs_src = "#version 330 core\n"
        "layout(location=0) in vec3 pos;\n"
        "layout(location=1) in vec4 col;\n"
        "layout(location=2) in vec4 uv_in;\n"
        "layout(location=3) in vec2 rad_ang;\n"
        "out vec4 _vDiffuse;\n"
        "out vec4 _uv;\n"
        "out vec2 _rad_ang;\n"
        "void main() {\n"
        "  gl_Position = vec4(pos, 1.0);\n"
        "  _vDiffuse = col;\n"
        "  _uv = uv_in;\n"
        "  _rad_ang = rad_ang;\n"
        "}\n";

    GLuint vs = CompileShaderStage(vs_src, GL_VERTEX_SHADER);
    if (!vs) { glDeleteShader(fs); return 0; }

    GLuint program = LinkProgram(vs, fs);
    glDeleteShader(vs);
    glDeleteShader(fs);

    if (program)
        CacheUniforms(program, params);

    return program;
}

GLuint GLShaderManager::CompileVertexShader(const char* body,
                                           std::vector<ShaderParamInfo>& params)
{
    std::string vs_src = BuildVShaderSource(body);
    GLuint vs = CompileShaderStage(vs_src.c_str(), GL_VERTEX_SHADER);
    if (!vs) return 0;

    // For vertex shaders that don't have a fragment shader pair,
    // we still need one for GL. Use a minimal FS.
    const char* fs_src = "#version 330 core\n"
        "out vec4 fragColor;\n"
        "void main() { fragColor = vec4(1.0); }\n";

    GLuint fs = CompileShaderStage(fs_src, GL_FRAGMENT_SHADER);
    if (!fs) { glDeleteShader(vs); return 0; }

    GLuint program = LinkProgram(vs, fs);
    glDeleteShader(vs);
    glDeleteShader(fs);

    if (program)
        CacheUniforms(program, params);

    return program;
}

void GLShaderManager::CacheUniforms(GLuint program, std::vector<ShaderParamInfo>& params)
{
    params.clear();

    GLint count = 0;
    glGetProgramiv(program, GL_ACTIVE_UNIFORMS, &count);

    for (GLint i = 0; i < count; i++)
    {
        char name[256];
        GLsizei name_len = 0;
        GLint size = 0;
        GLenum type = GL_NONE;
        glGetActiveUniform(program, (GLuint)i, sizeof(name), &name_len, &size, &type, name);
        name[name_len] = '\0';

        // Skip gl_ built-ins
        if (name[0] == 'g' && name[1] == 'l' && name[2] == '_')
            continue;

        ShaderParamInfo info;
        info.name = name;
        info.location = glGetUniformLocation(program, name);
        info.sampler_unit = -1;
        info.texcode = -1;

        // Detect sampler uniforms for texture binding
        if (type == GL_SAMPLER_2D || type == GL_SAMPLER_3D)
        {
            // We'll assign texture units dynamically; for now just note it
            info.sampler_unit = -1; // assigned at bind time
        }

        params.push_back(info);
    }
}

GLint GLShaderManager::GetUniformLocation(GLuint program, const char* name)
{
    return glGetUniformLocation(program, name);
}

void GLShaderManager::SetVec4(GLint loc, const glm::vec4& v)
{
    if (loc >= 0) glUniform4fv(loc, 1, glm::value_ptr(v));
}

void GLShaderManager::SetVec4(GLint loc, float x, float y, float z, float w)
{
    if (loc >= 0) glUniform4f(loc, x, y, z, w);
}

void GLShaderManager::SetFloat(GLint loc, float v)
{
    if (loc >= 0) glUniform1f(loc, v);
}

void GLShaderManager::SetMatrix4x3(GLint loc, const glm::mat4x3& m)
{
    if (loc >= 0) glUniformMatrix4x3fv(loc, 1, GL_FALSE, glm::value_ptr(m));
}
