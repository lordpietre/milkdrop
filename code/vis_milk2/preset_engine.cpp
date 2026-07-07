#include "preset_engine.h"
#include "milkdrop_renderer.h"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cmath>

PresetEngine::PresetEngine()
    : m_preset_version(100)
    , m_initialized(false)
    , m_pp_rad(0.0), m_pp_ang(0.0), m_pp_px(0.0), m_pp_py(0.0)
{}

PresetEngine::~PresetEngine()
{
    for (auto& pe : m_per_frame_exprs)
        te_free(pe.expr);
    for (auto& ie : m_init_exprs)
        te_free(ie.expr);
    for (auto& pe : m_per_pixel_exprs)
        te_free(pe.expr);
}

static void trim(std::string& s)
{
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r'))
        s.pop_back();
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t'))
        s.erase(0, 1);
}

void PresetEngine::RebuildVarBindings()
{
    m_te_vars.clear();
    for (auto& kv : m_vars)
    {
        te_variable tv;
        tv.name = kv.first.c_str();
        tv.address = &kv.second;
        tv.type = TE_VARIABLE;
        tv.context = 0;
        m_te_vars.push_back(tv);
    }
    // Add per-vertex variables
    {
        te_variable tv;
        tv.name = "rad";
        tv.address = &m_pp_rad;
        tv.type = TE_VARIABLE;
        tv.context = 0;
        m_te_vars.push_back(tv);
    }
    {
        te_variable tv;
        tv.name = "ang";
        tv.address = &m_pp_ang;
        tv.type = TE_VARIABLE;
        tv.context = 0;
        m_te_vars.push_back(tv);
    }
    {
        te_variable tv;
        tv.name = "PER_PIXEL_POINT.x";
        tv.address = &m_pp_px;
        tv.type = TE_VARIABLE;
        tv.context = 0;
        m_te_vars.push_back(tv);
    }
    {
        te_variable tv;
        tv.name = "PER_PIXEL_POINT.y";
        tv.address = &m_pp_py;
        tv.type = TE_VARIABLE;
        tv.context = 0;
        m_te_vars.push_back(tv);
    }
}

