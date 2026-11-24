#pragma once
#include "Render3D.h"

// Piezas 3D del modulo vertical de Top 73 (mapas de altura + material). Angulos: 0 = arriba, horario positivo.
// Medidas en pixeles de la foto frontal del modulo (panel de 334 px de ancho).
namespace t73p
{
using namespace r3d;
inline Mat mixM (Mat a, const Mat& b, float t)
{
    a.r += (b.r - a.r) * t; a.g += (b.g - a.g) * t; a.b += (b.b - a.b) * t; a.metal += (b.metal - a.metal) * t; a.gloss += (b.gloss - a.gloss) * t;
    a.er += (b.er - a.er) * t; a.eg += (b.eg - a.eg) * t; a.eb += (b.eb - a.eb) * t; a.f0 += (b.f0 - a.f0) * t; return a;
}
inline Mat paint() { Mat m = srgb (0.214f, 0.262f, 0.296f, 0, 0.34f); m.f0 = 0.035f; return m; }      // pintura azul pizarra semimate
inline Mat ink() { Mat m = srgb (0.95f, 0.95f, 0.94f, 0, 0.30f); m.f0 = 0.03f; return m; }
inline Mat chrome (float gloss = 0.86f, float tone = 0.90f) { return srgb (tone, tone + 0.004f, tone + 0.012f, 1, gloss); }
inline Mat plastic (float r, float g, float b, float gloss) { Mat m = srgb (r, g, b, 0, gloss); m.f0 = 0.05f; return m; }

// ---------------------------------------------------------------- falda de acrilico transparente (deja ver el panel)
inline Sample skirt (float x, float y, float R)
{
    Sample s; const float r = std::sqrt (x * x + y * y);
    s.m = plastic (0.275f, 0.315f, 0.350f, 0.94f); s.m.f0 = 0.07f;
    s.m.alpha = clamp01 (R + 0.5f - r);
    const float e = fillet (R - r, 0.07f * R);
    s.h = 0.06f * R * e + 0.012f * R * (1.0f - (r / R) * (r / R));
    if (r > 0.93f * R) { const float k = clamp01 ((r - 0.93f * R) / (0.04f * R)); s.m = mixM (s.m, plastic (0.42f, 0.46f, 0.50f, 0.96f), 0.6f * k); }   // canto pulido
    return s;
}

// ---------------------------------------------------------------- perilla de dos alas (roja de ganancia / azul del filtro)
// tapa redonda: cara mate un poco hundida y aro brillante; dos alas opuestas que salen del aro,
// ranura oscura en el ala de atras y linea blanca en el ala del indicador
inline Sample wingKnob (float x, float y, float Rc, float Rw, float hw, float ang, const Mat& body, bool lineOnPointer)
{
    Sample s; s.m = body; s.m.alpha = 0;
    const float r = std::sqrt (x * x + y * y);
    const float ca = std::cos (ang), sa = std::sin (ang);
    const float u = x * ca + y * sa, v = -x * sa + y * ca;                 // v < 0 hacia el indicador
    const float H = 0.62f * Rc;
    float h = -1;
    if (r < Rc + 1)
    {
        s.m.alpha = clamp01 (Rc + 0.5f - r);
        h = H * (0.72f + 0.28f * fillet (Rc - r, 0.14f * Rc));
        const float Rf = 0.80f * Rc;
        if (r < Rf + 1)
        {
            const float k = clamp01 (Rf + 0.5f - r);
            const float hf = H * 0.90f - 0.02f * Rc * (1.0f - (r / Rf) * (r / Rf));
            h = h * (1.0f - k) + hf * k;
            Mat f = body; f.gloss = 0.34f; f.r *= 0.72f; f.g *= 0.72f; f.b *= 0.72f;
            s.m = mixM (s.m, f, k);
        }
    }
    const float av = std::abs (v), au = std::abs (u);
    if (av > 0.80f * Rc - 1.0f)
    {
        const float rr = 0.30f * hw, dx = au - (hw - rr), dy = av - (Rw - rr);
        const float d = rr - std::sqrt (std::max (dx, 0.0f) * std::max (dx, 0.0f) + std::max (dy, 0.0f) * std::max (dy, 0.0f)) - std::min (std::max (dx, dy), 0.0f);
        if (d > -1)
        {
            const float a = clamp01 (d + 0.5f);
            const float t = clamp01 ((av - Rc) / (Rw - Rc));
            const float hwg = H * (0.96f - 0.22f * t) * (0.50f + 0.50f * fillet (d, 0.35f * hw));
            if (hwg > h - 0.5f || r > Rc)
            {
                if (hwg > h) h = hwg;
                s.m = body;
                const bool pointerWing = v < 0;
                if (! pointerWing && av > Rc * 0.92f && au < 0.06f * hw + 0.6f) { h -= 0.03f * Rc; s.m.r *= 0.45f; s.m.g *= 0.45f; s.m.b *= 0.45f; }   // ranura
                const float lu = std::abs (u + 0.10f * hw);
                if (lineOnPointer == pointerWing && av > Rc * 0.90f && av < Rw - 0.25f * hw && lu < 0.085f * hw + 0.6f)
                    s.m = mixM (s.m, plastic (0.97f, 0.97f, 0.96f, 0.5f), clamp01 (0.085f * hw + 0.6f - lu));
            }
            s.m.alpha = std::max (s.m.alpha, a);
        }
    }
    s.h = std::max (h, 0.0f); return s;
}

// ---------------------------------------------------------------- perilla gris moleteada (cuerpo; la raya se dibuja aparte)
// banda moleteada clara (plastico traslucido, borde con muescas poco profundas) y tapa central plana mas oscura
inline Sample knurlKnob (float x, float y, float R, int teeth)
{
    Sample s; s.m = plastic (0.76f, 0.78f, 0.79f, 0.60f); s.m.alpha = 0;
    const float r = std::sqrt (x * x + y * y), th = std::atan2 (y, x);
    const float tooth = 0.5f + 0.5f * std::cos ((float) teeth * th);
    const float Redge = R * (0.962f + 0.038f * tooth);
    if (r > Redge + 1) { s.h = 0; return s; }
    s.m.alpha = clamp01 (Redge + 0.5f - r);
    const float H = 0.55f * R, Rc = 0.66f * R;
    float h = H * (0.82f - 0.12f * clamp01 ((r - Rc) / (R - Rc))) * (0.45f + 0.55f * fillet (Redge - r, 0.07f * R));
    h += 0.020f * R * tooth * clamp01 ((r - Rc) / (0.06f * R));            // estrias del moleteado
    { const float k = clamp01 ((r - Rc) / (0.05f * R)); const float g = 0.88f + 0.12f * tooth; s.m.r *= 1.0f - k * (1.0f - g); s.m.g *= 1.0f - k * (1.0f - g); s.m.b *= 1.0f - k * (1.0f - g); }
    const float lipW = 0.035f * R;
    const float lip = clamp01 (1.0f - std::abs (r - Rc) / lipW);           // aro claro alrededor de la tapa
    if (r < Rc + lipW)
    {
        const float k = clamp01 ((Rc + 0.5f - r) / 1.0f);
        const float hc = H * 0.84f - 0.008f * R * (1.0f - r / Rc);
        if (r < Rc) h = h * (1.0f - k) + hc * k;
        h += 0.012f * R * lip;
        Mat c = plastic (0.55f, 0.58f, 0.605f, 0.26f);
        s.m = mixM (s.m, c, k);
        s.m = mixM (s.m, plastic (0.88f, 0.90f, 0.92f, 0.6f), lip * 0.85f);
    }
    s.h = h; return s;
}

// ---------------------------------------------------------------- aro blanco del selector de frecuencia (cuerpo)
inline Sample whiteRing (float x, float y, float Ri, float Ro)
{
    Sample s; const float r = std::sqrt (x * x + y * y), th = std::atan2 (y, x);
    s.m = plastic (0.94f, 0.935f, 0.915f, 0.58f); s.m.er = s.m.eg = s.m.eb = 0.08f; s.m.alpha = clamp01 (Ro + 0.5f - r);
    if (r < Ri)                                                            // hueco negro alrededor de la perilla
    {
        s.m = plastic (0.02f, 0.02f, 0.025f, 0.3f); s.m.er = s.m.eg = s.m.eb = 0.0f;
        s.h = 0.04f * Ro * clamp01 ((r - Ri + 3.0f) / 3.0f); return s;
    }
    float h = 0.26f * Ro * fillet (Ro - r, 0.08f * Ro) * fillet (r - Ri, 0.05f * Ro) * (0.85f + 0.15f * (Ro - r) / (Ro - Ri));
    if (r > 0.93f * Ro)                                                    // moleteado fino del borde
    {
        const float k = clamp01 ((r - 0.93f * Ro) / (0.03f * Ro)), g = 0.5f + 0.5f * std::cos (120.0f * th);
        h += 0.008f * Ro * g * k;
        s.m.r *= 1.0f - 0.10f * k * (1.0f - g); s.m.g *= 1.0f - 0.10f * k * (1.0f - g); s.m.b *= 1.0f - 0.10f * k * (1.0f - g);
    }
    s.h = h; return s;
}

// ---------------------------------------------------------------- tornillo allen negro con arandela cromada
inline Sample socketScrew (float x, float y, float R)
{
    Sample s; const float r = std::sqrt (x * x + y * y);
    s.m = chrome (0.70f, 0.62f); s.m.alpha = clamp01 (R + 0.5f - r);
    float h = 0.16f * R * fillet (R - r, 0.25f * R);
    if (r < 0.66f * R)
    {
        h = 0.22f * R + 0.06f * R * std::sqrt (std::max (0.0f, 1.0f - (r / (0.66f * R)) * (r / (0.66f * R))));
        s.m = srgb (0.035f, 0.035f, 0.04f, 1, 0.55f);
        const float hexd = std::max ({ std::abs (x), std::abs (x * 0.5f + y * 0.8660254f), std::abs (x * 0.5f - y * 0.8660254f) });
        if (hexd < 0.26f * R) { h -= 0.14f * R; s.m = srgb (0.01f, 0.01f, 0.01f, 0, 0.2f); }
    }
    s.h = h; return s;
}
// ---------------------------------------------------------------- tornillo de mano moleteado (cromo satinado)
inline Sample thumbScrew (float x, float y, float R)
{
    Sample s; const float r = std::sqrt (x * x + y * y), th = std::atan2 (y, x);
    const float g = 0.5f + 0.5f * std::cos (36.0f * th), Re = R * (0.95f + 0.05f * g);
    s.m = chrome (0.55f, 0.86f); s.m.alpha = clamp01 (Re + 0.5f - r);
    s.h = 0.45f * R * std::sqrt (std::max (0.0f, 1.0f - (r / Re) * (r / Re))) * (0.6f + 0.4f * fillet (Re - r, 0.25f * R)) + 0.02f * R * g * clamp01 ((r - 0.8f * R) / (0.1f * R));
    return s;
}
// ---------------------------------------------------------------- tecla blanca (EQL / PHASE): rectangulo de esquinas redondeadas
inline Sample capKey (float x, float y, float hw, float hh, bool pressed)
{
    Sample s; s.m = plastic (0.97f, 0.965f, 0.955f, 0.45f); s.m.er = s.m.eg = s.m.eb = 0.14f;   // blanco luminoso como la foto
    const float rr = 3.0f, qx = std::abs (x) - (hw - rr), qy = std::abs (y) - (hh - rr);
    const float d = rr - (std::sqrt (std::max (qx, 0.0f) * std::max (qx, 0.0f) + std::max (qy, 0.0f) * std::max (qy, 0.0f)) + std::min (std::max (qx, qy), 0.0f));
    s.m.alpha = clamp01 (d + 0.5f);
    const float H = pressed ? 4.0f : 9.0f;
    s.h = H * fillet (d, 2.6f) + 0.6f * (1.0f - std::abs (y) / hh);
    if (pressed) { s.m.er = s.m.eg = s.m.eb = 0.09f; }
    return s;
}
} // namespace t73p
