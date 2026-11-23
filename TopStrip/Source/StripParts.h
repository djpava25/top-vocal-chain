#pragma once
#include "Render3D.h"

// Piezas 3D del panel del Top Strip 737 (mapas de altura + material). Angulos: 0 = arriba, horario positivo.
// Las perillas se ven con la perspectiva de una foto de producto: la cara se desplaza un poco respecto a la base
// y deja ver la pared lateral moleteada, como en el aparato real.
namespace s7p
{
using namespace r3d;
inline Mat mixM (Mat a, const Mat& b, float t)
{
    a.r += (b.r - a.r) * t; a.g += (b.g - a.g) * t; a.b += (b.b - a.b) * t; a.metal += (b.metal - a.metal) * t; a.gloss += (b.gloss - a.gloss) * t;
    a.er += (b.er - a.er) * t; a.eg += (b.eg - a.eg) * t; a.eb += (b.eb - a.eb) * t; a.f0 += (b.f0 - a.f0) * t; a.alpha += (b.alpha - a.alpha) * t; return a;
}
inline Mat chrome (float gloss = 0.90f, float tone = 0.90f) { return srgb (tone, tone + 0.004f, tone + 0.012f, 1, gloss); }
inline Mat satin (float tone = 0.86f) { Mat m = srgb (tone, tone + 0.005f, tone, 0, 0.42f); m.metal = 0.4f; m.f0 = 0.10f; return m; }

// ---------------------------------------------------------------- perilla de aluminio pulido (cara satinada, bisel espejo, lado moleteado)
struct KnobStyle
{
    float faceFrac = 0.76f;     // radio de la cara satinada
    float height = 0.80f;       // altura de la perilla (en R)
    float bevel = 0.30f;        // profundidad del bisel redondeado
    int knurl = 90;             // estrias del moleteado
    float ptrIn = 0.30f, ptrOut = 0.74f, ptrW = 0.17f;
};
inline Sample knob (float x, float y, float R, float ang, float ox, float oy, const KnobStyle& st)
{
    Sample s; s.m = chrome(); s.m.alpha = 0;
    const float H = st.height * R;
    // ---- pared lateral: barrido de la circunferencia desde la base (0,0) hasta la cara (ox, oy)
    float wallA = 0; Sample w;
    {
        const float o2 = ox * ox + oy * oy;
        // distancia al segmento del eje (para el borde suavizado)
        float t = o2 > 1e-6f ? clamp01 ((x * ox + y * oy) / o2) : 0.0f;
        const float dseg = std::sqrt ((x - t * ox) * (x - t * ox) + (y - t * oy) * (y - t * oy));
        wallA = clamp01 (R + 0.5f - dseg);
        if (wallA > 0 && o2 > 1e-6f)
        {
            const float pd = x * ox + y * oy, disc = pd * pd - o2 * (x * x + y * y - R * R);
            float sw = disc >= 0 ? (pd + std::sqrt (disc)) / o2 : t;
            sw = clamp01 (sw);
            const float cx = (x - sw * ox) / R, cy = (y - sw * oy) / R;
            const float phi = std::atan2 (cy, cx);
            // moleteado: facetas en dientes de sierra alrededor del eje
            const float k = (float) st.knurl * phi / (2.0f * 3.14159265f);
            const float tri = (k - std::floor (k)) - 0.5f;
            const float pn = phi + 0.9f * tri * (2.0f * 3.14159265f / (float) st.knurl);
            w.hasN = true; w.n = norm ({ std::cos (pn), std::sin (pn), 1.15f });
            // aluminio moleteado: refleja la luz del estudio de forma difusa, con lineas claras y oscuras
            const float line = 0.86f + 0.14f * std::sin (k * 2.0f * 3.14159265f);
            w.m = srgb (0.86f * line, 0.865f * line, 0.87f * line, 0, 0.62f); w.m.metal = 0.3f; w.m.f0 = 0.25f;
            const float ao = 0.55f + 0.45f * sw;                                 // mas oscuro cerca del panel
            w.m.r *= ao; w.m.g *= ao; w.m.b *= ao;
            w.h = sw * H;
        }
        else if (wallA > 0) { w.h = H * 0.5f; w.m = chrome (0.8f, 0.8f); }
    }
    // ---- cara superior (bisel + cara + indicador), con transiciones suavizadas de 1 px
    const float fx = x - ox, fy = y - oy, rf = std::sqrt (fx * fx + fy * fy);
    const float topA = clamp01 (R + 0.5f - rf);
    Sample tp; tp.m = chrome();
    if (topA > 0)
    {
        const float Rf = st.faceFrac * R;
        // bisel pulido (espejo)
        const float t = clamp01 ((rf - Rf) / (R - Rf));
        const float hb = H - st.bevel * R * (1.0f - std::sqrt (std::max (0.0f, 1.0f - t * t)));
        const Mat mb = chrome (0.95f, 0.95f);
        // cara satinada con marcas de torno
        Mat f = satin (0.80f);
        const float lathe = 0.975f + 0.025f * std::sin (rf * 2.4f) + 0.02f * fbm (fx * 0.08f, fy * 0.08f, 11, 2);
        f.r *= lathe; f.g *= lathe; f.b *= lathe;
        const float fw = clamp01 (Rf - rf + 0.5f);
        tp.h = H * fw + hb * (1.0f - fw);
        tp.m = mixM (mb, f, fw);
        if (fw > 0)
        {
            // indicador: aleta elevada y redondeada
            const float sa = std::sin (ang), ca = std::cos (ang);
            const float along = fx * sa - fy * ca, across = fx * ca + fy * sa;
            const float hw = st.ptrW * R * 0.5f;
            const float dEnd = std::min (along - st.ptrIn * R, st.ptrOut * R - along);
            const float d = std::min (hw - std::abs (across), dEnd);
            if (d > -1.0f)
            {
                const float k = clamp01 (d + 0.5f) * fw;
                const float prof = std::sqrt (std::max (0.0f, 1.0f - (across / hw) * (across / hw)));
                const float ramp = smooth (dEnd / (0.10f * R));
                tp.h += k * 0.11f * R * prof * ramp;
                tp.m = mixM (tp.m, satin (0.95f), k);
            }
        }
    }
    if (topA <= 0 && wallA <= 0) return s;
    if (topA >= 0.999f || wallA <= 0) { tp.m.alpha = topA; return tp; }
    if (topA <= 0.001f) { w.m.alpha = wallA; return w; }
    // borde entre la cara y la pared
    Sample m = topA >= 0.5f ? tp : w;
    m.m.alpha = std::max (topA, wallA);
    return m;
}

// ---------------------------------------------------------------- boton iluminado (tapa de acrilico rojo)
// text(x, y) -> 0..1 cobertura de las letras serigrafiadas en la tapa
template <typename TextFn>
inline Sample lampButton (float x, float y, float hw, float hh, bool on, TextFn text)
{
    Sample s; s.m = srgb (0.02f, 0.02f, 0.02f, 0, 0.5f); s.m.alpha = 0;
    auto rrDist = [] (float px, float py, float ax, float ay, float r)
    {
        const float qx = std::abs (px) - ax + r, qy = std::abs (py) - ay + r;
        return std::min (std::max (qx, qy), 0.0f) + std::sqrt (std::max (qx, 0.0f) * std::max (qx, 0.0f) + std::max (qy, 0.0f) * std::max (qy, 0.0f)) - r;
    };
    // hueco del panel (marco oscuro)
    const float dHole = rrDist (x, y, hw + 0.11f * hh, hh + 0.11f * hh, 0.16f * hh);
    if (dHole < 0.5f) { s.m.alpha = clamp01 (0.5f - dHole); s.h = -0.15f * hh; }
    const float dCap = rrDist (x, y, hw, hh, 0.22f * hh);
    if (dCap < 0.5f)
    {
        const float a = clamp01 (0.5f - dCap);
        Mat c = srgb (on ? 0.62f : 0.13f, on ? 0.07f : 0.022f, on ? 0.09f : 0.026f, 0, 0.90f); c.f0 = 0.055f;
        const float nx = x / hw, ny = y / hh, d2 = std::min (1.0f, 0.55f * nx * nx + 0.85f * ny * ny);
        if (on)
        {
            const float g = 1.0f - d2;
            c.er = 0.55f + 1.15f * g + 0.25f * fbm (x * 0.15f, y * 0.3f, 4, 2);
            c.eg = 0.02f + 0.10f * g * g; c.eb = 0.03f + 0.10f * g * g;
        }
        else { c.er = 0.025f; }
        const float tx = text (x, y);
        if (tx > 0)
        {
            Mat t = on ? srgb (0.95f, 0.85f, 0.86f, 0, 0.6f) : srgb (0.45f, 0.40f, 0.40f, 0, 0.6f);
            if (on) { t.er = 1.35f; t.eg = 0.95f; t.eb = 0.98f; }
            c = mixM (c, t, tx * 0.92f);
        }
        s.m = mixM (s.m, c, a);
        s.m.alpha = std::max (s.m.alpha, a);
        s.h = 0.20f * hh + 0.42f * hh * fillet (-dCap, 0.30f * hh) + 0.01f * hh * fbm (x * 0.2f, y * 0.2f, 9, 2);
    }
    return s;
}

// ---------------------------------------------------------------- tornillo allen de cabeza redonda con arandela
inline Sample screw (float x, float y, float R, float rot = 0.0f)
{
    Sample s; s.m = chrome (0.70f, 0.82f);
    const float r = std::sqrt (x * x + y * y);
    const float alpha = clamp01 (R + 0.5f - r);
    if (alpha <= 0) { s.m.alpha = 0; return s; }
    const float rh = 0.70f * R;
    float h;
    if (r > rh) { h = 0.12f * R * fillet (R - r, 0.10f * R); s.m = chrome (0.62f, 0.80f); const float br = 0.94f + 0.06f * std::sin (r * 3.0f); s.m.r *= br; s.m.g *= br; s.m.b *= br; }
    else
    {
        h = 0.14f * R + 0.30f * R * std::sqrt (std::max (0.0f, 1.0f - (r / rh) * (r / rh)));
        s.m = chrome (0.84f, 0.88f);
        const float cr = std::cos (rot), sr = std::sin (rot), u = x * cr + y * sr, v = -x * sr + y * cr;
        const float hexd = std::max ({ std::abs (u), std::abs (u * 0.5f + v * 0.8660254f), std::abs (u * 0.5f - v * 0.8660254f) });
        const float dh = 0.21f * R - hexd;
        if (dh > -1) { const float k = clamp01 (dh + 0.5f); h -= 0.30f * R * k; s.m = mixM (s.m, srgb (0.08f, 0.08f, 0.085f, 1, 0.4f), k); }
    }
    s.m.alpha = alpha; s.h = h; return s;
}

// ---------------------------------------------------------------- jack de instrumento: cubeta gris, aro negro, tuerca hexagonal, agujero
inline Sample jack (float x, float y, float R)
{
    Sample s;
    const float r = std::sqrt (x * x + y * y);
    const float alpha = clamp01 (R + 0.5f - r);
    if (alpha <= 0) { s.m.alpha = 0; return s; }
    float h;
    if (r > 0.64f * R) { h = -0.04f * R * fillet (R - r, 0.05f * R); s.m = srgb (0.47f, 0.48f, 0.48f, 0, 0.30f); }    // cubeta gris mate
    else if (r > 0.44f * R)
    {
        h = 0.10f * R * fillet (0.64f * R - r, 0.06f * R); s.m = srgb (0.03f, 0.03f, 0.035f, 0, 0.70f);              // aro de plastico negro
    }
    else
    {
        const float hexd = std::max ({ std::abs (y), std::abs (y * 0.5f + x * 0.8660254f), std::abs (y * 0.5f - x * 0.8660254f) });
        if (hexd < 0.37f * R)
        {
            h = 0.12f * R + 0.06f * R * fillet (0.37f * R - hexd, 0.04f * R); s.m = chrome (0.70f, 0.90f);              // tuerca
            if (r < 0.19f * R) { h = 0.0f - 0.2f * R * clamp01 ((0.19f * R - r) / (0.04f * R)); s.m = srgb (0.015f, 0.015f, 0.015f, 0, 0.3f); }
            else if (r < 0.23f * R) { h = 0.14f * R; s.m = chrome (0.85f, 0.75f); }
        }
        else { h = 0.04f * R; s.m = srgb (0.035f, 0.035f, 0.04f, 0, 0.6f); }
    }
    s.m.alpha = alpha; s.h = h; return s;
}

// ---------------------------------------------------------------- interruptor basculante de encendido (marco gris + balancin negro)
inline Sample rocker (float x, float y, float hw, float hh, bool on, float symbolMask)
{
    Sample s; s.m.alpha = 0;
    auto rrDist = [] (float px, float py, float ax, float ay, float r)
    {
        const float qx = std::abs (px) - ax + r, qy = std::abs (py) - ay + r;
        return std::min (std::max (qx, qy), 0.0f) + std::sqrt (std::max (qx, 0.0f) * std::max (qx, 0.0f) + std::max (qy, 0.0f) * std::max (qy, 0.0f)) - r;
    };
    const float dB = rrDist (x, y, hw, hh, 0.16f * hh);
    if (dB < 0.5f)
    {
        s.m = srgb (0.52f, 0.53f, 0.53f, 0, 0.35f); s.m.alpha = clamp01 (0.5f - dB);
        s.h = 0.08f * hh * fillet (-dB, 0.10f * hh);
    }
    const float iw = hw * 0.82f, ih = hh * 0.80f;
    const float dR = rrDist (x, y, iw, ih, 0.10f * hh);
    if (dR < 0.5f)
    {
        const float a = clamp01 (0.5f - dR);
        const float side = on ? -1.0f : 1.0f;                         // lado levantado
        float h = 0.05f * hh + 0.30f * hh * clamp01 (0.5f + 0.5f * side * x / iw) + 0.04f * hh * fillet (-dR, 0.08f * hh);
        Mat m = srgb (0.045f, 0.045f, 0.05f, 0, 0.62f);
        if (symbolMask > 0) m = mixM (m, srgb (0.80f, 0.80f, 0.78f, 0, 0.5f), symbolMask);
        s.m = mixM (s.m, m, a); s.m.alpha = std::max (s.m.alpha, a);
        s.h = s.h * (1 - a) + h * a;
    }
    return s;
}

// ---------------------------------------------------------------- LED rojo de encendido (domo en un agujero del panel)
inline Sample led (float x, float y, float R, bool on)
{
    Sample s;
    const float r = std::sqrt (x * x + y * y);
    const float alpha = clamp01 (R + 0.5f - r);
    if (r > 0.80f * R) { s.m = srgb (0.06f, 0.05f, 0.05f, 0, 0.4f); s.h = -0.15f * R; s.m.alpha = alpha; return s; }
    const float rr = r / (0.80f * R);
    Mat g = srgb (on ? 0.95f : 0.42f, on ? 0.30f : 0.05f, on ? 0.32f : 0.06f, 0, 0.94f); g.f0 = 0.06f;
    if (on) { g.er = 1.0f + 1.1f * (1 - rr * rr); g.eg = 0.10f + 0.40f * (1 - rr) * (1 - rr); g.eb = 0.12f + 0.36f * (1 - rr) * (1 - rr); }
    s.m = g; s.m.alpha = alpha; s.h = 0.05f * R + 0.40f * R * std::sqrt (std::max (0.0f, 1.0f - rr * rr));
    return s;
}
} // namespace s7p