bool PresetEngine::LoadPreset(const char* filename)
{
    for (auto& pe : m_per_frame_exprs)
        te_free(pe.expr);
    for (auto& ie : m_init_exprs)
        te_free(ie.expr);
    for (auto& pe : m_per_pixel_exprs)
        te_free(pe.expr);
    m_per_frame_exprs.clear();
    m_init_exprs.clear();
    m_per_pixel_exprs.clear();
    m_per_frame_eqns_raw.clear();
    m_vars.clear();
    m_te_vars.clear();
    m_preset_values.clear();
    m_per_pixel_eqns.clear();
    m_per_frame_init.clear();
    m_warp_shader_text.clear();
    m_comp_shader_text.clear();
    m_initialized = false;

    m_preset_name = filename;
    size_t slash = m_preset_name.find_last_of("/\\");
    if (slash != std::string::npos)
        m_preset_name = m_preset_name.substr(slash + 1);
    size_t dot = m_preset_name.rfind('.');
    if (dot != std::string::npos)
        m_preset_name = m_preset_name.substr(0, dot);

    if (!ParseMilkFile(filename))
        return false;

    // Initialize all known variables with defaults
    m_vars["time"] = 0.0;
    m_vars["fps"] = 60.0;
    m_vars["frame"] = 0.0;
    m_vars["bass"] = 0.0;
    m_vars["mid"] = 0.0;
    m_vars["treb"] = 0.0;
    m_vars["bass_att"] = 0.0;
    m_vars["mid_att"] = 0.0;
    m_vars["treb_att"] = 0.0;
    m_vars["progress"] = 0.0;
    m_vars["zoom"] = 1.0;
    m_vars["zoomexp"] = 1.0;
    m_vars["rot"] = 0.0;
    m_vars["warp"] = 0.0;
    m_vars["cx"] = 0.5;
    m_vars["cy"] = 0.5;
    m_vars["dx"] = 0.0;
    m_vars["dy"] = 0.0;
    m_vars["sx"] = 1.0;
    m_vars["sy"] = 1.0;
    m_vars["decay"] = 0.98;
    m_vars["wave_a"] = 0.0;
    m_vars["wave_r"] = 1.0;
    m_vars["wave_g"] = 0.0;
    m_vars["wave_b"] = 0.0;
    m_vars["wave_x"] = 0.0;
    m_vars["wave_y"] = 0.0;
    m_vars["wave_mystery"] = 0.0;
    m_vars["wave_mode"] = 0.0;
    m_vars["ob_size"] = 0.0;
    m_vars["ob_r"] = 0.0;
    m_vars["ob_g"] = 0.0;
    m_vars["ob_b"] = 0.0;
    m_vars["ob_a"] = 0.0;
    m_vars["ib_size"] = 0.0;
    m_vars["ib_r"] = 0.0;
    m_vars["ib_g"] = 0.0;
    m_vars["ib_b"] = 0.0;
    m_vars["ib_a"] = 0.0;
    m_vars["monitor"] = 0.0;
    m_vars["shader"] = 0.0;
    m_vars["mv_x"] = 0.0;
    m_vars["mv_y"] = 0.0;
    m_vars["mv_dx"] = 0.0;
    m_vars["mv_dy"] = 0.0;
    m_vars["mv_l"] = 0.0;
    m_vars["mv_r"] = 0.0;
    m_vars["mv_g"] = 0.0;
    m_vars["mv_b"] = 0.0;
    m_vars["mv_a"] = 0.0;
    m_vars["fRating"] = 0.0;
    m_vars["fGammaAdj"] = 1.0;
    m_vars["fDecay"] = 0.98;
    m_vars["fVideoEchoZoom"] = 1.0;
    m_vars["fVideoEchoAlpha"] = 0.0;
    m_vars["fWarpAnimSpeed"] = 1.0;
    m_vars["fWarpScale"] = 1.0;
    m_vars["fZoomExponent"] = 1.0;
    m_vars["fShader"] = 0.0;
    for (int i = 1; i <= 8; i++) {
        char buf[8];
        snprintf(buf, sizeof(buf), "q%d", i);
        m_vars[buf] = 0.0;
    }

    // Set scalar values from .milk file
    for (auto& kv : m_preset_values)
    {
        auto it = m_vars.find(kv.first);
        if (it != m_vars.end())
            it->second = atof(kv.second.c_str());
    }

    RebuildVarBindings();

    // Compile per-frame init expressions (one-shot)
    if (!m_per_frame_init.empty())
    {
        std::istringstream stream(m_per_frame_init);
        std::string line;
        while (std::getline(stream, line))
        {
            trim(line);
            while (!line.empty() && line.back() == ';')
                line.pop_back();
            trim(line);
            if (line.empty())
                continue;

            size_t eq = line.find('=');
            if (eq == std::string::npos)
                continue;

            std::string var_name = line.substr(0, eq);
            std::string expr_str = line.substr(eq + 1);
            trim(var_name);

            if (m_vars.find(var_name) == m_vars.end())
            {
                m_vars[var_name] = 0.0;
                RebuildVarBindings();
            }

            int error = 0;
            te_expr* expr = te_compile(expr_str.c_str(), m_te_vars.data(), (int)m_te_vars.size(), &error);
            if (!expr)
            {
                fprintf(stderr, "init expr error for '%s': '%s'\n",
                        var_name.c_str(), expr_str.c_str());
                continue;
            }
            InitExpr ie;
            ie.var_name = var_name;
            ie.expr = expr;
            m_init_exprs.push_back(ie);
        }
    }

    // Compile per-frame expressions
    for (const auto& eq_str : m_per_frame_eqns_raw)
    {
        std::string s = eq_str;
        trim(s);
        while (!s.empty() && s.back() == ';')
            s.pop_back();
        trim(s);
        if (s.empty())
            continue;

        size_t eq = s.find('=');
        if (eq == std::string::npos)
            continue;

        std::string var_name = s.substr(0, eq);
        std::string expr_str = s.substr(eq + 1);
        trim(var_name);

        if (m_vars.find(var_name) == m_vars.end())
        {
            m_vars[var_name] = 0.0;
            RebuildVarBindings();
        }

        int error = 0;
        te_expr* expr = te_compile(expr_str.c_str(), m_te_vars.data(), (int)m_te_vars.size(), &error);
        if (!expr)
        {
            fprintf(stderr, "frame expr error for '%s': '%s'\n",
                    var_name.c_str(), expr_str.c_str());
            continue;
        }
        PerFrameExpr pfe;
        pfe.var_name = var_name;
        pfe.expr = expr;
        m_per_frame_exprs.push_back(pfe);
    }

    // Compile per-pixel expressions
    if (!m_per_pixel_eqns.empty())
    {
        std::istringstream stream(m_per_pixel_eqns);
        std::string line;
        while (std::getline(stream, line))
        {
            trim(line);
            while (!line.empty() && line.back() == ';')
                line.pop_back();
            trim(line);
            if (line.empty())
                continue;

            size_t eq = line.find('=');
            if (eq == std::string::npos)
                continue;

            std::string var_name = line.substr(0, eq);
            std::string expr_str = line.substr(eq + 1);
            trim(var_name);

            if (m_vars.find(var_name) == m_vars.end())
            {
                m_vars[var_name] = 0.0;
                RebuildVarBindings();
            }

            int error = 0;
            te_expr* expr = te_compile(expr_str.c_str(), m_te_vars.data(), (int)m_te_vars.size(), &error);
            if (!expr)
            {
                fprintf(stderr, "per_pixel expr error for '%s': '%s'\n",
                        var_name.c_str(), expr_str.c_str());
                continue;
            }
            PerPixelExpr ppe;
            ppe.var_name = var_name;
            ppe.expr = expr;
            m_per_pixel_exprs.push_back(ppe);
        }
    }

    m_initialized = true;
    return true;
}

