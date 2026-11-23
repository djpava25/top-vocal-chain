#pragma once
#include "Render3D.h"

// Formas 3D (mapas de altura + material) de las piezas del panel, en pixeles de la imagen.
// Angulos: 0 = arriba, positivo = sentido horario.
namespace parts
{
using namespace r3d;
constexpr float PI = 3.14159265358979f;
inline float theta (float x, float y) { return std::atan2 (x, -y); }
inline float wrap (float a) { while (a > PI) a -= 2 * PI; while (a < -PI) a += 2 * PI; return a; }
inline Mat bakelite (float x, float y) { Mat m = srgb (0.030f, 0.030f, 0.033f, 0, 0.93f); m.f0 = 0.075f; (void) x; (void) y; return m; }   // baquelita laqueada
inline Mat chrome (float gloss = 0.86f) { return srgb (0.88f, 0.885f, 0.895f, 1, gloss); }
inline Mat mixMat (Mat a, const Mat& b, float t)
{
    a.r += (b.r - a.r) * t; a.g += (b.g - a.g) * t; a.b += (b.b - a.b) * t;
    a.metal += (b.metal - a.metal) * t; a.gloss += (b.gloss - a.gloss) * t; a.er += (b.er - a.er) * t; a.eg += (b.eg - a.eg) * t; a.eb += (b.eb - a.eb) * t;
    return a;
}

// ---------------------------------------------------------------- perilla de falda con estrella de 8 lobulos
inline Sample skirtKnob (float x, float y, float R, float ang)
{
    Sample s; const float r = std::sqrt (x * x + y * y), t = theta (x, y);
    s.m = bakelite (x, y); s.m.alpha = clamp01 (R + 0.5f - r);
    if (s.m.alpha <= 0) return s;
    auto skirtH = [R] (float rr) { return 0.30f * R * (1.0f - std::pow (std::min (rr / R, 1.0f), 2.4f)) + 0.025f * R * fillet (R - rr, 0.05f * R); };
    float h = skirtH (r);
    // estrella (agarre): 8 lobulos anchos con muescas en V angostas; la cara de arriba cae hacia afuera
    const float ri = 0.60f * R, ro = 0.76f * R;
    const float ph = wrap (8.0f * (t - ang));                                      // 0 = centro del lobulo
    const float notch = std::exp (-std::pow ((std::abs (ph) - PI) / 0.55f, 2.0f));
    const float starR = ro * (1.0f - 0.025f * (ph / PI) * (ph / PI)) - (ro - ri) * notch;
    const float d = starR - r;
    if (d > -1.0f)
    {
        const float top = 0.64f * R - 0.20f * R * std::pow (clamp01 ((r - 0.50f * R) / (0.26f * R)), 1.7f);   // corona curva
        h = std::max (h, skirtH (r) + (top - skirtH (r)) * fillet (d, 0.075f * R));
    }
    // tapa con anillo y centro hundido
    const float dc = 0.48f * R - r;
    if (dc > 0) h = std::max (h, 0.64f * R + 0.04f * R * fillet (dc, 0.025f * R));
    const float dr = 0.38f * R - r;
    if (dr > 0) h -= (0.06f * R + 0.025f * R * (1.0f - (r / (0.38f * R)) * (r / (0.38f * R)))) * fillet (dr, 0.025f * R);
    // imperfecciones de la baquelita moldeada
    h += 0.0015f * R * fbm (x * 0.05f, y * 0.05f, 3, 2);
    // linea blanca pintada en la falda
    if (r > 0.79f * R && r < 0.98f * R)
    {
        const float dist = std::abs (wrap (t - ang)) * r;
        const float c = clamp01 (0.024f * R - dist + 0.5f);
        if (c > 0) { s.m = mixMat (s.m, srgb (0.92f, 0.91f, 0.87f, 0, 0.55f), c); h += 0.003f * R * c; }
    }
    s.h = h; return s;
}

// ---------------------------------------------------------------- perilla de barra (CPS / KCS)
inline Sample barKnob (float x, float y, float L, float ang)
{
    Sample s; s.m = bakelite (x, y); s.m.alpha = 0;
    const float ca = std::cos (ang), sa = std::sin (ang);
    const float u = x * ca + y * sa, v = -x * sa + y * ca;               // v < 0 hacia la punta
    const float r = std::sqrt (x * x + y * y);
    float h = -1;
    // base redonda
    const float dh = 0.56f * L - r;
    if (dh > -1) { s.m.alpha = std::max (s.m.alpha, clamp01 (dh + 0.5f)); h = 0.30f * L * fillet (dh, 0.12f * L); }
    // barra
    const float w = 0.33f * L + 0.03f * L * (v + L) / (1.92f * L);
    const float d = std::min ({ w - std::abs (u), v + L, 0.92f * L - v });
    if (d > -1)
    {
        s.m.alpha = std::max (s.m.alpha, clamp01 (d + 0.5f));
        float hb = 0.62f * L;
        const float vt = -0.42f * L;                // borde de la cara inclinada
        if (v < vt)
        {
            const float st = (vt - v) / (0.58f * L);
            hb = 0.62f * L - 0.40f * L * st;                                   // cara de la punta cortada en angulo
        }
        hb = hb * fillet (d, 0.025f * L) + 0.0008f * L * fbm (x * 0.05f, y * 0.05f, 5, 2);
        h = std::max (h, hb);
    }
    s.h = std::max (h, 0.0f); return s;
}

// ---------------------------------------------------------------- selector de pico (ATTEN SEL): cuerpo acampanado con estrias
inline Sample peakKnob (float x, float y, float L, float ang)
{
    Sample s; s.m = bakelite (x, y); s.m.alpha = 0;
    const float ca = std::cos (ang), sa = std::sin (ang);
    const float u = x * ca + y * sa, v = -x * sa + y * ca;
    const float r = std::sqrt (x * x + y * y);
    float h = -1;
    const float dh = 0.58f * L - r;                                         // base redonda detras
    if (dh > -1) { s.m.alpha = clamp01 (dh + 0.5f); h = 0.26f * L * fillet (dh, 0.14f * L); }
    const float span = (v + 1.02f * L) / (1.62f * L);                       // 0 en la punta, 1 abajo
    const float w = 0.44f * L * std::pow (clamp01 (span), 0.75f);
    const float d = std::min ({ w - std::abs (u), 0.60f * L - v });
    if (d > -1 && v > -1.02f * L)
    {
        s.m.alpha = std::max (s.m.alpha, clamp01 (d + 0.5f));
        const float q = std::abs (u) / std::max (w, 1.0f);
        float hp = (0.40f * L + 0.16f * L * (1.0f - q * q)) * fillet (d, 0.04f * L);
        hp += 0.022f * L * std::cos (q * PI * 3.5f) * clamp01 (d / (0.04f * L)) * clamp01 (span * 3.0f);   // estrias
        h = std::max (h, hp);
        if (std::abs (u) < 0.03f * L + 0.5f && v < -0.55f * L && v > -0.98f * L)
            s.m = mixMat (s.m, srgb (0.93f, 0.92f, 0.88f, 0, 0.55f), clamp01 (0.03f * L - std::abs (u) + 0.5f));
    }
    s.h = std::max (h, 0.0f) + 0.0008f * L * fbm (x * 0.05f, y * 0.05f, 9, 2); return s;
}

// ---------------------------------------------------------------- perilla OFF/ON
inline Sample powerKnob (float x, float y, float R, float ang)
{
    Sample s; s.m = bakelite (x, y);
    const float r = std::sqrt (x * x + y * y);
    const float ca = std::cos (ang), sa = std::sin (ang);
    const float u = x * ca + y * sa, v = -x * sa + y * ca;
    s.m.alpha = clamp01 (R + 0.5f - r);
    float h = r < R + 1 ? 0.42f * R * fillet (R - r, 0.22f * R) + 0.06f * R * (1.0f - std::min (1.0f, (r / R) * (r / R))) : -1.0f;
    // pestana
    const float w = 0.24f * R * clamp01 ((v + 1.38f * R) / (0.55f * R));
    const float d = std::min (w - std::abs (u), -0.6f * R - v);
    if (d > -1 && v > -1.38f * R) { s.m.alpha = std::max (s.m.alpha, clamp01 (d + 0.5f)); h = std::max (h, 0.26f * R * fillet (d, 0.06f * R)); }
    s.h = std::max (h, 0.0f) + 0.001f * R * fbm (x * 0.05f, y * 0.05f, 11, 2); return s;
}

// ---------------------------------------------------------------- tornillo cromado de ranura
inline Sample screw (float x, float y, float R, float slot)
{
    Sample s; s.m = chrome (0.80f);
    const float r = std::sqrt (x * x + y * y);
    s.m.alpha = clamp01 (R + 0.5f - r);
    float h = 0.32f * R * std::sqrt (std::max (0.0f, 1.0f - (r / R) * (r / R)));
    const float u = x * std::cos (slot) + y * std::sin (slot);
    const float ds = 0.13f * R - std::abs (u);
    if (ds > -1 && r < 0.93f * R)
    {
        const float k = fillet (ds + 0.5f, 0.05f * R);
        h -= 0.20f * R * k;
        s.m = mixMat (s.m, srgb (0.25f, 0.26f, 0.27f, 1, 0.5f), k);
    }
    s.h = h + 0.006f * R * fbm (x * 0.2f, y * 0.2f, 13, 2); return s;
}

// ---------------------------------------------------------------- palanca: tuerca hexagonal, casquillo y bola negra
inline Sample toggle (float x, float y, float R, bool up)
{
    Sample s; s.m = chrome (0.30f); s.m.r *= 0.98f; s.m.b *= 0.90f; s.m.alpha = 0;    // niquel satinado (algo calido)
    const float r = std::sqrt (x * x + y * y);
    const float hexd = std::max ({ std::abs (y), std::abs (y * 0.5f + x * 0.8660254f), std::abs (y * 0.5f - x * 0.8660254f) });
    const float dn = R - hexd;
    float h = -1;
    if (dn > -1)
    {
        s.m.alpha = clamp01 (dn + 0.5f);
        h = 0.20f * R * fillet (dn, 0.16f * R) + 0.004f * R * fbm (x * 0.3f, y * 0.3f, 15, 2);   // tuerca con chaflan
    }
    const float db = 0.62f * R - r;
    if (db > 0) h = std::max (h, 0.20f * R + 0.18f * R * fillet (db, 0.05f * R) + 0.012f * R * std::sin (r * 1.6f));
    if (r < 0.34f * R) { h = std::max (h, 0.44f * R + 0.06f * R * fillet (0.34f * R - r, 0.08f * R)); s.m = chrome (0.45f); }
    // la palanca se inclina hacia el que mira: la bola queda sobre la tuerca, arriba (IN) o abajo
    const float bxc = -0.40f * R, byc = (up ? -0.28f : 0.30f) * R, rb = 0.68f * R;
    const float bx = x - bxc, by = y - byc, dball = std::sqrt (bx * bx + by * by);
    if (dball < rb + 1)
    {
        const float hb = 0.95f * R + std::sqrt (std::max (0.0f, rb * rb - dball * dball));
        if (hb > h) { h = hb; s.m = srgb (0.03f, 0.03f, 0.032f, 0, 0.90f); s.m.f0 = 0.07f; }
        s.m.alpha = std::max (s.m.alpha, clamp01 (rb - dball + 0.5f));
    }
    s.h = std::max (h, 0.0f); return s;
}

// ---------------------------------------------------------------- lampara piloto: bisel moleteado y joya facetada
inline Sample jewel (float x, float y, float R, bool on)
{
    Sample s; s.m = chrome (0.82f);
    const float r = std::sqrt (x * x + y * y), t = theta (x, y);
    s.m.alpha = clamp01 (R + 0.5f - r);
    float h = 0;
    const float rj = 0.68f * R;
    if (r >= rj)
    {
        h = 0.24f * R * fillet (R - r, 0.12f * R);
        if (r > 0.86f * R) h += 0.014f * R * std::cos (t * 54.0f);                       // moleteado
        if (r < 0.80f * R) h = 0.24f * R - 0.10f * R * (0.80f * R - r) / (0.12f * R);   // bisel interior
    }
    else
    {
        // joya: poliedro convexo (tabla + 8 + 16 facetas tangentes a una esfera)
        const float Rs = 0.80f * R, cz = -0.48f * R;
        float hj = 1e9f;
        auto facet = [&] (float phi, float el)
        {
            const float nx = std::sin (el) * std::sin (phi), ny = -std::sin (el) * std::cos (phi), nz = std::cos (el);
            hj = std::min (hj, (Rs - nx * x - ny * y) / nz + cz);
        };
        facet (0, 0);
        for (int k = 0; k < 8; ++k) facet ((float) k * PI / 4 + PI / 8, 0.42f);
        for (int k = 0; k < 16; ++k) facet ((float) k * PI / 8, 0.80f);
        h = std::max (hj, 0.10f * R);
        Mat g = srgb (0.62f, 0.05f, 0.08f, 0, 0.97f); g.f0 = 0.08f;
        const float spark = smooth ((vnoise (x * 0.55f + 7, y * 0.55f, 21) - 0.72f) * 7.0f);
        const float depth = 0.5f + 0.5f * (1.0f - r / rj);
        if (on) { g.er = 0.22f * depth + 1.4f * spark; g.eg = 0.012f * depth + 0.55f * spark; g.eb = 0.01f + 0.10f * spark; }
        else { g.er = 0.02f * depth; g.eg = 0.0f; g.eb = 0.002f; }
        s.m = mixMat (s.m, g, clamp01 (rj - r + 0.5f));
    }
    s.h = h; return s;
}
} // namespace parts
