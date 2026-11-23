#pragma once
#include "Render3D.h"

// Piezas 3D del panel del 6961P (mapas de altura + material). Angulos: 0 = arriba, horario positivo.
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

// ---------------------------------------------------------------- perilla de aluminio anodizado negro con aleta
inline Sample finKnob (float x, float y, float R, float ang)
{
    Sample s; s.m = anodBlack(); s.m.alpha = 0;
    const float r = std::sqrt (x * x + y * y);
    const float ca = std::cos (ang), sa = std::sin (ang);
    const float u = x * ca + y * sa, v = -x * sa + y * ca;                 // v < 0 hacia el indicador
    float h = -1;
    // base cilindrica con chaflan
    const float db = R - r;
    if (db > -1) { s.m.alpha = clamp01 (db + 0.5f); h = 0.42f * R * fillet (db, 0.10f * R) + 0.004f * R * fbm (x * 0.05f, y * 0.05f, 3, 2); }
    // aleta (barra alta a lo ancho), mas baja hacia la punta
    const float wf = 0.30f * R;
    const float d = std::min ({ wf - std::abs (u), v + 1.06f * R, 0.98f * R - v });
    if (d > -1)
    {
        s.m.alpha = std::max (s.m.alpha, clamp01 (d + 0.5f));
        const float slope = clamp01 ((-v - 0.15f * R) / (0.91f * R));
        const float hf = (0.98f * R - 0.28f * R * slope) * fillet (d, 0.035f * R) + 0.003f * R * fbm (x * 0.02f, y * 0.6f, 5, 2);
        if (hf > h) h = hf;
        // linea blanca en la punta
        if (std::abs (u) < 0.045f * R + 0.5f && v < -0.50f * R && v > -1.0f * R)
            s.m = mixM (s.m, srgb (0.94f, 0.94f, 0.92f, 0, 0.45f), clamp01 (0.045f * R - std::abs (u) + 0.5f));
    }
    s.h = std::max (h, 0.0f); return s;
}

// ---------------------------------------------------------------- palanca mini cromada (pos: -1 arriba, 0 centro, 1 abajo)
inline Sample miniToggle (float x, float y, float R, int pos)
{
    Sample s; s.m = chrome (0.50f); s.m.alpha = 0;
    const float r = std::sqrt (x * x + y * y);
    float h = -1;
    // tuerca redonda pulida con el hueco oscuro donde pivota la palanca
    if (r < R + 1)
    {
        s.m.alpha = clamp01 (R + 0.5f - r);
        h = 0.22f * R * fillet (R - r, 0.22f * R);
        if (r < 0.58f * R) { h = 0.12f * R; s.m = srgb (0.10f, 0.10f, 0.11f, 1, 0.5f); }
    }
    const float dir = pos == 0 ? 0.0f : (float) pos, len = 1.05f * R;
    // palanca cromada con bola en la punta
    const float t = dir == 0 ? 0.0f : clamp01 ((y * dir) / len);
    const float dx = x, dy = dir == 0 ? y : y - dir * len * t, rad = 0.20f * R;
    const float ds = std::sqrt (dx * dx + dy * dy);
    if (ds < rad + 1 && (dir == 0 || y * dir > -0.2f * R))
    {
        const float hs = 0.40f * R + 0.30f * R * t + rad * std::sqrt (std::max (0.0f, 1.0f - (ds / rad) * (ds / rad)));
        if (hs > h) { h = hs; s.m = chrome (0.85f); }
        s.m.alpha = std::max (s.m.alpha, clamp01 (rad - ds + 0.5f));
    }
    const float bx = x, by = y - dir * len, rb = (dir == 0 ? 0.30f : 0.30f) * R, dball = std::sqrt (bx * bx + by * by);
    if (dball < rb + 1)
    {
        const float hb = 0.80f * R + std::sqrt (std::max (0.0f, rb * rb - dball * dball));
        if (hb > h) { h = hb; s.m = chrome (0.88f); }
        s.m.alpha = std::max (s.m.alpha, clamp01 (rb - dball + 0.5f));
    }
    s.h = std::max (h, 0.0f); return s;
}

// ---------------------------------------------------------------- tornillo de cabeza redonda con hexagono interior
inline Sample hexScrew (float x, float y, float R)
{
    Sample s; s.m = chrome (0.78f);
    const float r = std::sqrt (x * x + y * y);
    s.m.alpha = clamp01 (R + 0.5f - r);
    float h = 0.30f * R * std::sqrt (std::max (0.0f, 1.0f - (r / R) * (r / R))) + 0.06f * R * fillet (R - r, 0.15f * R);
    const float hexd = std::max ({ std::abs (x), std::abs (x * 0.5f + y * 0.8660254f), std::abs (x * 0.5f - y * 0.8660254f) });
    const float dh = 0.40f * R - hexd;
    if (dh > -1) { const float k = clamp01 (dh + 0.5f); h -= 0.22f * R * k; s.m = mixM (s.m, srgb (0.12f, 0.12f, 0.13f, 1, 0.4f), k); }
    s.h = h; return s;
}

// ---------------------------------------------------------------- jack de 1/4" (tuerca cromada y agujero)
inline Sample jack (float x, float y, float R)
{
    Sample s; s.m = chrome (0.72f); s.m.r *= 0.55f; s.m.g *= 0.55f; s.m.b *= 0.57f;    // cromo oscuro
    const float r = std::sqrt (x * x + y * y);
    s.m.alpha = clamp01 (R + 0.5f - r);
    float h;
    if (r > 0.62f * R) h = 0.30f * R * fillet (R - r, 0.16f * R) + 0.012f * R * std::sin (r * 0.9f);
    else if (r > 0.46f * R) h = 0.30f * R + 0.10f * R * fillet (r - 0.46f * R, 0.05f * R);
    else { h = 0.30f * R - 0.4f * R * clamp01 ((0.46f * R - r) / (0.06f * R)); s.m = srgb (0.02f, 0.02f, 0.02f, 0, 0.3f); }
    s.h = h; return s;
}

// ---------------------------------------------------------------- lampara: copa de aluminio cepillado, aro negro y lente roja
inline Sample lamp (float x, float y, float R, bool on)
{
    Sample s; s.m = chrome (0.62f);
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