void PresetEngine::ApplyShaderOverrides(MilkdropRenderer* renderer)
{
    if (!m_comp_shader_text.empty())
        renderer->CompileCompShader(m_comp_shader_text.c_str());
    if (!m_warp_shader_text.empty())
        renderer->CompileWarpShader(m_warp_shader_text.c_str());
}

bool PresetEngine::ParseMilkFile(const char* filename)
{
    std::ifstream file(filename);
    if (!file.is_open())
    {
        fprintf(stderr, "PresetEngine: cannot open '%s'\n", filename);
        return false;
    }

    std::string line;
    std::string current_section;

    while (std::getline(file, line))
    {
        trim(line);
        if (line.empty())
            continue;

        if (line.size() >= 2 && line[0] == '[' && line.back() == ']')
        {
            current_section = line.substr(1, line.size() - 2);
            continue;
        }

        if (line.size() >= 2 && line[0] == '/' && line[1] == '/')
            continue;

        size_t eq_pos = line.find('=');
        if (eq_pos == std::string::npos)
            continue;

        std::string key = line.substr(0, eq_pos);
        std::string value = line.substr(eq_pos + 1);
        trim(key);
        trim(value);

        if (key.empty())
            continue;

        if (key.find("per_frame_init_") == 0)
        {
            if (!m_per_frame_init.empty())
                m_per_frame_init += '\n';
            m_per_frame_init += value;
        }
        else if (key.find("per_frame_") == 0)
        {
            m_per_frame_eqns_raw.push_back(value);
        }
        else if (key.find("per_pixel_") == 0)
        {
            if (!m_per_pixel_eqns.empty())
                m_per_pixel_eqns += '\n';
            m_per_pixel_eqns += value;
        }
        else if (key.find("warp_") == 0 && key != "warp")
        {
            if (value[0] == '`') value = value.substr(1);
            if (!m_warp_shader_text.empty())
                m_warp_shader_text += '\n';
            m_warp_shader_text += value;
        }
        else if (key.find("comp_") == 0 && key.find("comp_version") == std::string::npos)
        {
            if (value[0] == '`') value = value.substr(1);
            if (!m_comp_shader_text.empty())
                m_comp_shader_text += '\n';
            m_comp_shader_text += value;
        }
        else
        {
            m_preset_values[key] = value;
        }
    }

    file.close();
    return true;
}

