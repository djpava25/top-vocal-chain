#pragma once
#include <vector>
#include <utility>

namespace tvc
{
// Presets de fabrica. Valores en unidades reales (dB, Hz, ms, %).
// Los parametros que no aparecen vuelven a su valor por defecto.
struct FactoryPreset
{
    const char* name;
    std::vector<std::pair<const char*, float>> values;
};

inline const std::vector<FactoryPreset>& getFactoryPresets()
{
    static const std::vector<FactoryPreset> presets {
        { "Default", {} },

        { "Dominican Dembow", { { "pre_type", 6.0f }, { "pre_drive", 10.0f }, { "pre_warmth", 55.0f }, { "tune_mode", 1.0f }, { "tune_scale", 2.0f }, { "eq_type", 2.0f }, { "eq_lowcut", 130.0f }, { "eq_hp_slope", 3.0f }, { "eq_low_gain", -2.0f }, { "eq_mid_freq", 2200.0f }, { "eq_mid_gain", 3.0f }, { "eq_pres_gain", 3.5f }, { "eq_air_gain", 3.0f }, { "eq_hicut", 18000.0f }, { "comp_type", 2.0f }, { "comp_thr", -26.0f }, { "comp_ratio", 8.0f }, { "comp_att", 1.5f }, { "comp_rel", 50.0f }, { "comp_makeup", 7.0f }, { "comp_knee", 3.0f }, { "wide_mode", 3.0f }, { "wide_amount", 30.0f }, { "wide_width", 110.0f }, { "dly_div", 6.0f }, { "dly_fb", 20.0f }, { "dly_mix", 10.0f }, { "dly_locut", 300.0f }, { "dly_sat", 20.0f }, { "dly_duck", 50.0f }, { "rev_type", 0.0f }, { "rev_decay", 0.8f }, { "rev_mix", 8.0f }, { "rev_predelay", 10.0f }, { "ai_amount", 55.0f } } },

        { "Trap Aggressive", { { "pre_type", 7.0f }, { "pre_drive", 8.0f }, { "tune_mode", 1.0f }, { "tune_scale", 2.0f }, { "eq_type", 2.0f }, { "eq_lowcut", 110.0f }, { "eq_mid_freq", 2500.0f }, { "eq_mid_gain", 2.0f }, { "eq_pres_gain", 3.0f }, { "eq_air_gain", 4.5f }, { "comp_type", 5.0f }, { "comp_thr", -24.0f }, { "comp_ratio", 8.0f }, { "comp_makeup", 6.0f }, { "comp_mix", 85.0f }, { "wide_mode", 3.0f }, { "wide_amount", 45.0f }, { "wide_width", 120.0f }, { "dly_div", 4.0f }, { "dly_fb", 30.0f }, { "dly_mix", 14.0f }, { "dly_pingpong", 1.0f }, { "dly_duck", 40.0f }, { "dly_sat", 15.0f }, { "rev_type", 1.0f }, { "rev_decay", 1.4f }, { "rev_mix", 10.0f }, { "rev_predelay", 25.0f }, { "ai_amount", 50.0f } } },

        { "Modern Reggaeton", { { "pre_type", 5.0f }, { "pre_drive", 6.0f }, { "tune_speed", 15.0f }, { "tune_amount", 95.0f }, { "tune_scale", 2.0f }, { "tune_humanize", 20.0f }, { "eq_type", 2.0f }, { "eq_lowcut", 120.0f }, { "eq_mid_freq", 3000.0f }, { "eq_mid_gain", 1.5f }, { "eq_pres_gain", 2.5f }, { "eq_air_gain", 4.5f }, { "comp_type", 4.0f }, { "comp_thr", -22.0f }, { "comp_ratio", 4.0f }, { "comp_makeup", 4.0f }, { "wide_mode", 2.0f }, { "wide_amount", 30.0f }, { "dly_div", 4.0f }, { "dly_fb", 28.0f }, { "dly_mix", 15.0f }, { "dly_pingpong", 1.0f }, { "dly_duck", 45.0f }, { "dly_width", 140.0f }, { "rev_type", 4.0f }, { "rev_decay", 1.6f }, { "rev_mix", 15.0f }, { "rev_predelay", 30.0f }, { "ai_amount", 50.0f } } },

        { "Bad Bunny Style", { { "pre_type", 1.0f }, { "pre_drive", 6.0f }, { "pre_warmth", 60.0f }, { "tune_speed", 8.0f }, { "tune_amount", 100.0f }, { "tune_flex", 20.0f }, { "tune_scale", 2.0f }, { "eq_type", 3.0f }, { "eq_lowcut", 100.0f }, { "eq_low_gain", 2.0f }, { "eq_pres_gain", 2.5f }, { "eq_air_gain", 5.0f }, { "comp_type", 1.0f }, { "comp_thr", -22.0f }, { "comp_ratio", 4.0f }, { "comp_makeup", 5.0f }, { "wide_mode", 4.0f }, { "wide_amount", 40.0f }, { "wide_width", 125.0f }, { "dly_div", 2.0f }, { "dly_fb", 32.0f }, { "dly_mix", 16.0f }, { "dly_duck", 55.0f }, { "dly_mod", 25.0f }, { "dly_locut", 250.0f }, { "rev_type", 2.0f }, { "rev_decay", 2.4f }, { "rev_mix", 14.0f }, { "rev_predelay", 35.0f }, { "rev_duck", 40.0f }, { "ai_amount", 45.0f } } },

        { "El Alfa Style", { { "pre_type", 6.0f }, { "pre_drive", 14.0f }, { "tune_mode", 1.0f }, { "tune_scale", 2.0f }, { "eq_type", 2.0f }, { "eq_lowcut", 140.0f }, { "eq_hp_slope", 3.0f }, { "eq_low_gain", -3.0f }, { "eq_mid_freq", 2000.0f }, { "eq_mid_gain", 3.5f }, { "eq_pres_gain", 4.0f }, { "eq_air_gain", 3.0f }, { "comp_type", 2.0f }, { "comp_thr", -28.0f }, { "comp_ratio", 12.0f }, { "comp_att", 1.0f }, { "comp_rel", 40.0f }, { "comp_makeup", 9.0f }, { "comp_knee", 2.0f }, { "wide_mode", 0.0f }, { "wide_amount", 25.0f }, { "dly_div", 6.0f }, { "dly_fb", 18.0f }, { "dly_mix", 8.0f }, { "dly_sat", 30.0f }, { "dly_duck", 60.0f }, { "rev_type", 0.0f }, { "rev_decay", 0.6f }, { "rev_mix", 6.0f }, { "limit_on", 1.0f }, { "ai_amount", 55.0f } } },

        { "Melodic Trap", { { "pre_type", 1.0f }, { "pre_drive", 5.0f }, { "tune_speed", 12.0f }, { "tune_amount", 100.0f }, { "tune_humanize", 35.0f }, { "tune_scale", 2.0f }, { "eq_type", 1.0f }, { "eq_lowcut", 90.0f }, { "eq_mid_freq", 360.0f }, { "eq_mid_gain", -2.0f }, { "eq_pres_gain", 2.0f }, { "eq_air_gain", 4.0f }, { "comp_type", 4.0f }, { "comp_thr", -20.0f }, { "comp_ratio", 4.0f }, { "comp_makeup", 4.0f }, { "wide_mode", 4.0f }, { "wide_amount", 45.0f }, { "dly_div", 1.0f }, { "dly_fb", 38.0f }, { "dly_mix", 18.0f }, { "dly_pingpong", 1.0f }, { "dly_mod", 30.0f }, { "dly_duck", 50.0f }, { "rev_type", 2.0f }, { "rev_decay", 3.0f }, { "rev_mix", 18.0f }, { "rev_predelay", 40.0f }, { "rev_duck", 35.0f }, { "ai_amount", 45.0f } } },

        { "Clean Pop Vocal", { { "pre_type", 3.0f }, { "pre_drive", 3.0f }, { "tune_speed", 40.0f }, { "tune_amount", 80.0f }, { "tune_humanize", 50.0f }, { "tune_flex", 40.0f }, { "tune_scale", 1.0f }, { "eq_type", 0.0f }, { "eq_lowcut", 90.0f }, { "eq_mid_freq", 400.0f }, { "eq_mid_gain", -2.0f }, { "eq_mid_q", 1.2f }, { "eq_pres_gain", 2.0f }, { "eq_air_gain", 5.0f }, { "comp_type", 1.0f }, { "comp_thr", -20.0f }, { "comp_ratio", 3.0f }, { "comp_makeup", 4.0f }, { "wide_mode", 2.0f }, { "wide_amount", 30.0f }, { "dly_div", 1.0f }, { "dly_fb", 25.0f }, { "dly_mix", 10.0f }, { "dly_duck", 60.0f }, { "rev_type", 4.0f }, { "rev_decay", 1.8f }, { "rev_mix", 16.0f }, { "rev_predelay", 30.0f }, { "ai_amount", 55.0f } } },

        { "R&B Smooth", { { "pre_type", 2.0f }, { "pre_drive", 4.0f }, { "pre_warmth", 70.0f }, { "tune_speed", 90.0f }, { "tune_amount", 60.0f }, { "tune_humanize", 60.0f }, { "tune_flex", 50.0f }, { "eq_type", 3.0f }, { "eq_low_gain", 2.0f }, { "eq_mid_freq", 350.0f }, { "eq_mid_gain", -1.5f }, { "eq_pres_gain", 1.0f }, { "eq_air_gain", 3.5f }, { "comp_type", 6.0f }, { "comp_thr", -20.0f }, { "comp_ratio", 3.0f }, { "comp_makeup", 3.0f }, { "wide_mode", 4.0f }, { "wide_amount", 45.0f }, { "dly_div", 2.0f }, { "dly_fb", 35.0f }, { "dly_mix", 15.0f }, { "dly_hicut", 4000.0f }, { "dly_mod", 35.0f }, { "dly_duck", 40.0f }, { "rev_type", 1.0f }, { "rev_decay", 2.6f }, { "rev_damp", 60.0f }, { "rev_mix", 22.0f }, { "rev_predelay", 25.0f }, { "ai_amount", 40.0f } } },

        { "Rap Punch", { { "pre_type", 5.0f }, { "pre_drive", 9.0f }, { "pre_warmth", 60.0f }, { "tune_speed", 35.0f }, { "tune_amount", 60.0f }, { "tune_flex", 60.0f }, { "eq_type", 1.0f }, { "eq_lowcut", 100.0f }, { "eq_low_gain", 1.0f }, { "eq_mid_freq", 1600.0f }, { "eq_mid_gain", 2.0f }, { "eq_pres_gain", 3.0f }, { "eq_air_gain", 2.0f }, { "comp_type", 2.0f }, { "comp_thr", -24.0f }, { "comp_ratio", 8.0f }, { "comp_att", 2.0f }, { "comp_rel", 60.0f }, { "comp_makeup", 6.0f }, { "comp_mix", 80.0f }, { "wide_mode", 0.0f }, { "wide_amount", 15.0f }, { "dly_div", 6.0f }, { "dly_fb", 15.0f }, { "dly_mix", 7.0f }, { "dly_duck", 70.0f }, { "rev_type", 0.0f }, { "rev_decay", 0.7f }, { "rev_mix", 6.0f }, { "ai_amount", 60.0f } } },

        { "Drill Vocal", { { "pre_type", 7.0f }, { "pre_drive", 9.0f }, { "tune_speed", 20.0f }, { "tune_amount", 80.0f }, { "eq_type", 2.0f }, { "eq_lowcut", 130.0f }, { "eq_low_gain", -2.0f }, { "eq_mid_freq", 2500.0f }, { "eq_mid_gain", 2.5f }, { "eq_pres_gain", 3.5f }, { "eq_air_gain", 2.5f }, { "eq_hicut", 16000.0f }, { "comp_type", 5.0f }, { "comp_thr", -26.0f }, { "comp_ratio", 10.0f }, { "comp_makeup", 7.0f }, { "wide_mode", 0.0f }, { "wide_amount", 20.0f }, { "dly_div", 5.0f }, { "dly_fb", 22.0f }, { "dly_mix", 12.0f }, { "dly_sat", 25.0f }, { "dly_locut", 400.0f }, { "dly_duck", 55.0f }, { "rev_type", 3.0f }, { "rev_decay", 1.0f }, { "rev_mix", 9.0f }, { "ai_amount", 50.0f } } },

        { "Bachata Modern", { { "pre_type", 1.0f }, { "pre_drive", 4.0f }, { "pre_warmth", 50.0f }, { "tune_speed", 50.0f }, { "tune_amount", 75.0f }, { "tune_humanize", 50.0f }, { "tune_scale", 1.0f }, { "eq_type", 1.0f }, { "eq_lowcut", 85.0f }, { "eq_low_gain", 1.5f }, { "eq_mid_freq", 700.0f }, { "eq_mid_gain", -1.5f }, { "eq_pres_gain", 2.0f }, { "eq_air_gain", 3.5f }, { "comp_type", 1.0f }, { "comp_thr", -18.0f }, { "comp_ratio", 3.0f }, { "comp_makeup", 3.0f }, { "wide_mode", 2.0f }, { "wide_amount", 25.0f }, { "dly_div", 1.0f }, { "dly_fb", 28.0f }, { "dly_mix", 12.0f }, { "dly_duck", 50.0f }, { "rev_type", 2.0f }, { "rev_decay", 2.2f }, { "rev_mix", 20.0f }, { "rev_predelay", 30.0f }, { "ai_amount", 45.0f } } },

        { "Studio Clean", { { "pre_type", 0.0f }, { "pre_drive", 0.0f }, { "tune_speed", 80.0f }, { "tune_amount", 40.0f }, { "tune_humanize", 60.0f }, { "tune_flex", 70.0f }, { "eq_type", 0.0f }, { "eq_lowcut", 80.0f }, { "eq_pres_gain", 1.0f }, { "eq_air_gain", 1.5f }, { "comp_type", 0.0f }, { "comp_thr", -18.0f }, { "comp_ratio", 2.5f }, { "comp_att", 10.0f }, { "comp_rel", 150.0f }, { "comp_makeup", 2.0f }, { "wide_on", 0.0f }, { "dly_on", 0.0f }, { "rev_type", 0.0f }, { "rev_decay", 0.6f }, { "rev_mix", 5.0f }, { "ai_amount", 40.0f } } },

        { "Clasico Neve + 1176", {
            { "pre_type", 5.0f }, { "pre_drive", 9.0f }, { "pre_warmth", 50.0f },
            { "eq_type", 1.0f }, { "eq_lowcut", 80.0f }, { "eq_mid_freq", 1600.0f }, { "eq_mid_gain", 2.0f }, { "eq_pres_gain", 2.0f }, { "eq_air_gain", 3.0f },
            { "comp_type", 2.0f }, { "comp_thr", -24.0f }, { "comp_ratio", 4.0f }, { "comp_att", 3.0f }, { "comp_rel", 80.0f }, { "comp_makeup", 6.0f },
            { "tune_speed", 30.0f }, { "tune_amount", 80.0f }, { "rev_mix", 12.0f }, { "dly_mix", 10.0f } } },

        { "Pultec + LA-2A (Suave)", {
            { "pre_type", 1.0f }, { "pre_drive", 5.0f }, { "pre_warmth", 60.0f },
            { "eq_type", 3.0f }, { "eq_lowcut", 60.0f }, { "eq_low_gain", 4.0f }, { "eq_pres_gain", 3.0f }, { "eq_air_gain", 4.0f },
            { "comp_type", 1.0f }, { "comp_thr", -22.0f }, { "comp_ratio", 3.0f }, { "comp_rel", 250.0f }, { "comp_makeup", 5.0f },
            { "tune_speed", 60.0f }, { "tune_amount", 70.0f }, { "rev_mix", 18.0f } } },

        { "API + Cinta (Punch)", {
            { "pre_type", 4.0f }, { "pre_drive", 10.0f }, { "pre_warmth", 35.0f },
            { "eq_type", 2.0f }, { "eq_lowcut", 100.0f }, { "eq_mid_freq", 3000.0f }, { "eq_mid_gain", 3.0f }, { "eq_pres_gain", 2.0f }, { "eq_air_gain", 2.0f },
            { "comp_type", 3.0f }, { "comp_thr", -20.0f }, { "comp_ratio", 4.0f }, { "comp_makeup", 4.0f },
            { "tune_speed", 25.0f }, { "dly_mix", 12.0f }, { "rev_mix", 10.0f } } },

        { "Lo-Fi", {
            { "wide_mode", 1.0f }, { "wide_amount", 40.0f }, { "pre_type", 4.0f }, { "comp_type", 2.0f }, { "eq_type", 1.0f }, { "tune_speed", 60.0f }, { "tune_amount", 60.0f },
            { "pre_drive", 16.0f }, { "pre_warmth", 85.0f }, { "pre_trim", -4.0f },
            { "eq_lowcut", 250.0f }, { "eq_low_gain", -3.0f }, { "eq_mid_freq", 1200.0f }, { "eq_mid_gain", 4.0f },
            { "eq_pres_gain", -2.0f }, { "eq_air_gain", -8.0f },
            { "comp_thr", -24.0f }, { "comp_ratio", 6.0f }, { "comp_makeup", 5.0f },
            { "dly_div", 5.0f }, { "dly_fb", 40.0f }, { "dly_mix", 18.0f }, { "dly_hicut", 2500.0f },
            { "rev_size", 35.0f }, { "rev_damp", 80.0f }, { "rev_mix", 14.0f }, { "ai_amount", 20.0f } } },

        { "Podcast / Voz Hablada", {
            { "wide_on", 0.0f }, { "pre_type", 5.0f }, { "comp_type", 1.0f }, { "eq_type", 1.0f }, { "tune_on", 0.0f },
            { "pre_drive", 3.0f }, { "pre_warmth", 40.0f },
            { "eq_lowcut", 90.0f }, { "eq_low_gain", 1.5f }, { "eq_mid_freq", 300.0f }, { "eq_mid_gain", -2.0f },
            { "eq_pres_gain", 2.5f }, { "eq_air_gain", 1.5f },
            { "comp_thr", -24.0f }, { "comp_ratio", 4.0f }, { "comp_att", 8.0f }, { "comp_rel", 180.0f }, { "comp_makeup", 6.0f },
            { "dly_on", 0.0f }, { "rev_on", 0.0f }, { "ai_amount", 80.0f } } },
    };
    return presets;
}
} // namespace tvc
