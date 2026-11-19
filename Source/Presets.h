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

        { "Trap Autotune Duro", {
            { "wide_mode", 3.0f }, { "wide_amount", 40.0f }, { "pre_type", 2.0f }, { "comp_type", 2.0f }, { "eq_type", 2.0f }, { "tune_speed", 0.0f }, { "tune_amount", 100.0f }, { "tune_scale", 2.0f },
            { "pre_drive", 6.0f }, { "pre_warmth", 50.0f },
            { "eq_lowcut", 110.0f }, { "eq_low_gain", -1.0f }, { "eq_mid_freq", 2500.0f }, { "eq_mid_gain", 1.5f },
            { "eq_pres_gain", 3.0f }, { "eq_air_gain", 4.0f },
            { "comp_thr", -22.0f }, { "comp_ratio", 6.0f }, { "comp_att", 3.0f }, { "comp_rel", 80.0f }, { "comp_makeup", 5.0f },
            { "dly_div", 4.0f }, { "dly_fb", 30.0f }, { "dly_mix", 14.0f }, { "dly_pingpong", 1.0f },
            { "rev_size", 40.0f }, { "rev_mix", 12.0f }, { "ai_amount", 50.0f } } },

        { "Rap Moderno", {
            { "wide_mode", 0.0f }, { "wide_amount", 20.0f }, { "pre_type", 0.0f }, { "comp_type", 2.0f }, { "eq_type", 1.0f }, { "tune_speed", 35.0f }, { "tune_amount", 70.0f },
            { "pre_drive", 8.0f }, { "pre_warmth", 60.0f },
            { "eq_lowcut", 100.0f }, { "eq_low_gain", 1.0f }, { "eq_mid_freq", 1800.0f }, { "eq_mid_gain", 2.0f },
            { "eq_pres_gain", 3.0f }, { "eq_air_gain", 2.5f },
            { "comp_thr", -24.0f }, { "comp_ratio", 8.0f }, { "comp_att", 2.0f }, { "comp_rel", 60.0f }, { "comp_makeup", 6.0f },
            { "dly_div", 7.0f }, { "dly_fb", 15.0f }, { "dly_mix", 8.0f },
            { "rev_size", 25.0f }, { "rev_mix", 7.0f }, { "ai_amount", 60.0f } } },

        { "Reggaeton", {
            { "wide_mode", 3.0f }, { "wide_amount", 35.0f }, { "pre_type", 3.0f }, { "comp_type", 2.0f }, { "eq_type", 2.0f }, { "tune_speed", 15.0f }, { "tune_amount", 90.0f }, { "tune_scale", 2.0f },
            { "pre_drive", 5.0f }, { "pre_warmth", 45.0f },
            { "eq_lowcut", 120.0f }, { "eq_mid_freq", 3000.0f }, { "eq_mid_gain", 1.5f },
            { "eq_pres_gain", 2.5f }, { "eq_air_gain", 4.5f },
            { "comp_thr", -20.0f }, { "comp_ratio", 5.0f }, { "comp_att", 4.0f }, { "comp_rel", 100.0f }, { "comp_makeup", 4.0f },
            { "dly_div", 4.0f }, { "dly_fb", 28.0f }, { "dly_mix", 16.0f }, { "dly_pingpong", 1.0f },
            { "rev_size", 55.0f }, { "rev_mix", 16.0f }, { "ai_amount", 50.0f } } },

        { "Dembow", {
            { "wide_mode", 0.0f }, { "wide_amount", 30.0f }, { "pre_type", 2.0f }, { "comp_type", 2.0f }, { "eq_type", 2.0f }, { "tune_speed", 5.0f }, { "tune_amount", 100.0f }, { "tune_scale", 2.0f },
            { "pre_drive", 10.0f }, { "pre_warmth", 55.0f },
            { "eq_lowcut", 130.0f }, { "eq_low_gain", -2.0f }, { "eq_mid_freq", 2200.0f }, { "eq_mid_gain", 3.0f },
            { "eq_pres_gain", 3.5f }, { "eq_air_gain", 3.0f },
            { "comp_thr", -26.0f }, { "comp_ratio", 8.0f }, { "comp_att", 1.5f }, { "comp_rel", 50.0f }, { "comp_makeup", 7.0f },
            { "dly_div", 7.0f }, { "dly_fb", 20.0f }, { "dly_mix", 10.0f },
            { "rev_size", 30.0f }, { "rev_mix", 8.0f }, { "ai_amount", 55.0f } } },

        { "Pop Brillante", {
            { "wide_mode", 2.0f }, { "wide_amount", 35.0f }, { "pre_type", 0.0f }, { "comp_type", 1.0f }, { "eq_type", 3.0f }, { "tune_speed", 50.0f }, { "tune_amount", 80.0f }, { "tune_scale", 1.0f },
            { "pre_drive", 3.0f }, { "pre_warmth", 30.0f },
            { "eq_lowcut", 90.0f }, { "eq_mid_freq", 400.0f }, { "eq_mid_gain", -2.0f }, { "eq_mid_q", 1.2f },
            { "eq_pres_gain", 2.0f }, { "eq_air_gain", 5.0f },
            { "comp_thr", -20.0f }, { "comp_ratio", 4.0f }, { "comp_att", 6.0f }, { "comp_rel", 150.0f }, { "comp_makeup", 4.0f },
            { "dly_div", 1.0f }, { "dly_fb", 25.0f }, { "dly_mix", 12.0f },
            { "rev_size", 60.0f }, { "rev_damp", 35.0f }, { "rev_mix", 20.0f }, { "ai_amount", 50.0f } } },

        { "R&B Suave", {
            { "wide_mode", 4.0f }, { "wide_amount", 45.0f }, { "pre_type", 1.0f }, { "comp_type", 1.0f }, { "eq_type", 3.0f }, { "tune_speed", 90.0f }, { "tune_amount", 60.0f }, { "tune_scale", 2.0f },
            { "pre_drive", 4.0f }, { "pre_warmth", 70.0f },
            { "eq_lowcut", 70.0f }, { "eq_low_gain", 1.5f }, { "eq_mid_freq", 350.0f }, { "eq_mid_gain", -1.5f },
            { "eq_pres_gain", 1.0f }, { "eq_air_gain", 3.5f },
            { "comp_thr", -18.0f }, { "comp_ratio", 3.0f }, { "comp_att", 10.0f }, { "comp_rel", 200.0f }, { "comp_makeup", 3.0f },
            { "dly_div", 2.0f }, { "dly_fb", 35.0f }, { "dly_mix", 15.0f }, { "dly_hicut", 4000.0f },
            { "rev_size", 75.0f }, { "rev_damp", 60.0f }, { "rev_mix", 24.0f }, { "ai_amount", 40.0f } } },

        { "Balada Intima", {
            { "wide_mode", 4.0f }, { "wide_amount", 25.0f }, { "pre_type", 0.0f }, { "comp_type", 3.0f }, { "eq_type", 1.0f }, { "tune_speed", 150.0f }, { "tune_amount", 50.0f }, { "tune_scale", 1.0f },
            { "pre_drive", 2.0f }, { "pre_warmth", 50.0f },
            { "eq_lowcut", 70.0f }, { "eq_low_gain", 2.0f }, { "eq_pres_gain", 1.0f }, { "eq_air_gain", 3.0f },
            { "comp_thr", -16.0f }, { "comp_ratio", 2.5f }, { "comp_att", 15.0f }, { "comp_rel", 250.0f }, { "comp_makeup", 2.0f },
            { "dly_div", 1.0f }, { "dly_fb", 20.0f }, { "dly_mix", 8.0f },
            { "rev_size", 85.0f }, { "rev_damp", 55.0f }, { "rev_mix", 28.0f }, { "ai_amount", 40.0f } } },

        { "Lo-Fi", {
            { "wide_mode", 1.0f }, { "wide_amount", 40.0f }, { "pre_type", 1.0f }, { "comp_type", 2.0f }, { "eq_type", 1.0f }, { "tune_speed", 60.0f }, { "tune_amount", 60.0f },
            { "pre_drive", 16.0f }, { "pre_warmth", 85.0f }, { "pre_trim", -4.0f },
            { "eq_lowcut", 250.0f }, { "eq_low_gain", -3.0f }, { "eq_mid_freq", 1200.0f }, { "eq_mid_gain", 4.0f },
            { "eq_pres_gain", -2.0f }, { "eq_air_gain", -8.0f },
            { "comp_thr", -24.0f }, { "comp_ratio", 6.0f }, { "comp_makeup", 5.0f },
            { "dly_div", 5.0f }, { "dly_fb", 40.0f }, { "dly_mix", 18.0f }, { "dly_hicut", 2500.0f },
            { "rev_size", 35.0f }, { "rev_damp", 80.0f }, { "rev_mix", 14.0f }, { "ai_amount", 20.0f } } },

        { "Clasico Neve + 1176", {
            { "pre_type", 3.0f }, { "pre_drive", 9.0f }, { "pre_warmth", 50.0f },
            { "eq_type", 1.0f }, { "eq_lowcut", 80.0f }, { "eq_mid_freq", 1600.0f }, { "eq_mid_gain", 2.0f }, { "eq_pres_gain", 2.0f }, { "eq_air_gain", 3.0f },
            { "comp_type", 2.0f }, { "comp_thr", -24.0f }, { "comp_ratio", 4.0f }, { "comp_att", 3.0f }, { "comp_rel", 80.0f }, { "comp_makeup", 6.0f },
            { "tune_speed", 30.0f }, { "tune_amount", 80.0f }, { "rev_mix", 12.0f }, { "dly_mix", 10.0f } } },

        { "Pultec + LA-2A (Suave)", {
            { "pre_type", 0.0f }, { "pre_drive", 5.0f }, { "pre_warmth", 60.0f },
            { "eq_type", 3.0f }, { "eq_lowcut", 60.0f }, { "eq_low_gain", 4.0f }, { "eq_pres_gain", 3.0f }, { "eq_air_gain", 4.0f },
            { "comp_type", 1.0f }, { "comp_thr", -22.0f }, { "comp_ratio", 3.0f }, { "comp_rel", 250.0f }, { "comp_makeup", 5.0f },
            { "tune_speed", 60.0f }, { "tune_amount", 70.0f }, { "rev_mix", 18.0f } } },

        { "API + Cinta (Punch)", {
            { "pre_type", 1.0f }, { "pre_drive", 10.0f }, { "pre_warmth", 35.0f },
            { "eq_type", 2.0f }, { "eq_lowcut", 100.0f }, { "eq_mid_freq", 3000.0f }, { "eq_mid_gain", 3.0f }, { "eq_pres_gain", 2.0f }, { "eq_air_gain", 2.0f },
            { "comp_type", 3.0f }, { "comp_thr", -20.0f }, { "comp_ratio", 4.0f }, { "comp_makeup", 4.0f },
            { "tune_speed", 25.0f }, { "dly_mix", 12.0f }, { "rev_mix", 10.0f } } },

        { "Podcast / Voz Hablada", {
            { "wide_on", 0.0f }, { "pre_type", 3.0f }, { "comp_type", 1.0f }, { "eq_type", 1.0f }, { "tune_on", 0.0f },
            { "pre_drive", 3.0f }, { "pre_warmth", 40.0f },
            { "eq_lowcut", 90.0f }, { "eq_low_gain", 1.5f }, { "eq_mid_freq", 300.0f }, { "eq_mid_gain", -2.0f },
            { "eq_pres_gain", 2.5f }, { "eq_air_gain", 1.5f },
            { "comp_thr", -24.0f }, { "comp_ratio", 4.0f }, { "comp_att", 8.0f }, { "comp_rel", 180.0f }, { "comp_makeup", 6.0f },
            { "dly_on", 0.0f }, { "rev_on", 0.0f }, { "ai_amount", 80.0f } } },
    };
    return presets;
}
} // namespace tvc
