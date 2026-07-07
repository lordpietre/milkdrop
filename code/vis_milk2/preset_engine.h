#ifndef __PRESET_ENGINE_H__
#define __PRESET_ENGINE_H__ 1

#include <string>
#include <vector>
#include <map>
#include "tinyexpr.h"

class MilkdropRenderer;

class PresetEngine
{
public:
    PresetEngine();
    ~PresetEngine();

    bool LoadPreset(const char* filename);
    void ApplyShaderOverrides(class MilkdropRenderer* renderer);
    void EvaluateFrame(float time, float fps,
                       float bass, float mid, float treb,
                       float bass_att, float mid_att, float treb_att,
                       int frame,
                       MilkdropRenderer* renderer);

    // Evaluate per-pixel equations for a range of vertices
    // rad, ang, px, py are per-vertex arrays of length count
    // out_zoom, etc. are output arrays (same length), may be nullptr
    void EvaluatePerPixel(int count,
                          const float* rad, const float* ang,
                          const float* px, const float* py,
                          float* out_zoom, float* out_rot,
                          float* out_warp,
                          float* out_cx, float* out_cy,
                          float* out_dx, float* out_dy,
                          float* out_sx, float* out_sy);

    const char* GetPresetName() const { return m_preset_name.c_str(); }

private:
    bool ParseMilkFile(const char* filename);

    // Variable storage
    std::map<std::string, double> m_vars;

    // TinyExpr variable bindings (rebuilt when m_vars changes)
    std::vector<te_variable> m_te_vars;

    // Per-vertex variable storage (shared by all per_pixel expressions)
    double m_pp_rad;
    double m_pp_ang;
    double m_pp_px;
    double m_pp_py;

    // Compiled per-frame expressions
    struct PerFrameExpr {
        std::string var_name;
        te_expr* expr;
    };
    std::vector<PerFrameExpr> m_per_frame_exprs;

    // Init expressions (one-shot on first frame)
    struct InitExpr {
        std::string var_name;
        te_expr* expr;
    };
    std::vector<InitExpr> m_init_exprs;

    // Per-pixel compiled expressions
    struct PerPixelExpr {
        std::string var_name;  // which variable to modify
        te_expr* expr;         // compiled expression
    };
    std::vector<PerPixelExpr> m_per_pixel_exprs;

    // Raw per-frame equation strings (before compilation)
    std::vector<std::string> m_per_frame_eqns_raw;

    // Per-vertex shader code (for warp/comp overrides)
    std::string m_per_pixel_eqns;
    std::string m_warp_shader_text;
    std::string m_comp_shader_text;

    // Preset scalar values (stored for default values)
    std::map<std::string, std::string> m_preset_values;

    // Per-frame init code string
    std::string m_per_frame_init;

    std::string m_preset_name;
    int m_preset_version;

    bool m_initialized;

    // Build te_variable array from current m_vars
    void RebuildVarBindings();
};

#endif
