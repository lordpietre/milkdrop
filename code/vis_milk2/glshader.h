#ifndef __GL_SHADER_H__
#define __GL_SHADER_H__ 1

#include <GL/glew.h>
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <map>

// Replicates the D3DX constant table query for OpenGL
struct ShaderParamInfo
{
    GLint location;
    std::string name;
    int sampler_unit;      // -1 if not a sampler
    int texcode;           // TEX_VS, TEX_BLUR1, TEX_DISK, etc.
    std::string tex_filename;
};

class GLShaderManager
{
public:
    GLShaderManager();
    ~GLShaderManager();

    // Load shader source from file
    std::string LoadFile(const char* path);

    // Load the default shader templates (from resources)
    bool LoadTemplates(const char* data_path);

    // Compile a MilkDrop pixel shader:
    //   include_text + defines + body_text -> GLSL
    //   Returns program id (0 on failure)
    GLuint CompilePixelShader(const char* body, const char* defines,
                              std::vector<ShaderParamInfo>& params);

    GLuint CompileVertexShader(const char* body,
                               std::vector<ShaderParamInfo>& params);

    // Build the "full" shader source (include + defines + body)
    std::string BuildPShaderSource(const char* body, const char* defines);
    std::string BuildVShaderSource(const char* body);

    // Cache uniform locations by scanning program
    void CacheUniforms(GLuint program, std::vector<ShaderParamInfo>& params);
    GLint GetUniformLocation(GLuint program, const char* name);

    // Set MilkDrop uniforms by name/location
    void SetVec4(GLint loc, const glm::vec4& v);
    void SetVec4(GLint loc, float x, float y, float z, float w);
    void SetFloat(GLint loc, float v);
    void SetMatrix4x3(GLint loc, const glm::mat4x3& m);

    // Template shader sources loaded from disk
    std::string m_include_text;
    std::string m_warp_vs_text;
    std::string m_warp_ps_default;
    std::string m_comp_vs_text;
    std::string m_comp_ps_default;
    std::string m_blur_vs_text;
    std::string m_blur1_ps_text;
    std::string m_blur2_ps_text;

    GLuint CompileShaderStage(const char* src, GLenum type);
    GLuint LinkProgram(GLuint vs, GLuint fs);
};

#endif // __GL_SHADER_H__
