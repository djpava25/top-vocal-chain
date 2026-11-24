#pragma once
#include "Render3D.h"

// Piezas 3D del frente de Top 3A (mapas de altura + material). Angulos: 0 = arriba, horario positivo.
namespace t3p
{
using namespace r3d;
inline Mat mixM (Mat a, const Mat& b, float t)
{
    a.r += (b.r - a.r) * t; a.g += (b.g - a.g) * t; a.b += (b.b - a.b) * t; a.metal += (b.metal - a.metal) * t; a.gloss += (b.gloss - a.gloss) * t;
    a.er += (b.er - a.er) * t; a.eg += (b.eg - a.eg) * t; a.eb += (b.eb - a.eb) * t; a.f0 += (b.f0 - a.f0) * t; a.alpha += (b.alpha - a.alpha) * t; return a;
}
inline Mat plastic (float r, float g, float b, float gloss) { Mat m = srgb (r, g, b, 0, gloss); m.f0 = 0.05f; return m; }
inline Mat chrome (float gloss = 0.86f, float tone = 0.90f) { return srgb (tone, tone + 0.004f, tone + 0.012f, 1, gloss); }
// pintura negra satinada del frente (con un leve tono calido, como la foto)
inline Mat panelPaint() { Mat m = srgb (0.078f, 0.073f, 0.075f, 0, 0.52f); m.f0 = 0.045f; return m; }
inline Mat ink() { Mat m = srgb (0.93f, 0.93f, 0.92f, 0, 0.30f); m.f0 = 0.03f; return m; }

// ---------------------------------------------------------------- falda de acrilico ahumado (deja ver el panel)
inline Sample smokeSkirt (float x, float y, float R, float Rb)
{
    Sample s; const float r = std::sqrt (x * x + y * y);
    s.m = plastic (0.035f, 0.035f, 0.04f, 0.95f); s.m.f0 = 0.07f;
    const float t = clamp01 ((r - Rb) / (R - Rb));                         // 0 junto al cuerpo, 1 en el borde
    s.m.alpha = clamp01 (R + 0.5f - r) * (0.62f + 0.30f * t * t);
    // cono: alto junto al cuerpo, baja hacia el borde redondeado
    s.h = (0.20f * R * (1.0f - 0.75f * t)) * fillet (R - r, 0.06f * R);
    if (r > 0.95f * R) { const float k = clamp01 ((r - 0.95f * R) / (0.03f * R)); s.m = mixM (s.m, plastic (0.10f, 0.10f, 0.11f, 0.97f), 0.5f * k); s.m.alpha = std::max (s.m.alpha, 0.9f * clamp01 (R + 0.5f - r)); }
    return s;
}

// ---------------------------------------------------------------- cuerpo negro moleteado + tapa de aluminio torneado
// vista levemente oblicua (camara arriba a la derecha, como la foto): la tapa se corre hacia arriba-izquierda
// (ox, oy en pixeles) y se ve la pared del cilindro con sus estrias verticales abajo a la derecha.
inline Sample knobBody (float x, float y, float R, float Rc, int teeth, float H, float ox, float oy)
{
    Sample s; s.m = plastic (0.030f, 0.030f, 0.033f, 0.55f); s.m.alpha = 0;
    // tapa superior (en el centro corrido)
    const float tx = x - ox, ty = y - oy, rt = std::sqrt (tx * tx + ty * ty);
    if (rt < R + 0.5f)
    {
        const float th = std::atan2 (ty, tx), tooth = 0.5f + 0.5f * std::cos ((float) teeth * th);
        s.m.alpha = clamp01 (R + 0.5f - rt);
        float h = H + 0.06f * R * fillet (R - rt, 0.08f * R);
        { const float g = 0.85f + 0.15f * tooth; s.m.r *= g; s.m.g *= g; s.m.b *= g; }
        if (rt < Rc + 1.0f)
        {
            // disco de aluminio torneado (satinado claro): anillos finos y brillo en "corbata" hacia la luz
            const float k = clamp01 (Rc + 0.5f - rt), u = rt / Rc;
            const float hc = H + 0.07f * R + 0.06f * Rc * std::sqrt (std::max (0.0f, 1.0f - u * u)) - 0.04f * Rc * (1.0f - fillet (Rc - rt, 0.09f * Rc));
            const float spin = 0.0018f * Rc * std::sin (rt * 2.6f) + 0.0012f * Rc * (vnoise (rt * 1.9f, 0.5f, 3) - 0.5f);
            h = h * (1.0f - k) + (hc + spin) * k;
            Mat c = srgb (0.80f, 0.80f, 0.79f, 0, 0.62f); c.f0 = 0.30f;
            const float tie = 1.0f + 0.16f * std::cos (2.0f * (th + 0.80f)) - 0.05f * u;
            const float tone = tie * (0.95f + 0.05f * vnoise (rt * 0.8f, th * 4.0f, 7));
            c.r *= tone; c.g *= tone; c.b *= tone;
            if (rt > 0.93f * Rc) { const float e = clamp01 ((rt - 0.93f * Rc) / (0.05f * Rc)); c.r *= 1 - 0.35f * e; c.g *= 1 - 0.35f * e; c.b *= 1 - 0.35f * e; }   // chaflan
            s.m = mixM (s.m, c, k);
        }
        s.h = h; return s;
    }
    // pared del cilindro: el punto esta sobre el borde del disco a la altura t (se toma la mas alta visible)
    const float L2 = ox * ox + oy * oy;
    float best = -1;
    if (L2 > 1e-6f)
    {
        // |p - t c| = R  ->  L2 t^2 - 2 (p.c) t + |p|^2 - R^2 = 0
        const float pc = x * ox + y * oy, pp = x * x + y * y - R * R;
        const float disc = pc * pc - L2 * pp;
        if (disc >= 0)
        {
            const float sq = std::sqrt (disc);
            for (float t : { (pc + sq) / L2, (pc - sq) / L2 }) if (t >= 0.0f && t <= 1.0f) best = std::max (best, t);
        }
        if (best < 0 && pp < 0) best = 0;                                   // dentro de la base
    }
    if (best < 0) { s.h = 0; return s; }
    const float qx = x - best * ox, qy = y - best * oy, th = std::atan2 (qy, qx);
    // estrias: la normal gira alrededor de la tangente
    const float ph = (float) teeth * th, gro = std::sin (ph) * 0.95f;
    const float nx = std::cos (th) - gro * std::sin (th), ny = std::sin (th) + gro * std::cos (th);
    s.hasN = true; s.n = norm ({ nx, ny, 0.30f });
    const float edge = clamp01 ((R + 0.5f) - std::sqrt (x * x + y * y)) ;
    s.m.alpha = std::max (edge, clamp01 (best * 40.0f));
    const float g = 0.45f + 0.55f * (0.5f + 0.5f * std::cos (ph));
    s.m.r *= g; s.m.g *= g; s.m.b *= g; s.m.gloss = 0.72f; s.m.f0 = 0.06f;
    s.h = H * best;
    return s;
}

// ---------------------------------------------------------------- tornillo Phillips negro de cabeza redonda
inline Sample phillips (float x, float y, float R)
{
    Sample s; const float r = std::sqrt (x * x + y * y);
    s.m = srgb (0.05f, 0.05f, 0.055f, 1, 0.62f); s.m.alpha = clamp01 (R + 0.5f - r);
    const float u = clamp01 (r / R);
    float h = 0.36f * R * std::sqrt (std::max (0.0f, 1.0f - u * u)) * (0.55f + 0.45f * fillet (R - r, 0.25f * R));
    const float c = std::min (std::abs (x), std::abs (y)), along = std::max (std::abs (x), std::abs (y));
    if (c < 0.11f * R && along < 0.62f * R) { const float d = 0.16f * R * (1.0f - along / (0.62f * R)) * clamp01 ((0.11f * R - c) / 0.04f / R + 0.3f); h -= d + 0.05f * R; s.m = srgb (0.01f, 0.01f, 0.012f, 0, 0.3f); }
    s.h = h; return s;
}

// ---------------------------------------------------------------- tuerca hexagonal cromada de la palanca (con buje)
inline Sample hexNut (float x, float y, float R)
{
    Sample s; s.m = chrome (0.78f, 0.86f); s.m.alpha = 0;
    const float hexd = std::max ({ std::abs (y), std::abs (y * 0.5f + x * 0.8660254f), std::abs (y * 0.5f - x * 0.8660254f) });
    const float Ri = R * 0.8660254f;                                        // apotema
    if (hexd > Ri + 1.0f) return s;
    s.m.alpha = clamp01 (Ri + 0.5f - hexd);
    float h = 0.30f * R * (0.35f + 0.65f * fillet (Ri - hexd, 0.12f * R));
    const float r = std::sqrt (x * x + y * y);
    if (r < 0.60f * R)                                                      // buje roscado
    {
        const float k = clamp01 (0.60f * R + 0.5f - r);
        h = h * (1.0f - k) + (0.42f * R + 0.01f * R * std::sin (r * 2.2f)) * k * (0.8f + 0.2f * fillet (0.6f * R - r, 0.1f * R));
        s.m = mixM (s.m, chrome (0.66f, 0.80f), k);
        if (r < 0.30f * R) { const float kk = clamp01 (0.30f * R + 0.5f - r); h -= 0.10f * R * kk; s.m = mixM (s.m, srgb (0.06f, 0.06f, 0.065f, 1, 0.4f), 0.8f * kk); }
    }
    s.h = h; return s;
}

// ---------------------------------------------------------------- palanca de bate (dir -1 izquierda / +1 derecha)
// vista de frente: cilindro corto que sale del buje hacia un lado y termina en una bolita, mas cerca de la camara
inline Sample batLever (float x, float y, float R, float dir)
{
    Sample s; s.m = chrome (0.88f, 0.92f); s.m.alpha = 0;
    const float L = 1.75f * R, r0 = 0.22f * R, r1 = 0.15f * R, rb = 0.30f * R;
    const float a = x * dir;                                                // a lo largo de la palanca
    float best = -1;
    if (a > -0.1f * R && a < L)
    {
        const float t = clamp01 (a / L), rr = r0 + (r1 - r0) * t;
        if (std::abs (y) < rr + 1.0f)
        {
            const float q = clamp01 (std::abs (y) / rr);
            const float z = std::sqrt (std::max (0.0f, 1.0f - q * q));
            best = 0.55f * R + 0.55f * R * t + rr * z;
            s.m.alpha = clamp01 (rr + 0.5f - std::abs (y));
            s.hasN = true; s.n = norm ({ -0.35f * dir * z, y / std::max (rr, 1e-3f), z });
        }
    }
    const float bx = x - dir * L, br = std::sqrt (bx * bx + y * y);
    if (br < rb + 1.0f)
    {
        const float q = clamp01 (br / rb), z = std::sqrt (std::max (0.0f, 1.0f - q * q));
        const float hb = 1.10f * R + rb * z;
        if (hb > best)
        {
            best = hb; s.m.alpha = std::max (s.m.alpha, clamp01 (rb + 0.5f - br));
            s.hasN = true; s.n = norm ({ bx / rb, y / rb, z });
        }
    }
    s.h = std::max (0.0f, best); return s;
}

// ---------------------------------------------------------------- piloto de POWER (lente ambar con aro negro)
inline Sample pilot (float x, float y, float R, bool on)
{
    Sample s; const float r = std::sqrt (x * x + y * y);
    s.m = srgb (0.04f, 0.04f, 0.045f, 0, 0.5f); s.m.alpha = clamp01 (R + 0.5f - r);
    float h = 0.25f * R * fillet (R - r, 0.3f * R);
    if (r < 0.62f * R)
    {
        const float k = clamp01 (0.62f * R + 0.5f - r), u = r / (0.62f * R);
        h += 0.22f * R * std::sqrt (std::max (0.0f, 1.0f - u * u)) * k;
        Mat lens = plastic (0.55f, 0.30f, 0.06f, 0.92f);
        if (on) { lens.er = 2.4f * (1.0f - 0.5f * u * u); lens.eg = 1.05f * (1.0f - 0.6f * u * u); lens.eb = 0.12f; }
        else { lens.r *= 0.35f; lens.g *= 0.35f; lens.b *= 0.35f; }
        s.m = mixM (s.m, lens, k);
    }
    s.h = h; return s;
}

// ---------------------------------------------------------------- boton cuadrado negro (bandeja)
inline Sample squareKey (float x, float y, float hw, float hh, bool pressed)
{
    Sample s; s.m = plastic (0.06f, 0.062f, 0.068f, 0.62f);
    const float rr = 4.0f, qx = std::abs (x) - (hw - rr), qy = std::abs (y) - (hh - rr);
    const float d = rr - (std::sqrt (std::max (qx, 0.0f) * std::max (qx, 0.0f) + std::max (qy, 0.0f) * std::max (qy, 0.0f)) + std::min (std::max (qx, qy), 0.0f));
    s.m.alpha = clamp01 (d + 0.5f);
    s.h = (pressed ? 3.0f : 7.0f) * fillet (d, 3.0f) + 0.4f * (1.0f - std::abs (y) / hh);
    return s;
}
} // namespace t3p
