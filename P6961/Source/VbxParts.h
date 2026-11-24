#pragma once
#include "Render3D.h"

// Piezas 3D del panel de Top VoxStrips (mapas de altura + material). Angulos: 0 = arriba, horario positivo.
namespace vbparts
{
using namespace r3d;
constexpr float PI = 3.14159265358979f;
inline Mat mixM (Mat a, const Mat& b, float t)
{
    a.r += (b.r - a.r) * t; a.g += (b.g - a.g) * t; a.b += (b.b - a.b) * t; a.metal += (b.metal - a.metal) * t; a.gloss += (b.gloss - a.gloss) * t;
    a.er += (b.er - a.er) * t; a.eg += (b.eg - a.eg) * t; a.eb += (b.eb - a.eb) * t; a.f0 += (b.f0 - a.f0) * t; return a;
}
inline Mat anodBlack (float gloss = 0.40f) { Mat m = srgb (0.040f, 0.040f, 0.045f, 0, gloss); m.f0 = 0.035f; return m; }   // anodizado satinado
inline Mat chrome (float gloss = 0.86f) { return srgb (0.88f, 0.885f, 0.895f, 1, gloss); }

// ---------------------------------------------------------------- perilla negra con barra central y dos conchas laterales
// Como en la foto: barra ancha de textura mate (granallada) con la linea blanca, y a cada lado una concha
// concava brillante que baja hacia el borde redondeado. La luz del estudio deja reflejos verticales en las conchas.
inline Sample finKnob (float x, float y, float R, float ang)
{
    Sample s; s.m = anodBlack(); s.m.alpha = 0;
    const float r = std::sqrt (x * x + y * y);
    if (r > R + 1.0f) { s.h = 0; return s; }
    s.m.alpha = clamp01 (R + 0.5f - r);
    const float ca = std::cos (ang), sa = std::sin (ang);
    const float u = x * ca + y * sa, v = -x * sa + y * ca;                 // v < 0 hacia el indicador
    const float H = 0.62f * R, wf = 0.44f * R;
    const float rim = fillet (R - r, 0.07f * R);                          // canto exterior redondeado
    const float au = std::abs (u);
    float h;
    Mat m;
    if (au < wf)
    {
        // barra: tapa casi plana, cantos suaves, grano mate
        const float e = fillet (wf - au, 0.09f * R);
        h = H * (0.86f + 0.14f * e) + 0.010f * R * fbm (x * 0.35f, y * 0.35f, 4, 2);
        m = srgb (0.055f, 0.055f, 0.060f, 0, 0.30f); m.f0 = 0.03f;
        const float g = 0.85f + 0.3f * vnoise (x * 0.9f, y * 0.9f, 8);   // granallado
        m.r *= g; m.g *= g; m.b *= g;
        // linea blanca del indicador, sobre la barra
        const float lw = 0.036f * R;
        if (au < lw + 0.6f && v < -0.40f * R && v > -0.93f * R)
        {
            const float k = clamp01 (lw - au + 0.6f) * clamp01 ((-0.40f * R - v) / (0.02f * R + 0.5f)) * clamp01 ((v + 0.93f * R) / (0.02f * R + 0.5f));
            m = mixM (m, srgb (0.95f, 0.95f, 0.93f, 0, 0.50f), k);
            h -= 0.004f * R * k;
        }
    }
    else
    {
        // concha concava: baja desde la barra y se aplana hacia el borde
        const float chord = std::sqrt (std::max (1.0f, R * R - v * v));
        const float t = clamp01 ((au - wf) / std::max (1.0f, chord - wf));
        h = H * (0.66f + 0.34f * (1.0f - t) * (1.0f - t));
        m = srgb (0.030f, 0.030f, 0.034f, 0, 0.56f); m.f0 = 0.045f;     // plastico negro satinado: reflejos suaves y anchos
    }
    h *= 0.35f + 0.65f * rim;
    if (r > 0.90f * R) { const float k = clamp01 ((r - 0.90f * R) / (0.06f * R)); m.gloss += (0.30f - m.gloss) * k; m.f0 += (0.03f - m.f0) * k; }   // canto mate: sin aro brillante
    s.m = m; s.m.alpha = clamp01 (R + 0.5f - r);
    s.h = std::max (h, 0.0f); return s;
}

// ---------------------------------------------------------------- palanca mini cromada (pos: -1 arriba, 0 centro, 1 abajo)
// Tuerca redonda pulida con ranura de chaveta arriba, buje oscuro, palanca conica (bat) hacia la camara y bola en la punta.
inline Sample miniToggle (float x, float y, float R, int pos)
{
    Sample s; s.m = chrome (0.80f); s.m.alpha = 0;
    const float r = std::sqrt (x * x + y * y);
    float h = -1;
    if (r < R + 1)
    {
        s.m.alpha = clamp01 (R + 0.5f - r);
        h = 0.20f * R * fillet (R - r, 0.25f * R);
        if (r < 0.66f * R) { h = 0.24f * R + 0.04f * R * fillet (0.66f * R - r, 0.06f * R); s.m = chrome (0.55f); s.m.r *= 0.62f; s.m.g *= 0.62f; s.m.b *= 0.64f; }   // buje
        if (r < 0.46f * R) { h = 0.20f * R; s.m = srgb (0.06f, 0.06f, 0.065f, 1, 0.5f); }
        if (std::abs (x) < 0.07f * R && y < -0.70f * R && y > -0.95f * R) { h -= 0.08f * R; s.m = srgb (0.05f, 0.05f, 0.05f, 0, 0.3f); }   // chaveta
    }
    const float dir = (float) pos, len = 1.02f * R;
    // palanca conica: ancha en la base, angosta antes de la bola; sube hacia la camara
    if (pos != 0)
    {
        const float t = clamp01 ((y * dir) / len);
        if (y * dir > -0.25f * R && y * dir < len)
        {
            const float rad = (0.30f - 0.13f * t) * R, dx = std::abs (x);
            if (dx < rad + 1)
            {
                const float hs = 0.45f * R + 0.55f * R * t + rad * std::sqrt (std::max (0.0f, 1.0f - (dx / rad) * (dx / rad)));
                if (hs > h) { h = hs; s.m = chrome (0.90f); }
                s.m.alpha = std::max (s.m.alpha, clamp01 (rad - dx + 0.5f));
            }
        }
    }
    const float by = y - dir * len, rb = (pos == 0 ? 0.36f : 0.30f) * R, dball = std::sqrt (x * x + by * by);
    if (dball < rb + 1)
    {
        const float hb = (pos == 0 ? 1.1f : 1.0f) * R + std::sqrt (std::max (0.0f, rb * rb - dball * dball));
        if (hb > h) { h = hb; s.m = chrome (0.92f); }
        s.m.alpha = std::max (s.m.alpha, clamp01 (rb - dball + 0.5f));
    }
    s.h = std::max (h, 0.0f); return s;
}

// ---------------------------------------------------------------- tornillo de cabeza redonda con hexagono interior
inline Sample hexScrew (float x, float y, float R)
{
    // cabeza cilindrica cromada (canto redondeado, tapa casi plana) con hexagono hundido de paredes grises
    Sample s; s.m = chrome (0.80f);
    const float r = std::sqrt (x * x + y * y);
    s.m.alpha = clamp01 (R + 0.5f - r);
    float h = 0.32f * R * fillet (R - r, 0.16f * R) + 0.03f * R * (1.0f - (r / R) * (r / R));
    const float hexd = std::max ({ std::abs (x), std::abs (x * 0.5f + y * 0.8660254f), std::abs (x * 0.5f - y * 0.8660254f) });
    const float dh = 0.44f * R - hexd;
    if (dh > -1)
    {
        const float k = clamp01 (dh + 0.5f);
        h -= 0.20f * R * k * fillet (dh, 0.10f * R);
        s.m = mixM (s.m, srgb (0.55f, 0.55f, 0.57f, 1, 0.55f), k);
        if (dh > 0.14f * R) s.m = mixM (s.m, srgb (0.30f, 0.30f, 0.32f, 1, 0.45f), clamp01 ((dh - 0.14f * R) / (0.08f * R)));
    }
    s.h = h; return s;
}

// ---------------------------------------------------------------- jack de 1/4" (tuerca de niquel negro pulido, labio y agujero)
inline Sample jack (float x, float y, float R)
{
    // aro exterior negro, toro cromado brillante, labio oscuro y agujero negro (como la foto)
    Sample s; s.m = srgb (0.05f, 0.05f, 0.055f, 0, 0.50f);
    const float r = std::sqrt (x * x + y * y);
    s.m.alpha = clamp01 (R + 0.5f - r);
    float h;
    if (r > 0.86f * R) h = 0.18f * R * fillet (R - r, 0.08f * R);
    else if (r > 0.52f * R)
    {
        const float d = (r - 0.69f * R) / (0.17f * R);
        h = 0.18f * R + 0.16f * R * std::sqrt (std::max (0.0f, 1.0f - d * d));
        s.m = chrome (0.86f);
    }
    else if (r > 0.44f * R) { h = 0.14f * R; s.m = srgb (0.25f, 0.25f, 0.27f, 1, 0.75f); }
    else { h = 0.14f * R - 0.5f * R * clamp01 ((0.44f * R - r) / (0.05f * R)); s.m = srgb (0.010f, 0.010f, 0.010f, 0, 0.3f); }
    s.h = h; return s;
}

// ---------------------------------------------------------------- lampara: copa de aluminio cepillado, aro negro y lente roja
inline Sample lamp (float x, float y, float R, bool on)
{
    Sample s; s.m = chrome (0.70f); s.m.r *= 0.55f; s.m.g *= 0.56f; s.m.b *= 0.58f;      // cromo ahumado: refleja oscuro como en la foto
    const float r = std::sqrt (x * x + y * y), t = std::atan2 (x, -y);
    s.m.alpha = clamp01 (R + 0.5f - r);
    float h;
    if (r > 0.42f * R)
    {
        // copa conica hacia adentro, cepillado radial
        h = 0.10f * R * fillet (R - r, 0.04f * R) - 0.22f * R * clamp01 ((R - r) / (0.58f * R));
        const float br = 0.93f + 0.07f * fbm (t * 40.0f, r * 0.02f, 7, 2);
        s.m.r *= br; s.m.g *= br; s.m.b *= br;
    }
    else if (r > 0.33f * R) { h = -0.22f * R + 0.06f * R * fillet (std::min (r - 0.33f * R, 0.42f * R - r), 0.03f * R); s.m = srgb (0.03f, 0.03f, 0.03f, 0, 0.6f); }
    else
    {
        h = -0.18f * R + 0.14f * R * std::sqrt (std::max (0.0f, 1.0f - (r / (0.33f * R)) * (r / (0.33f * R))));
        Mat g = srgb (0.75f, 0.10f, 0.10f, 0, 0.9f); g.f0 = 0.06f;
        const float c = 1.0f - r / (0.33f * R);
        if (on) { g.er = 0.9f + 1.6f * c * c; g.eg = 0.05f + 0.35f * c * c; g.eb = 0.05f + 0.25f * c * c; }
        else { g.er = 0.04f; g.eg = 0.0f; g.eb = 0.0f; }
        s.m = g;
    }
    s.h = h; return s;
}
} // namespace vbparts