void PresetEngine::EvaluateFrame(float time, float fps,
                                  float bass, float mid, float treb,
                                  float bass_att, float mid_att, float treb_att,
                                  int frame,
                                  MilkdropRenderer* renderer)
{
    if (!m_initialized)
        return;

    // Set read-only built-in variables
    m_vars["time"] = time;
    m_vars["fps"] = fps;
    m_vars["frame"] = (double)frame;
    m_vars["bass"] = bass;
    m_vars["mid"] = mid;
    m_vars["treb"] = treb;
    m_vars["bass_att"] = bass_att;
    m_vars["mid_att"] = mid_att;
    m_vars["treb_att"] = treb_att;

    // Execute init expressions (one-shot, then cleared)
    for (auto& ie : m_init_exprs)
    {
        double val = te_eval(ie.expr);
        m_vars[ie.var_name] = val;
    }
    m_init_exprs.clear();

    // Evaluate per-frame expressions in order
    for (auto& pe : m_per_frame_exprs)
    {
        double val = te_eval(pe.expr);
        m_vars[pe.var_name] = val;
    }

    // Copy relevant variables to renderer
    auto g = [&](const char* name, double def) -> float {
        auto it = m_vars.find(name);
        return (it != m_vars.end()) ? (float)it->second : (float)def;
    };

    float decay = g("fDecay", 0.98f);
    if (decay < 0.0f) decay = 0.0f;
    if (decay > 1.0f) decay = 1.0f;
    renderer->m_decay = 1.0f - decay;

    renderer->m_zoom  = g("zoom", 1.0f);
    renderer->m_rot   = g("rot", 0.0f);
    renderer->m_warp  = g("warp", 0.0f);
    renderer->m_cx    = g("cx", 0.5f);
    renderer->m_cy    = g("cy", 0.5f);
    renderer->m_dx    = g("dx", 0.0f);
    renderer->m_dy    = g("dy", 0.0f);
    renderer->m_sx    = g("sx", 1.0f);
    renderer->m_sy    = g("sy", 1.0f);
}

void PresetEngine::EvaluatePerPixel(int count,
                                     const float* rad, const float* ang,
                                     const float* px, const float* py,
                                     float* out_zoom, float* out_rot,
                                     float* out_warp,
                                     float* out_cx, float* out_cy,
                                     float* out_dx, float* out_dy,
                                     float* out_sx, float* out_sy)
{
    if (!m_initialized || m_per_pixel_exprs.empty())
        return;

    for (int i = 0; i < count; i++)
    {
        // Set per-vertex variables
        m_pp_rad = rad[i];
        m_pp_ang = ang[i];
        m_pp_px = px[i];
        m_pp_py = py[i];

        // Evaluate all per_pixel expressions for this vertex
        // Each expression assigns to a variable; accumulate into output arrays
        for (auto& pe : m_per_pixel_exprs)
        {
            double val = te_eval(pe.expr);

            if (pe.var_name == "zoom" && out_zoom)
                out_zoom[i] = (float)val;
            else if (pe.var_name == "rot" && out_rot)
                out_rot[i] = (float)val;
            else if (pe.var_name == "warp" && out_warp)
                out_warp[i] = (float)val;
            else if (pe.var_name == "cx" && out_cx)
                out_cx[i] = (float)val;
            else if (pe.var_name == "cy" && out_cy)
                out_cy[i] = (float)val;
            else if (pe.var_name == "dx" && out_dx)
                out_dx[i] = (float)val;
            else if (pe.var_name == "dy" && out_dy)
                out_dy[i] = (float)val;
            else if (pe.var_name == "sx" && out_sx)
                out_sx[i] = (float)val;
            else if (pe.var_name == "sy" && out_sy)
                out_sy[i] = (float)val;
            // Set the global variable too
            auto vit = m_vars.find(pe.var_name);
            if (vit != m_vars.end())
                vit->second = val;
        }
    }
}
