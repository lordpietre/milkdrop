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
    const char* GetPresetName() const { return m_preset_name.c_str(); }

private:
    bool ParseMilkFile(const char* filename);

    // Variable storage
    std::map<std::string, double> m_vars;
    std::vector<te_variable> m_te_vars;

    // Compiled per-frame expressions
    struct PerFrameExpr {
        std::string var_name;    // LHS variable name
        te_expr* expr;           // RHS compiled expression
    };
    std::vector<PerFrameExpr> m_per_frame_exprs;

    // Init expressions (one-shot)
    struct InitExpr {
        std::string var_name;
        te_expr* expr;
    };
    std::vector<InitExpr> m_init_exprs;

    // Per-frame init code string
    std::string m_per_frame_init;

    // Preset scalar values (stored for default values)
    std::map<std::string, std::string> m_preset_values;

    // Raw per-frame equation strings (before compilation)
    std::vector<std::string> m_per_frame_eqns_raw;

    // Per-vertex expressions (not yet compiled)
    std::string m_per_pixel_eqns;
    std::string m_warp_shader_text;
    std::string m_comp_shader_text;

    std::string m_preset_name;
    int m_preset_version;

    bool m_initialized;

    // Build te_variable array from current m_vars
    void RebuildVarBindings();
};

#endif
